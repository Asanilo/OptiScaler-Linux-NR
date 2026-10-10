# NR flicker investigation: staged capture v2

Comparison build: `67d0881ee8c03e33bb8d3ea1b2a4eb9861e7e421`.
User-observed affected areas: lights, reflections, character edges. Post-SR flickers;
pre-SR has not shown the same sustained flicker. Neither observation proves the
origin of the problem. Existing exposure logs are thresholded, not per-frame.

## What this iteration changes

`Capture 8 frames` records copies at the actual points in the production D3D12
pipeline. It does not reduce NR strength or change placement, white point, cache
cadence, model resolution or pass count. Stage names:

| Stage | Actual resource / interpretation |
| --- | --- |
| original | Game colour before Encode; negative HDR values retained |
| proxy | Model input after Encode and optional working-resolution downsample |
| model_raw | Final vendor pass output before Resolve, without debug-view conversion |
| resolve | Resolve output before Sky or legacy stabilization |
| final_filtered | Output after configured filter attempt; check logs for filter failure |
| final_cached | Output of a cache-only frame; no current-frame model output exists |
| depth / motion | Actual model guide resources, including accumulated cache motion |
| cache_depth / cache_motion | Guides on a successful cache-only frame |

Captures observe the cache rather than suspending it. Missing model stages on a
cached frame must not be filled with an old output. Missing required stages,
changed resource layouts, discarded command recordings and budget exhaustion
produce incomplete batches. Proxy backend capture is explicitly unsupported.

The old implementation mislabeled Proxy as original, discarded dark frames using
a byte-level heuristic and cleared previous captures. All three are removed.
Copies now become readable only after real queue submission completion and command
recording retirement, using `NrGpuLifetime`. Polling happens at the D3D12 seam even
when NR is disabled/skipped. If the seam stops being called, the pending batch waits;
a shutdown with still-pending recordings is explicitly logged rather than mapped.

Each request creates a unique `dlssnr-capture/batch-*` directory beside the DLL,
with `manifest.json` schema 2 and raw images. Image `format` is the decoding
interpretation; `resource_format` and `copy_format` record the distinct resource
and copy-plane formats. A D32 resource may expose an R32 typeless copy plane. Limits: eight NR recordings and
768 MiB readback per batch, including guide sidecars. A failed batch can contain
partial data. Old batches are never automatically removed. Capturing and writing
large raw files affects timing; measure performance with capture disabled.

Metadata includes NR frame ID, a Present hint (not guaranteed consecutive),
placement, effective white point, held game exposure and originating frame,
originating/current pre-exposure, read status and age, reset flags, pass tuning,
guide sizes, motion scales and actual image footprints. Source=1 with a valid
exposure uses the existing held-exposure/current-pre-exposure calculation; this
iteration records that behavior rather than silently replacing it.

A separate verified bookkeeping defect was corrected: exposure-slot labels are
now updated only when the slot is reusable. This is not evidence that the defect
caused DS2 flicker.

## Small game experiment

Use the same save/camera, with NR enabled and **SR -> NR**, Passes=1,
WorkingScale=1, Detail=Colour=Intensity=1. Disable Sky cache, legacy stabilization,
FG, Compare and Debug. Keep every other game setting identical.

1. Game exposure source: wait 10 seconds; collect three eight-frame batches about
   one second apart while camera and character remain stationary.
2. Change only white-point source to Paper white only, fixed value 1.0; settle and
   repeat the same three captures.
3. Change placement to NR -> SR with the same fixed white point; settle and repeat.

Use F10 -> DLSS Neural Rendering -> Inspect -> Capture 8 frames, or let the
operator's log collector create `dlssnr-capture.trigger` beside the DLL. No Steam
wrapper is required. The trigger is checked every 60 NR dispatch frames.

After the first preview, select three normalized ROIs (light/reflection/edge).
Use identical coordinates for every batch. Eight recordings can miss an event;
repeat windows if needed rather than concluding the problem is absent.

## Analysis

Requires Python 3 and NumPy. Outputs go to a new directory; existing results are
not overwritten.

```sh
python3 tools/analyze_nr_capture.py /path/to/batch --output /path/to/new-analysis \
  --roi light:0.1,0.1,0.2,0.2 --roi reflection:0.4,0.5,0.2,0.2 \
  --roi edge:0.6,0.2,0.1,0.4
```

