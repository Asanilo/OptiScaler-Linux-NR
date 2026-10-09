#include "../OptiScaler/dlssnr/NrPreUpscale.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <vector>

namespace
{
struct Resource
{
    int id;
};

// Deliberately model NGX's separate typed and untyped slots. A permissive stub that casts either
// slot to the other would hide the actual regression in a native game's parameter block.
struct Parameters
{
    Resource* typed = nullptr;
    void* untyped = nullptr;
    bool typedSuccess = true;
    bool untypedSuccess = true;
    int typedWrites = 0;
    int untypedWrites = 0;

    int Get(const char*, Resource** out)
    {
        *out = typed;
        return typedSuccess ? 1 : -1;
    }
    int Get(const char*, void** out)
    {
        *out = untyped;
        return untypedSuccess ? 1 : -1;
    }
    void Set(const char*, Resource* value)
    {
        typed = value;
        ++typedWrites;
    }
    void Set(const char*, void* value)
    {
        untyped = value;
        ++untypedWrites;
    }
};

using Color = DlssNr::ScopedColorSubstitution<Resource, Parameters, 1>;
static_assert(!std::is_copy_constructible_v<Color>);
static_assert(!std::is_move_constructible_v<Color>);

int Evaluate(Parameters& params, Resource* edited, bool succeed)
{
    Color color(&params, "Color");
    Resource* original = color.Original();
    const bool replaced = color.Replace(edited);
    assert(replaced == (original != nullptr && edited != nullptr));
    if (replaced)
        assert(params.typed == edited || params.untyped == edited);
    return succeed ? 1 : -1; // destructor must run on both results
}

void ParameterRestoration()
{
    Resource original { 1 }, edited { 2 }, unrelatedSlot { 3 };
    for (bool succeed : { false, true })
    {
        Parameters typed;
        typed.typed = &original;
        typed.untyped = &unrelatedSlot;
        assert(Evaluate(typed, &edited, succeed) == (succeed ? 1 : -1));
        assert(typed.typed == &original && typed.untyped == &unrelatedSlot);
        assert(typed.typedWrites == 2 && typed.untypedWrites == 0);

        Parameters untyped;
        untyped.untyped = &original;
        assert(Evaluate(untyped, &edited, succeed) == (succeed ? 1 : -1));
        assert(untyped.typed == nullptr && untyped.untyped == &original);
        assert(untyped.typedWrites == 0 && untyped.untypedWrites == 2);
    }

    Parameters params;
    params.typed = &original;
    assert(Evaluate(params, nullptr, true) == 1); // NR creation/skip frame
    assert(params.typed == &original && params.typedWrites == 0);

    try
    {
        Color color(&params, "Color");
        assert(color.Replace(&edited));
        throw std::runtime_error("upscaler failed");
    }
    catch (const std::runtime_error&)
    {
        assert(params.typed == &original);
    }

    Parameters empty;
    assert(Evaluate(empty, &edited, true) == 1);
    assert(empty.typedWrites == 0 && empty.untypedWrites == 0);
    Color missing(nullptr, "Color");
    assert(missing.Original() == nullptr && !missing.Replace(&edited));

    // Failed Get must not trust the output pointer even if an implementation left one behind.
    Parameters failed;
    failed.typed = &unrelatedSlot;
    failed.typedSuccess = false;
    failed.untyped = &original;
    assert(Evaluate(failed, &edited, false) == -1);
    assert(failed.typed == &unrelatedSlot && failed.untyped == &original);
    assert(failed.typedWrites == 0 && failed.untypedWrites == 2);
    failed.untypedSuccess = false;
    const int writes = failed.untypedWrites;
    assert(Evaluate(failed, &edited, true) == 1);
    assert(failed.untypedWrites == writes);
}

void PlacementAcrossCreationFrames()
{
    using Mode = DlssNr::PlacementComparison;
    for (const auto mode : { Mode::Saved, Mode::Off, Mode::BeforeSr, Mode::AfterSr })
        assert(!DlssNr::ComparisonEnabled(false, mode)); // master toggle always wins
    assert(!DlssNr::ComparisonEnabled(true, Mode::Off));
    assert(DlssNr::ComparisonEnabled(true, Mode::BeforeSr));
    assert(DlssNr::ComparisonEnabled(true, Mode::AfterSr));
    assert(DlssNr::ComparisonBeforeSr(true, Mode::Saved));
    assert(!DlssNr::ComparisonBeforeSr(false, Mode::Saved));
    assert(DlssNr::ComparisonBeforeSr(false, Mode::BeforeSr));
    assert(!DlssNr::ComparisonBeforeSr(true, Mode::AfterSr));
    assert(DlssNr::UsePreUpscale(true, true, false, true));
    assert(!DlssNr::UsePreUpscale(false, true, false, true));
    assert(!DlssNr::UsePreUpscale(true, false, false, true));
    assert(!DlssNr::UsePreUpscale(true, true, true, true));   // dual pipeline owns NR
    assert(!DlssNr::UsePreUpscale(true, true, false, false)); // RR/FG excluded

    Resource original { 1 }, edited { 2 };
    Parameters params;
    params.typed = &original;
    int preCalls = 0, postCalls = 0, srCalls = 0;
    for (int frame = 0; frame < 5; ++frame)
    {
        const bool requested = DlssNr::UsePreUpscale(true, true, false, true);
        const auto result = DlssNr::EvaluateWithNr<Resource, 1>(
            &params, "Color", requested, true,
            [&]() -> Resource*
            {
                ++preCalls;
                // First frame creates NR, third frame skips: neither produced an edited resource.
                return frame == 0 || frame == 2 ? nullptr : &edited;
            },
            [&]()
            {
                ++srCalls;
                assert(params.typed == (frame == 0 || frame == 2 ? &original : &edited));
                return 1;
            },
            [&]() { ++postCalls; });
        assert(result == 1);
        assert(params.typed == &original);
    }
    assert(preCalls == 5 && srCalls == 5 && postCalls == 0);
}

void ProductionOrchestration()
{
    Resource original { 1 }, edited { 2 }, otherOriginal { 3 };
    for (bool typed : { false, true })
        for (bool pre : { false, true })
            for (bool allowPost : { false, true })
                for (int srResult : { -1, 1 })
                {
                    Parameters params;
                    if (typed)
                        params.typed = &original;
                    else
                        params.untyped = &original;
                    std::vector<std::string> events;
                    auto current = [&]() { return typed ? params.typed : static_cast<Resource*>(params.untyped); };
                    const int result = DlssNr::EvaluateWithNr<Resource, 1>(
                        &params, "Color", pre, allowPost,
                        [&]()
                        {
                            assert(current() == &original);
                            events.push_back("pre");
                            return &edited;
                        },
                        [&]()
                        {
                            assert(current() == (pre ? &edited : &original));
                            events.push_back("sr");
                            return srResult;
                        },
                        [&]()
                        {
                            assert(current() == &original);
                            events.push_back("post");
                        });
                    assert(result == srResult && current() == &original);
                    std::vector<std::string> expected =
                        pre ? std::vector<std::string> { "pre", "sr" } : std::vector<std::string> { "sr" };
                    if (!pre && allowPost && srResult == 1)
                        expected.push_back("post");
                    assert(events == expected);
                    if (!pre)
                        assert(params.typedWrites == 0 && params.untypedWrites == 0);
                }

    // A missing input skips pre NR but does not change the requested placement to post NR.
    Parameters missing;
    int before = 0, sr = 0, after = 0;
    DlssNr::EvaluateWithNr<Resource, 1>(
        &missing, "Color", true, true,
        [&]()
        {
            ++before;
            return &edited;
        },
        [&]()
        {
            ++sr;
            return 1;
        },
        [&]() { ++after; });
    assert(before == 0 && sr == 1 && after == 0);

    // Exceptions in either backend restore the input and do not invoke the post callback.
    for (bool failBefore : { false, true })
    {
        Parameters params;
        params.typed = &original;
        after = 0;
        try
        {
            DlssNr::EvaluateWithNr<Resource, 1>(
                &params, "Color", true, true,
                [&]() -> Resource*
                {
                    if (failBefore)
                        throw std::runtime_error("NR backend threw");
                    return &edited;
                },
                [&]() -> int { throw std::runtime_error("SR backend threw"); }, [&]() { ++after; });
            assert(false);
        }
        catch (const std::runtime_error&)
        {
            assert(params.typed == &original && after == 0);
        }
    }

    // Nested evaluations for independent parameter blocks cannot restore each other's Color.
    Parameters first, second;
    first.typed = &original;
    second.typed = &otherOriginal;
    DlssNr::EvaluateWithNr<Resource, 1>(
        &first, "Color", true, true, [&]() { return &edited; },
        [&]()
        {
            assert(first.typed == &edited && second.typed == &otherOriginal);
            DlssNr::EvaluateWithNr<Resource, 1>(
                &second, "Color", true, true, [&]() { return &edited; },
                [&]()
                {
                    assert(first.typed == &edited && second.typed == &edited);
                    return -1;
                },
                []() {});
            assert(first.typed == &edited && second.typed == &otherOriginal);
            return 1;
        },
        []() {});
    assert(first.typed == &original && second.typed == &otherOriginal);
}
} // namespace

int main()
{
    ParameterRestoration();
    PlacementAcrossCreationFrames();
    ProductionOrchestration();
    std::cout << "NR parameter, placement and production orchestration regressions passed\n";
}
