#include "../OptiScaler/dlssnr/NrPreUpscale.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <type_traits>

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
        {
            Color color(&params, "Color");
            if (requested && color.Original() != nullptr)
            {
                ++preCalls;
                // First frame creates NR, third frame skips: neither produced an edited resource.
                color.Replace(frame == 0 || frame == 2 ? nullptr : &edited);
            }
            ++srCalls;
            assert(params.typed == (frame == 0 || frame == 2 ? &original : &edited));
        }
        assert(params.typed == &original);
        if (!requested)
            ++postCalls;
    }
    assert(preCalls == 5 && srCalls == 5 && postCalls == 0);
}
} // namespace

int main()
{
    ParameterRestoration();
    PlacementAcrossCreationFrames();
    std::cout << "NR pre-upscale parameter and placement regressions passed\n";
}
