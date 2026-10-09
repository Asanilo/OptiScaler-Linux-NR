#include "../OptiScaler/dlssnr/EditCacheCadence.h"
#include <cassert>
#include <iostream>
#include <limits>

using Cadence = DlssNr::EditCacheCadence;

int main()
{
    for (const unsigned int interval : { 1u, 2u, 3u, 16u })
    {
        Cadence cadence;
        bool history = false;
        for (unsigned int i = 0; i < interval * 20u; ++i)
        {
            const auto decision = cadence.Begin(interval, false, 0.1f, history, false, false);
            assert(decision.Refresh() == (i % interval == 0));
            assert(decision.interval == interval && decision.crossfade > 0.0f && decision.crossfade <= 1.0f);
            if (decision.Refresh())
            {
                cadence.RefreshRecorded();
                history = true;
            }
            else
                cadence.CachedRecorded();
        }
        assert(cadence.refreshes == 20 && cadence.cached == 20 * (interval - 1));
    }
    Cadence retry;
    // Failure to record a refresh does not acknowledge it, or permit a cached result.
    for (int i = 0; i < 3; ++i)
        assert(retry.Begin(2, false, 0.1f, false, false, false).Refresh());
    assert(retry.refreshes == 0 && retry.cached == 0);
    retry.RefreshRecorded();
    assert(!retry.Begin(2, false, 0.1f, true, false, false).Refresh());
    assert(retry.Begin(2, false, 0.1f, true, true, false).Refresh());
    retry.RefreshRecorded();
    assert(retry.Begin(16, false, 0.1f, true, false, true).Refresh());
    retry.RefreshRecorded();
    assert(retry.Begin(16, false, 0.1f, false, false, false).Refresh());
    Cadence adaptive;
    adaptive.RefreshRecorded();
    for (int i = 0; i < 29; ++i)
        adaptive.Begin(3, true, 0.1f, true, false, false);
    assert(adaptive.regime == 1);
    assert(adaptive.Begin(3, true, 0.1f, true, false, false).interval == 6);
    assert(adaptive.regime == 0);
    for (int i = 0; i < 3; ++i)
    {
        adaptive.ObserveRejected(1.0f);
        adaptive.Begin(3, true, 0.1f, true, false, false);
        assert(adaptive.regime == 0);
    }
    adaptive.ObserveRejected(1.0f);
    assert(adaptive.Begin(3, true, 0.1f, true, false, false).interval == 2);
    assert(adaptive.regime == 2);
    adaptive.ResetMotion();
    assert(adaptive.regime == 1);
    adaptive.ObserveRejected(std::numeric_limits<float>::quiet_NaN());
    assert(adaptive.Begin(0, false, std::numeric_limits<float>::quiet_NaN(), false, false, false).interval == 1);
    assert(adaptive.Begin(999, false, -1.0f, false, false, false).interval == 16);
    std::cout << "PASS: NR input cadence 1/2/3/16, failed refresh retry, reset/measurement, adaptive hysteresis and "
                 "finite bounds\n";
}
