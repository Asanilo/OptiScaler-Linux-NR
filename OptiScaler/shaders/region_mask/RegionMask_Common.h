#pragma once

// Region mask: give part of the frame back as the game drew it, chosen by depth.
// The model still sees and edits the whole frame; this only decides what of its
// edit reaches the screen. Mode 1 keeps the near foreground (the character) as
// the game drew it; mode 2 keeps the far scenery instead.
//
// The constants mirror region_mask.hlsl's Params cbuffer, one ordered run of
// 4-byte scalars. Append only, in both places.

#include <cstdint>

struct alignas(256) RegionMaskConstants
{
    uint32_t Width;
    uint32_t Height;
    uint32_t DepthWidth;
    uint32_t DepthHeight;
    uint32_t DepthInverted;
    uint32_t Mode; // 1 near, 2 far
    uint32_t IgnoreFloor;
    uint32_t Show;
    float Distance;
    float Softness;
    float Strength;
    float Pad;
};

static_assert(sizeof(RegionMaskConstants) == 256);
