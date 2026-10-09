// Minimal every-frame stabiliser derived from Skynizz/optiscaler-dlss5
// 591fd305: log-ratio edits, depth/colour validated reprojection and step limit.
// No cache skipping, pyramid propagation, crossfade or additional temporal mix.
cbuffer Params : register(b0)
{
    uint Width, Height, DepthWidth, DepthHeight;
    uint MotionWidth, MotionHeight, DepthInverted, HistoryValid;
    float MvScaleX, MvScaleY, JitterDeltaX, JitterDeltaY;
    float Epsilon, StepLimit, DepthTolerance, ColourTolerance;
    uint Despeckle; float Pad0, Pad1, Pad2;
};
Texture2D<float4> Fresh : register(t0);
Texture2D<float4> Original : register(t1);
Texture2D<float> Depth : register(t2);
Texture2D<float2> Motion : register(t3);
Texture2D<float4> PreviousEdit : register(t4);
Texture2D<float2> PreviousGuide : register(t5);
RWTexture2D<float4> Target : register(u0);
RWTexture2D<float4> NextEdit : register(u1);
RWTexture2D<float2> NextGuide : register(u2);

static const float3 Luma = float3(0.2126, 0.7152, 0.0722);
float LinearDepth(float d)
{ return DepthInverted != 0 ? 1.0 / max(d, 1e-7) : 1.0 / max(1.0 - d, 1e-7); }
float3 EditAt(int2 p)
{
    return clamp(log2((max(Fresh.Load(int3(p, 0)).rgb, 0) + Epsilon) /
                      (max(Original.Load(int3(p, 0)).rgb, 0) + Epsilon)), -4, 4);
}
float2 MotionOffset(float2 uv)
{
    int2 center = clamp(int2(uv * float2(DepthWidth, DepthHeight)), 0, int2(DepthWidth, DepthHeight) - 1);
    int2 nearest = center;
    float best = 3.4e38;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            int2 p = clamp(center + int2(x, y), 0, int2(DepthWidth, DepthHeight) - 1);
            float z = LinearDepth(Depth.Load(int3(p, 0)));
            if (z < best) { best = z; nearest = p; }
        }
    float2 guideUv = (float2(nearest) + 0.5) / float2(DepthWidth, DepthHeight);
    int2 m = clamp(int2(guideUv * float2(MotionWidth, MotionHeight)), 0, int2(MotionWidth, MotionHeight) - 1);
    return Motion.Load(int3(m, 0)) * float2(MvScaleX, MvScaleY) / float2(MotionWidth, MotionHeight) +
           float2(JitterDeltaX, JitterDeltaY);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= Width || id.y >= Height) return;
    int2 p = int2(id.xy);
    float2 uv = (float2(p) + 0.5) / float2(Width, Height);
    int2 d = clamp(int2(uv * float2(DepthWidth, DepthHeight)), 0, int2(DepthWidth, DepthHeight) - 1);
    float depth = LinearDepth(Depth.Load(int3(d, 0)));
    float4 raw = Fresh.Load(int3(p, 0));
    float3 original = max(Original.Load(int3(p, 0)).rgb, 0);
    float logLuma = log2(dot(original, Luma) + Epsilon);
    float3 edit = EditAt(p);
    float3 carried = 0;
    float previousLuma = 0;
    float trust = 0;
    float2 q = uv + MotionOffset(uv);
    if (HistoryValid != 0 && all(q >= 0) && all(q <= 1))
    {
        float2 pos = q * float2(Width, Height) - 0.5;
        int2 i0 = int2(floor(pos));
        float2 f = frac(pos);
        [unroll] for (int k = 0; k < 4; ++k)
        {
            int2 o = int2(k & 1, k >> 1);
            int2 t = clamp(i0 + o, 0, int2(Width, Height) - 1);
            float wb = (o.x ? f.x : 1 - f.x) * (o.y ? f.y : 1 - f.y);
            float2 guide = PreviousGuide.Load(int3(t, 0));
            float4 history = PreviousEdit.Load(int3(t, 0));
            float rel = abs(guide.x - depth) / max(min(guide.x, depth), 1e-7);
            float w = wb * saturate((2 * DepthTolerance - rel) / max(DepthTolerance, 1e-6)) * history.a;
            carried += history.rgb * w;
            previousLuma += guide.y * w;
            trust += w;
        }
        if (trust > 1e-4)
        {
            carried /= trust;
            previousLuma /= trust;
            trust *= saturate(1 - abs(previousLuma - logLuma) / max(ColourTolerance, 1e-6));
        }
    }
    // Invalid/disoccluded history must show this frame exactly, including alpha.
    // The history stores a bounded edit, but is never substituted on those pixels.
    if (trust >= 0.5 && StepLimit > 0)
    {
        if (Despeckle != 0)
        {
            float lo = 1e9, hi = -1e9;
            [unroll] for (int k = 0; k < 9; ++k)
                if (k != 4)
                {
                    int2 n = clamp(p + int2(k % 3 - 1, k / 3 - 1), 0, int2(Width, Height) - 1);
                    float e = dot(EditAt(n), Luma); lo = min(lo, e); hi = max(hi, e);
                }
            float l = dot(edit, Luma); edit += clamp(l, lo - 0.1, hi + 0.1) - l;
        }
        float freshLuma = dot(edit, Luma);
        float carriedLuma = dot(carried, Luma);
        edit += carriedLuma + clamp(freshLuma - carriedLuma, -StepLimit, StepLimit) - freshLuma;
        raw.rgb = max((original + Epsilon) * exp2(edit) - Epsilon, 0);
    }
    Target[id.xy] = raw;
    NextEdit[id.xy] = float4(edit, 1);
    NextGuide[id.xy] = float2(depth, logLuma);
}
