# Frozen r2 Sky shader interface for Native (source-backed, not new wire)

r2 commit0b871602; shader SPIR-V SHAe99efc568377f2a59563609e68705fcc038789b1fd7e70522981437a89c08f92; owned adapter40e25f532a5b3ef02d633dfb858fdb21ba3d94f6238db50e96050d918ff7f772. CSMain compute8x8. Original cache HLSL source SHA0efdd31f1976435b0b956b94814c96a70e68310b838f916aecc289375af02b4f. r2 GPU five cases passed per Root, not real guide phase/quality certification.

| Binding | Vulkan descriptor | HLSL role |
|---|---|---|
|0|uniform buffer,range256|Original DlssNrCacheConstants scalar bytes0..155; gWholeValid uint at156 in padding. No push constants.|
|1,2|sampled images|t0 HistEdit RGBA32F109 (RGB logratio,a highconfidence); t1 HistGuide RGBA32F109 (xy pseudo-linearDepth/logLuma)|
|3,4,5|sampled images|t2 unedited current/origin Colour RGBA32F109; t3 Depth R32F100; t4 Motion RG32F103, independent actual guide extents|
|6,7,8,9|sampled images|t5 Aux0: Capture saved NR[origin], Mode3 previous pyramid, ApplyL1; t6 Aux1 ApplyL1guide; t7/t8 ApplyL2/L3|
|10|sampled stand-in|t9 unused gap; layout still binds a valid dummy|
|11,12|sampled images|t10 exposure (UseGameExposure0/manualWP1); t11 HistTarget(crossfade off,valid dummy)|
|13..17|storage images Rgba32f|u0 edit/level/composite; u1 guide; u2 untouched Color copy in Reproject; u3 L1; u4 L1 guide|
|18|storage image R32ui|u5 reject stats,4x1,StatsSlot0; image atomic capability applies|
|19|storage image Rgba32f|u6 optional crossfade target,off/valid dummy|
|20|separate sampler|linear clamp-to-edge,nearest mip,maxLod0|

Capture2/Reproject1: Width/Height are full color dimensions; Depth/Motion dimensions independently real. Dispatch ceil(W/8),ceil(H/8). Writes edit,guide and quarter L1+L1guide. Capture inputs ORIGIN Colour+Depth+NR, HistoryValid0 for fresh anchor. Reproject inputs that link's unmodified Colour+Depth+MV and previous edit/guide,HistoryValid1. Then Mode3 twice: destination dimensions in Width/Height, previous dimensions in SourceWidth/Height, output L2 thenL3. Each level is ceil(previous/4). Apply4 only after complete valid catchup: current raw Colour+current history/guide,L1,L1guide,L2,L3 -> independent current composite. gWholeValid0 copies current Colour exactly, including negatives/signedzero/alpha. Low pixel confidence is NOT whole invalidity. Exact zero edit identity guard preserves original bytes. r2 FreshEdit guard checks finite RAW RGB bit equality before max/ratio/log2, preserving genuine unequal input path.

Required image support: sampled+storage+transfer formats, RGBA32F sampled linear; storage shader formats explicitly Rgba32f/R32ui, Shader capability only, no speculative read/write-without-format feature. Context min limits:7 storage images,12 sampled images,UBO range>=256. Frame-image descriptors use GENERAL in fixture; actual barriers from producer/consumer writes to shader reads/storage writes and across stages are mandatory. Fixture's broad barriers and actual10s fence are serialization proof, NOT permission to wait/reset pool on Native renderer. Native must retain actual per-dispatch UBO/descriptor/composite/backing until source/cache/merge last-use ticks; cannot reuse fixture's4 CB slots while in flight.

Default fixed constants: manualWP1=>Epsilon1/512,DepthTol.1,ColourTol.5,HighDecay.97,RefreshBlend/LowGain/HighGain1,Bilateral1,MaxLumaEdit1.5; optionalcrossfade/JBU/adaptive/temporal/stabilize/despeckle/debug off. NR modelReset1 each sparse refresh is separate from edit history.

Original guide formula (cache HLSL207/222): c=clamp(int(uv*DepthExtent)); select nearest LinDepth in3x3; bestUV=(best+.5)/DepthExtent; MotionTexel=clamp(int(bestUV*MotionExtent)); offsetUV=storedMV.xy*cacheMvScale/MotionExtent + JitterDeltaUV. Stored game MV is previous-minus-current RENDER pixels; cache scale1 is this source-backed candidate normalization. This is not proof of NR Evaluate MVecScale semantics nor720guide/1080color phase. Post-SR JitterDelta0 is phase_correction=none candidate, not actual game jitter0/k_output identified. Do not resample/reduce jitter twice or replace missing links with source_gap.

Production boundary: Native every-frame Capture/Reproject/Pyramid/Apply against CURRENT SR; Helper sparse NR only. This table describes shader/resource calls, not a second protocol or Helper cache service.
