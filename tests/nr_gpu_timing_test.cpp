#include "../OptiScaler/dlssnr/GpuTimingWindow.h"
#include <iostream>
#include <limits>
#include <stdexcept>

void Require(bool ok, const char* why)
{
    if (!ok)
        throw std::runtime_error(why);
}
void Near(std::optional<double> value, double expected, const char* why)
{
    Require(value && std::abs(*value - expected) < 1e-9, why);
}
int main()
{
    try
    {
        DlssNr::GpuTimingWindow window;
        for (std::uint64_t frame = 1; frame <= 6; ++frame)
            window.Record(frame, frame % 3 != 1);
        // Cached queries finish first: do not display their cost as the whole mean.
        for (auto frame : { 2, 3, 5, 6 })
            window.AddTotal(frame, 1.0);
        Require(!window.Get().mean && window.Get().samples == 4, "missing refresh phase is visible");
        window.AddModel(4, 6.0); // Out-of-order arrival joins the matching total below.
        window.AddTotal(1, 10.0);
        window.AddTotal(4, 8.0);
        window.AddModel(1, 7.0);
        auto status = window.Get();
        Near(status.mean, 22.0 / 6, "mean uses actual 2 refresh / 4 cache samples");
        Near(status.refreshMean, 9, "refresh mean");
        Near(status.cachedMean, 1, "cached mean");
        Near(status.modelMean, 6.5, "matched model mean");
        Near(status.otherMean, 2.5, "matched overhead");
        Require(status.frames == 6 && status.samples == 6 && status.pairedSamples == 2, "coverage");
        window.AddTotal(1, 900);  // Re-read/replay must not count a sample twice.
        window.AddTotal(99, 900); // No recorded frame, no fabricated timing.
        Near(window.Get().mean, 22.0 / 6, "duplicates and unknown frames ignored");
        window.Reset();
        window.Record(7, false);
        window.AddTotal(4, 8); // Old cached/refresh queries must not survive a toggle.
        Require(!window.Get().mean, "reset rejects late old queries");
        window.AddTotal(7, 8);
        window.AddModel(7, 9);
        Require(window.Get().pairedSamples == 0 && !window.Get().otherMean,
                "invalid pairing cannot yield negative cost");
        for (std::uint64_t frame = 8; frame <= 130; ++frame)
        {
            window.Record(frame, false);
            window.AddTotal(frame, frame % 2 ? 2 : 4);
        }
        window.AddTotal(130, std::numeric_limits<double>::infinity());
        window.AddModel(130, -1);
        status = window.Get();
        Require(status.frames == 120 && status.samples == 120, "bounded rolling window");
        Near(status.mean, 3, "window eviction");
        window.Reset();
        window.Record(131, false);
        window.AddTotal(131, std::numeric_limits<double>::quiet_NaN());
        window.AddTotal(131, -1);
        Require(window.Get().samples == 0, "invalid samples excluded");
        window.AddTotal(131, 0); // A zero timestamp delta is valid; do not clamp to a desired result.
        Near(window.Get().mean, 0, "legitimate zero cost retained");
        std::cout
            << "PASS: weighted GPU samples, phase coverage, exact frame pairing, reset, eviction and invalid queries\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