ROI values above are examples, not DS2 selections. Without `--roi`, full-frame
statistics and previews are generated for choosing the real regions. Raw HDR
values, footprint offsets and row pitches are preserved during decoding.
Previews use one fixed white point across the batch. They are not the measurement.
`metrics.csv` and `analysis.json` provide mean/p95/max luminance, temporal absolute
changes, and paired model/Resolve/filter edit measurements. Unknown game transfer
functions are explicitly labeled. Different colour spaces and dimensions must
not be compared numerically without a defined mapping. Camera motion is not
registered; fixed-camera measurements and moving-camera ghosting checks serve
different purposes.

## Fix and acceptance gates

- Original fluctuates: examine game SR output and resource/timing inputs first.
- Proxy first fluctuates: examine exposure age, HDR mapping and encoding.
- Raw model first fluctuates: verify guides, reset/history and model invocation.
- Resolve first amplifies instability: reproduce through the production Resolve
  shader and correct its ratio/colour/boundary logic.
- If vendor output remains unstable with correct inputs, an optional local
  temporal filter is permitted, default off, with motion/depth disocclusion
  rejection and independent ghosting/detail checks. Do not stack filters blindly.

The targeted image fix and real game acceptance remain pending actual captures.
Passing decoder/GPU infrastructure tests does **not** mean flicker is fixed.
Acceptance must compare the same save/scene in three repeated stationary and
slow-turn runs, preserve NR detail/strength, check the three affected ROIs and
new ghosting, and cover pre-SR, Sky interval 1/2, toggles and Alt-Tab. A long-session
Linux acceptance claim remains separate from this focused experiment.

## First DS2 capture and diagnostic correction

The first 63dc524a DS2 batch stopped on frame 2 with `resource_layout_changed`.
The guard compared the previous copy footprint format with the next resource
format; a depth-plane format difference is valid and does not indicate a resize.
This was a diagnostic bug, not evidence of an in-game resource change. The
partial batch is preserved. The guard now compares resource formats across
frames and the manifest records decode/copy/resource formats separately. The
GPU fixture now captures an actual D32 depth texture across two frames and
checks its values. No NR rendering or white-point algorithm changes accompany
this correction.

## DS2 three-condition results and motion-input candidate

Local comparison build `10fd7a5211b829d93c8b17289e513eb814a955c0` produced
three complete eight-frame batches for each condition (72 frames total). The
operator reported sustained flicker for post-SR with either game exposure or
fixed paper white 1.0, and no sustained flicker for pre-SR with fixed white 1.0.
All recorded effective white points were 1.0. This weakens exposure oscillation
as an explanation for **these windows**, not every scene.

Candidate light/reflection ROIs already show additional variation in raw model
output. For example, mean temporal luminance MAE over 21 transitions in the
post-SR reflection ROI was 0.000081 / 0.000916 for proxy / raw model with game
exposure, and 0.000082 / 0.000949 with fixed white. Both stages were decoded from
sRGB to linear for this comparison. The ROIs await operator confirmation.
Character idle motion is a confounder. Pre-SR captures contain render jitter,
are lower resolution, and precede SR: their raw pixel MAE is **not** a measurement
of the final pre-SR screen's visible flicker.

