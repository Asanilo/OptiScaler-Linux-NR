# Serial MultiPass candidate (not GPU accepted)

Base: daily Helper 713f8ac. Reference: OptiScaler `DlssNr_Dx12.cpp` chain at
3290..3377 and final Resolve at 3507. This ports the topology into the existing
Wine Helper / Vulkan NGX forwarder; it does not change the forwarder ABI or
claim a D3D12 backend was added.

One pass remains the default. A paired Native candidate permits passes=2 via
its explicit Apply/drain/rebuild protocol. Both features use the same model,
preset, Style and Intensity, independent capability parameter blocks, identical
1280x720 guides and separate vendor histories. Feature creation has its own
submitted command buffer and observed GPU fence before any evaluation.

HDR -> frozen Encode once (preserved proxy/keep) -> Feature1/raw0 ->
Feature2/raw1 -> frozen Resolve once (initial proxy, final raw1, original keep).
raw0 transitions to read-only before pass2; its NGX input wrapper is read-only.
Private raw1 is allocated only for two passes and retained until safe teardown.
Descriptors reset only after prior actual fence + Native CONSUMED; Encode and
Resolve have distinct mapped constants, each written once per command buffer.

Only adjacent source frames in the same epoch/history/params/pass mode retain
model history. Sparse gaps, cuts, rebuilds and mode changes reset both histories.
Single pass continues to Reset=1. This is not an assertion that sparse play has
continuous model history. Two-pass completion includes actual resource binding
identities; the Native owner validates raw0 output == pass2 input and distinct
features/outputs before accepting one final origin result.

The normal daily transport, external handoff and retirement remain authoritative.
Extra feature creation/release counts are independently checked on rebuild ACKs
and normal shutdown; Native counts verified per-job Evaluate calls for the final
audit. No low-resolution pass, blend, extra residual shader, update-frequency
change or unlocked pass count is present. Frozen SPIR-V bytes are unchanged;
HLSL rebuild equivalence remains unverified.

`NRBRIDGE_HOME` selects an isolated build output root for nrbridge/build.py.
This candidate requires paired Native/Helper markers and the daily entry.
GPU imagery, cost, switches and teardown acceptance are still required.
