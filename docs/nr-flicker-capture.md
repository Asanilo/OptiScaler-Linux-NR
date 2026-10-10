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
