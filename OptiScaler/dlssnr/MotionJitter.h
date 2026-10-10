#pragma once
#include <cmath>
#include <cstdint>

namespace DlssNr
{
struct MotionJitterCorrection
{
    bool apply = false;
    bool reset = false;
    float x = 0, y = 0; // In the game's stored vector units, before MVecScale.
};

// Game-input history, independent of model/cache refresh cadence. A missing NR
// dispatch breaks continuity; never subtract a jitter from two input frames ago.
class MotionJitterHistory
{
    bool valid_ = false, pre_ = false, jittered_ = false;
    uint64_t serial_ = 0;
    unsigned int width_ = 0, height_ = 0, renderWidth_ = 0, renderHeight_ = 0;
    float x_ = 0, y_ = 0, scaleX_ = 0, scaleY_ = 0;

  public:
    void Invalidate() { valid_ = false; }
    MotionJitterCorrection Advance(uint64_t serial, bool pre, bool jittered, bool jitterValid, float x, float y,
                                   float scaleX, float scaleY, unsigned int width, unsigned int height, bool reset,
                                   unsigned int renderWidth = 0, unsigned int renderHeight = 0)
    {
        const bool usable = jitterValid && std::isfinite(x) && std::isfinite(y) && std::isfinite(scaleX) &&
                            std::isfinite(scaleY) && std::abs(scaleX) > 1e-8f && std::abs(scaleY) > 1e-8f && width &&
                            height;
        const bool continuous = valid_ && serial == serial_ + 1 && pre == pre_ && jittered == jittered_ &&
                                width == width_ && height == height_ && renderWidth == renderWidth_ &&
                                renderHeight == renderHeight_ && scaleX == scaleX_ && scaleY == scaleY_;
        MotionJitterCorrection result;
        result.reset = reset || !continuous || (jittered && !usable);
        // Jittered MV = scene motion + previous jitter - current jitter.
        // Post-SR colour no longer contains that jitter. Pre-SR still does.
        result.apply = !pre && jittered && usable && continuous && !reset;
        if (result.apply)
        {
            result.x = (x - x_) / scaleX;
            result.y = (y - y_) / scaleY;
            if (!std::isfinite(result.x) || !std::isfinite(result.y))
            {
                result = {};
                result.reset = true;
            }
        }
        valid_ = !jittered || usable;
        serial_ = serial;
        pre_ = pre;
        jittered_ = jittered;
        x_ = x;
        y_ = y;
        scaleX_ = scaleX;
        scaleY_ = scaleY;
        width_ = width;
        height_ = height;
        renderWidth_ = renderWidth;
        renderHeight_ = renderHeight;
        return result;
    }
};
} // namespace DlssNr
