#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace DlssNr
{
// Scheduling counts actual NR input frames, never swapchain presents or GPU
// completion. GPU readbacks and resource ownership are separately fenced.
class EditCacheCadence
{
  public:
    enum class Reason
    {
        Cached,
        NoHistory,
        Reset,
        Measurement,
        Interval
    };
    struct Decision
    {
        Reason reason;
        unsigned int interval;
        float crossfade;
        bool Refresh() const { return reason != Reason::Cached; }
    };

    Decision Begin(unsigned int interval, bool adaptive, float threshold, bool history, bool reset, bool measurement)
    {
        ++frame;
        interval = std::clamp(interval, 1u, 16u);
        threshold = std::isfinite(threshold) ? std::clamp(threshold, 0.001f, 1.0f) : 0.1f;
        effectiveInterval = ChooseInterval(interval, adaptive, threshold);
        Reason reason = !history                                   ? Reason::NoHistory
                        : reset                                    ? Reason::Reset
                        : measurement                              ? Reason::Measurement
                        : frame - lastRefresh >= effectiveInterval ? Reason::Interval
                                                                   : Reason::Cached;
        const auto since = frame - lastRefresh;
        const float step = reason == Reason::Cached
                               ? (since >= effectiveInterval ? 1.0f : 1.0f / float(effectiveInterval - since))
                               : 1.0f / float(effectiveInterval);
        return { reason, effectiveInterval, step };
    }

    // Called only after the output/history dispatches were successfully recorded.
    void RefreshRecorded()
    {
        lastRefresh = frame;
        ++refreshes;
    }
    void CachedRecorded() { ++cached; }
    void ObserveRejected(float fraction)
    {
        if (std::isfinite(fraction))
            motion = motion * 0.75f + std::clamp(fraction, 0.0f, 1.0f) * 0.25f;
    }
    void ResetMotion()
    {
        motion = 0.0f;
        regime = candidate = 1;
        regimeFrames = 0;
    }

    uint64_t frame = 0, lastRefresh = 0, refreshes = 0, cached = 0;
    unsigned int effectiveInterval = 1;
    int regime = 1;

  private:
    float motion = 0.0f;
    int candidate = 1;
    unsigned int regimeFrames = 0;
    unsigned int ChooseInterval(unsigned int interval, bool adaptive, float threshold)
    {
        if (!adaptive || interval <= 1)
        {
            regime = candidate = 1;
            regimeFrames = 0;
            return interval;
        }
        const int wanted = motion > threshold ? 2 : motion < std::max(0.015f, threshold * 0.15f) ? 0 : 1;
        if (wanted == regime)
        {
            candidate = wanted;
            regimeFrames = 0;
        }
        else
        {
            if (wanted != candidate)
            {
                candidate = wanted;
                regimeFrames = 0;
            }
            if (++regimeFrames >= (wanted == 2 ? 4u : 30u))
            {
                regime = wanted;
                regimeFrames = 0;
            }
        }
        return regime == 0 ? std::min(interval * 2u, 8u) : regime == 2 ? std::max(1u, (interval + 1u) / 2u) : interval;
    }
};
} // namespace DlssNr