The game declares `MVJittered`. Static-light median vectors multiplied by
(-640, 360) match **previous jitter minus current jitter**. Across the 42 adjacent
pairs of the two post-SR conditions, motion RMS was 0.510455 render pixels;
subtracting this jitter term leaves 0.000649 pixels. See NVIDIA's
[DLSS programming guide, Motion Vector Flags](https://raw.githubusercontent.com/NVIDIA/DLSS/main/doc/DLSS_Programming_Guide_Release.pdf)
for the flag's meaning. The NR call supplied neither a jitter flag nor offsets.
This is evidence of an input-coordinate mismatch with unjittered post-SR colour;
it is not yet causal proof for the observed flicker.

The candidate D3D12 correction point-loads the game's motion texture into a
private RG32 float texture and adds `(currentJitter - previousJitter) / MVecScale`.
The game resource and real motion remain intact. The corrected guide feeds NR,
Sky accumulation/reprojection and legacy stabilization. Pre-SR model input is
unchanged; cache/stabilizer reprojection no longer adds jitter again when the
source vectors already contain it. Missing/invalid inputs, discontinuity,
discarded recordings, placement/flag/size/scale changes and game resets invalidate
history. Cache-only input frames still advance the jitter history.

Capture now includes `game_motion` (before correction/accumulation) and
`game_depth` (before guide cloning), plus the actual model guides and correction
metadata. Existing 72 model-depth captures contain zero throughout. Since the
original depth was not recorded by that build, this does **not** establish whether
the game, guide copy or diagnostic path lost depth. That check remains open.

Local evidence lives outside Git under `testlogs/nr-flicker-10fd7a52/`, including
`comparison-summary.json`, `motion-jitter-evidence.json`, candidate ROIs, batch
hashes, logs and per-stage analyses. No game captures or vendor binaries are
published. The candidate still needs full-DLL build and controlled in-game
acceptance. Do not call the flicker fixed from synthetic shader tests alone.


### Reproduced depth-copy defect

An isolated RTX 4060 / GE-Proton 11-7 test reproduced the existing D32S8 clone
path: source depth 0.625, `CopyResource` into a non-depth resource with
`R32_FLOAT_X8X24_TYPELESS`, clone readback 0.0. The failing run is preserved at
`testlogs/nr-motion-final/`; it is not counted as a passing run. This establishes
a reproducible integration defect, although original game-depth capture is
still needed to connect it to the in-game all-zero guides.

The fix explicitly samples plane 0 with a depth SRV into an owned R32_FLOAT UAV.
It restores the game's resource state, uses the existing recording/fence holds,
and supplies the extracted texture to NR and both stabilization paths. It rejects
unsupported multisampled/array/non-readable inputs rather than fabricating depth.
D3D12's [planar depth specification](https://github.com/microsoft/DirectX-Specs/blob/master/d3d/PlanarDepthStencilDDISpec.md)
distinguishes the resource, depth SRV and copy-plane formats. The fix keeps those
roles separate and avoids the base shader helper's incorrect conversion of a
valid R32_FLOAT_X8X24 depth SRV format back to a DSV format.

Candidate validation: CPU jitter-history tests with ASan/UBSan (LeakSanitizer
disabled because the execution sandbox uses ptrace), six capture decoder tests,
and the real NVIDIA GPU suite passed. The GPU suite checks typed/typeless D32S8
and D24S8 spatial depth patterns, zero/far values, post-SR motion preservation,
cache intervals/history/lifetime and legacy stabilization. Ten expected outcomes
include seven deliberately failing controls; restoring the old depth copy and
reversing the motion correction each trigger the intended assertion. Evidence:
`testlogs/nr-guide-fix-final/`. No claim of vendor-model or game-image acceptance
follows from these tests.


## First in-game verification of fee544fe

The operator reports that sustained flicker is no longer visible in the current
DS2 private-room scene. Three new complete eight-frame batches verify post-SR,
one completed NR pass, working scale/detail/colour/intensity 1.0, Sky and legacy
stabilization off, and Debug/Compare off. NR is running; this observation was not
obtained by disabling or weakening it.

All 24 extracted depth images exactly match their corresponding original game
depth images. Every pixel has nonzero depth (range across the windows approximately
0.0153754 to 0.0865254, reversed Z). All 24 actual model motion images match the
recorded original vectors plus the recorded jitter correction, with zero maximum
component error in decoded float32. No model history reset occurs in these windows.
A static background ROI has approximately 0.505619 pixel RMS motion before
correction and 0.000619 afterward.

The new camera is visibly different from the baseline: the display-shelf lamp
moved from approximately y489 to y566 at 1080p. The inherited ROI now samples
background above the lamp. Its motion check is useful, but pixel-aligned
before/after flicker percentages would be invalid. Both fixes shipped together;
their separate contributions to the visual improvement were not measured.

Evidence is local at `testlogs/nr-flicker-fee544fe/`: `acceptance.json`,
`guide-verification.json`, `camera-comparison.json`, the operator report, hashed
capture manifests and stage analyses. This is a positive current-scene result,
not full Linux acceptance. The operator also completed the requested slow camera
turn and character walk/stop check and reports stable output with no obvious new
ghosting. The operator then checked Sky-off NR->SR and SR->NR with Sky intervals 1 and 2,
reporting all three normal without sustained flicker, obvious new ghosting or
crashes. Logs confirm the placement changes and actual Sky intervals 1/2, including
alternating model refresh/cache reuse at interval 2. Long sessions, other scenes
or games, explicit Alt-Tab stress and controlled FPS/latency remain unaccepted.
