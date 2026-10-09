#pragma once
#include <cstdint>
#include <cstddef>

// Shared by production and the real-GPU shader fixture. Match nr_stabilize.hlsl.
struct alignas(256) NrStabilizerConstants
{
    uint32_t Width, Height, DepthWidth, DepthHeight;
    uint32_t MotionWidth, MotionHeight, DepthInverted, HistoryValid;
    float MvScaleX, MvScaleY, JitterDeltaX, JitterDeltaY;
    float Epsilon, StepLimit, DepthTolerance, ColourTolerance;
    uint32_t Despeckle;
    float Pad0, Pad1, Pad2;
};
static_assert(sizeof(NrStabilizerConstants) == 256);
static_assert(offsetof(NrStabilizerConstants, Epsilon) == 48);
static_assert(offsetof(NrStabilizerConstants, Despeckle) == 64);
