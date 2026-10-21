// Region mask: the edited frame, the game's untouched frame and depth in;
// a frame where part of the edit is given back to the game's own pixels out.
//
// Near is view depth over the near plane, read the same way the edit cache
// reads it (reversed-Z stores near/z). With the ground left out, near does not
// count on the ground: a standing character's depth barely changes up the
// screen, while the ground is a plane whose depth moves the same way on both
// sides of a pixel.

cbuffer Params : register(b0)
{
    uint gWidth;
    uint gHeight;
    uint gDepthWidth;
    uint gDepthHeight;
    uint gDepthInverted;
    uint gMode;        // 1 near gets the game's frame back, 2 far does
    uint gIgnoreFloor;
    uint gShow;
    float gDistance;
    float gSoftness;
    float gStrength;
    float gPad;
};

Texture2D<float4> gEdited : register(t0);
Texture2D<float4> gOriginal : register(t1);
Texture2D<float> gDepth : register(t2);
RWTexture2D<float4> gOut : register(u0);

float MaskLogDepth(int2 t)
{
    const int2 hi = int2((int) gDepthWidth, (int) gDepthHeight) - 1;
    const float d = gDepth.Load(int3(clamp(t, int2(0, 0), hi), 0)).r;
    // Proportional to view depth either way round.
    return log2(gDepthInverted != 0 ? 1.0 / max(d, 1e-7) : 1.0 / max(1.0 - d, 1e-7));
}

float MaskNear(int2 t)
{
    const float l = MaskLogDepth(t);
    float nearW = 1.0 - smoothstep(-gSoftness, gSoftness, l - log2(max(gDistance, 1e-3)));

    if (gIgnoreFloor != 0 && nearW > 0.0)
    {
        const int k = max(1, (int) gDepthHeight / 360);
        const float up = MaskLogDepth(t - int2(0, k)) - l;
        const float down = l - MaskLogDepth(t + int2(0, k));
        const float plane = up * down > 0.0 ? min(abs(up), abs(down)) : 0.0;
        const float slope = plane / (float) k * ((float) gDepthHeight / 100.0);
        nearW *= 1.0 - smoothstep(0.006, 0.012, slope);
    }

    return nearW;
}

[numthreads(8, 8, 1)] void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= gWidth || id.y >= gHeight)
        return;

    const float2 uv = (float2(id.xy) + 0.5) / float2(gWidth, gHeight);
    float region = 0.0;

    if (gMode == 1 || gMode == 2)
    {
        const float2 p = uv * float2(gDepthWidth, gDepthHeight) - 0.5;
        const int2 t0 = int2(floor(p));
        const float2 f = p - floor(p);
        const float n = lerp(lerp(MaskNear(t0), MaskNear(t0 + int2(1, 0)), f.x),
                             lerp(MaskNear(t0 + int2(0, 1)), MaskNear(t0 + int2(1, 1)), f.x), f.y);
        region = saturate((gMode == 1 ? n : 1.0 - n) * gStrength);
    }

    const float4 edited = gEdited.Load(int3(id.xy, 0));
    const float4 original = gOriginal.Load(int3(id.xy, 0));
    float3 rgb = lerp(edited.rgb, original.rgb, region);

    // A multiply, so the tint reads the same on a linear or an encoded frame.
    if (gShow != 0)
        rgb *= lerp(float3(1.0, 1.0, 1.0), float3(1.0, 0.35, 1.0), region);

    gOut[id.xy] = float4(rgb, edited.a);
}
