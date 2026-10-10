#include "../OptiScaler/dlssnr/MotionJitter.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
int main()
{
    DlssNr::MotionJitterHistory h;
    auto frame = [&](uint64_t id, bool pre, bool jittered, float x, float y, bool reset = false, float sx = -640,
                     float sy = 360, unsigned int w = 1280, unsigned int ht = 720)
    { return h.Advance(id, pre, jittered, true, x, y, sx, sy, w, ht, reset); };
    auto first = frame(1, false, true, 0.21875f, 0.179012358f);
    assert(first.reset && !first.apply);
    auto second = frame(2, false, true, -0.03125f, -0.376543224f);
    assert(second.apply && !second.reset);
    // Recorded half-precision game vectors from a static lamp, with signed,
    // anisotropic scales. Quantization prevents an exactly-zero residual.
    assert(std::abs((-0.000389576f + second.x) * -640) < 0.001f);
    assert(std::abs((0.001543045f + second.y) * 360) < 0.001f);
    // Preserve arbitrary real object/camera motion after removing only jitter.
    assert(std::abs(((3.0f + 0.25f) / -640 + second.x) * -640 - 3) < 1e-5f);
    assert(std::abs(((-7.0f + 0.555555582f) / 360 + second.y) * 360 + 7) < 1e-5f);
    assert(frame(4, false, true, 0.2f, 0.3f).reset); // Missing input, not cache cadence.
    assert(frame(5, false, true, 0.1f, 0.2f).apply);
    auto cut = frame(6, false, true, 0.4f, 0.2f, true);
    assert(cut.reset && !cut.apply);
    assert(frame(7, false, true, 0.1f, 0.2f).apply);
    assert(frame(8, true, true, 0.1f, 0.2f).reset);
    auto pre = frame(9, true, true, 0.2f, 0.3f);
    assert(!pre.reset && !pre.apply);
    assert(frame(10, false, false, 0.1f, 0.2f).reset);
    auto plain = frame(11, false, false, 0.3f, 0.4f);
    assert(!plain.reset && !plain.apply);
    assert(frame(12, false, true, 0.1f, 0.2f).reset);
    assert(frame(13, false, true, 0.1f, 0.2f, false, 0).reset);
    assert(frame(14, false, true, 0.1f, 0.2f).reset);
    assert(frame(15, false, true, std::numeric_limits<float>::quiet_NaN(), 0).reset);
    assert(frame(16, false, true, 0.1f, 0.2f).reset);
    assert(frame(17, false, true, 0.2f, 0.3f, false, -640, 360, 1920, 1080).reset);
    assert(frame(18, false, true, 0.2f, 0.3f, false, -960, 540, 1920, 1080).reset);
    h.Invalidate();
    assert(frame(19, false, true, 0.1f, 0.2f).reset);
    assert(frame(20, false, true, 0.2f, 0.3f).apply);
    DlssNr::MotionJitterHistory drs;
    assert(drs.Advance(1, false, true, true, 0, 0, -640, 360, 1920, 1080, false, 1280, 720).reset);
    assert(drs.Advance(2, false, true, true, 0.1f, 0.2f, -640, 360, 1920, 1080, false, 1280, 720).apply);
    assert(drs.Advance(3, false, true, true, 0.2f, 0.3f, -640, 360, 1920, 1080, false, 960, 540).reset);
    assert(drs.Advance(4, false, true, false, 0, 0, -640, 360, 1920, 1080, false, 960, 540).reset);
    std::puts("PASS: observed jitter cancellation, real motion, continuity, placement, flags, resets, invalid inputs");
}
