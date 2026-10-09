#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace DlssNr
{
struct GpuTimingStatus
{
    std::size_t frames = 0, samples = 0, refreshSamples = 0, cachedSamples = 0, pairedSamples = 0;
    std::optional<double> mean, refreshMean, cachedMean, pairedTotalMean, modelMean, otherMean;
};

// A window of actual NR input frames, not presents or a theoretical cache ratio.
// Late queries join by frame identity. Missing queries remain visible in coverage.
class GpuTimingWindow
{
    struct Frame
    {
        std::uint64_t id = 0;
        bool cached = false;
        std::optional<double> total, model;
    };
    static constexpr std::size_t kFrames = 120;
    std::array<Frame, kFrames> _frames {};
    std::size_t _next = 0, _count = 0;

    void Add(std::uint64_t id, double ms, bool model)
    {
        if (!std::isfinite(ms) || ms < 0.0)
            return;
        for (std::size_t i = 0; i < _count; ++i)
            if (_frames[i].id == id)
            {
                auto& slot = model ? _frames[i].model : _frames[i].total;
                if (!slot) // A completed query is consumed once, including on replay.
                    slot = ms;
                return;
            }
    }

  public:
    void Reset() { *this = GpuTimingWindow {}; }
    void Record(std::uint64_t id, bool cached)
    {
        _frames[_next] = { id, cached, {}, {} };
        _next = (_next + 1) % kFrames;
        if (_count < kFrames)
            ++_count;
    }
    void AddTotal(std::uint64_t id, double ms) { Add(id, ms, false); }
    void AddModel(std::uint64_t id, double ms) { Add(id, ms, true); }
    GpuTimingStatus Get() const
    {
        GpuTimingStatus out {};
        out.frames = _count;
        std::size_t refreshFrames = 0, cachedFrames = 0;
        double total = 0, refresh = 0, cached = 0, paired = 0, model = 0;
        for (std::size_t i = 0; i < _count; ++i)
        {
            const auto& frame = _frames[i];
            (frame.cached ? cachedFrames : refreshFrames)++;
            if (!frame.total)
                continue;
            ++out.samples;
            total += *frame.total;
            if (frame.cached)
            {
                ++out.cachedSamples;
                cached += *frame.total;
            }
            else
            {
                ++out.refreshSamples;
                refresh += *frame.total;
            }
            // Subtraction is valid only for timestamps bracketing the same frame.
            if (!frame.cached && frame.model && *frame.model <= *frame.total)
            {
                ++out.pairedSamples;
                paired += *frame.total;
                model += *frame.model;
            }
        }
        if (out.refreshSamples)
            out.refreshMean = refresh / out.refreshSamples;
        if (out.cachedSamples)
            out.cachedMean = cached / out.cachedSamples;
        // Never present a cache-only sample population as the combined cost.
        if (out.samples && (!refreshFrames || out.refreshSamples) && (!cachedFrames || out.cachedSamples))
            out.mean = total / out.samples;
        if (out.pairedSamples)
        {
            out.pairedTotalMean = paired / out.pairedSamples;
            out.modelMean = model / out.pairedSamples;
            out.otherMean = (paired - model) / out.pairedSamples;
        }
        return out;
    }
};
} // namespace DlssNr
