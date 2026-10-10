# Current remaining work

Updated 2026-10-10. This document describes outstanding work from the existing DS2 integration. A new game's scope will be assigned separately by the user; do not begin it from this handoff. In the shared workspace, read [handoff](../../docs/handoff-20261010.md), [current state](../../docs/game.md) and [provenance](../../docs/game-references.json) first.

## Completed and scoped results

- Direct pre-SR and runtime placement comparison, production parameter restoration, queue-fenced ownership and per-device/SR state are implemented.
- Sky complete edit cache, alternate/adaptive cadence and anti-flicker are implemented. Timing uses paired completed GPU samples; last cached/composition samples are not whole-frame latency. See [Sky cache](sky-cache.md).
- `fee544fe` corrects planar depth extraction and post-SR jittered motion. DS2 current-scene static/moving, pre-SR and Sky intervals1/2 short checks passed. Evidence: workspace `testlogs/nr-flicker-fee544fe/acceptance.json`. The two fixes were installed together, so individual causal shares are unmeasured.
- `eee98821` adds native DLSS-G request and raw runtime diagnostics without changing the NR fixes. See [FG results and limits](fg-high-multiplier-test.md).
- FSR4.1.1 INT8 SR and live return to DLSS were observed on the same machine. User reports broadly similar FPS, FSR slightly lower. This was not a controlled benchmark. See [FSR record](../../docs/fsr41-ds2-test.md).

## Remaining gates

NR's original long-session gates remain uncompleted: 60-minute play,20 enable/placement switches,10 each of Alt-Tab, SR-quality and resolution changes, no crash/device loss/black frame/sustained flicker or unbounded warmed-up memory growth. Current-scene checks do not satisfy these gates. Other scenes/games, actual NGX multi-device behaviour and native Vulkan/DX11 combinations remain unaccepted.

Controlled performance needs the same scene, resolution, SR quality, NR placement/model/cache and FG state. Collect three repeated runs and distinguish NR GPU cost, render/present intervals and input latency. The original stabiliser target of added GPU median≤0.5ms and Present p95 deterioration≤5% was not measured; it remains default-off. The approximate54→64FPS cache feedback and later FSR/DLSS comparison are observations, not benchmark proof.

Native FG still reports `setDynamicMFGParams failed with status1` despite DisableReflexSync/DisableFlipMetering. Investigate the actual RSYNC negotiation and frame intervals; do not suppress the error to claim success. Unique interpolated images, display spacing, latency and long-session acceptance remain pending. Current2X/4X/5X/6X request/present counters do not establish them.

FSR4.1.1 SR needs dedicated movement/edges/ghosting, history/reset/resize and long-session checks, plus an FG-off, NR-off controlled comparison with DLSS. FSR4 FG was not deployed or validated. Do not infer NVIDIA FG4 support from FSR4 INT8 upscaling.

No arbitrary-game SR/NR/FG injection has been accepted. Temporal FSR2+/XeSS/DLSS inputs and DX12 interfaces provide integration opportunities; missing depth/motion/temporal entry points require separate adaptation.

## Operation and build

The user starts/stops and operates normal Steam. Install verified files only after normal exit; never operate the game through Python launch wrappers. Keep original files, source/build/hash provenance and raw failed observations. Current game binary source is `eee98821`; later documentation commits do not mean a new DLL was deployed.

In the shared workspace, `build/install_fsr41_test.py --rollback` previews the outer FSR restore. Restore it before `build/install_fg_test.py --rollback`. Add `--apply` only while DS2 is stopped and integrity checks pass. Do not bypass mismatch guards; reconcile user-saved configuration with recorded provenance first.

Use the existing `Build (Fast)` workflow, explicitly targeting the own fork and `linux-nr`, for source changes. It packages the proxy and NR forwarder, not models. `clang-format.yml` expects clang-format20; the local toolchain copy is under shared workspace `build/toolchain/clang-format-20/clang_format/data/bin/clang-format`. The unmerged local `ci/linux-nr-build` workflow branch remains separate because workflow-scope credentials were unavailable; default-branch integration is not completed.

Independent GPU fixtures use an isolated Proton prefix and shared workspace LLVM-MinGW/Detours directories, not the game prefix. Build from the repository root with `tools/build_nr_gpu_tests.py`, then run the corresponding `tools/run_nr_cache_gpu_tests.py`. Provenance remains in shared workspace `build/nr-*/gpu-build-manifest.json` and raw `testlogs`. The fixtures and deliberate defect controls do not substitute for game appearance or stress checks.

## Historical evidence

The frozen y4m baseline `7b7220bb` reproduced post-SR flicker; the user stopped the remaining six-condition matrix. Baseline/off round2 was prepared but not played. Do not mark it complete or repeat it as the default starting point.

The minimal stabiliser build `502c4140` executed but remained visually ineffective. Full Sky cache alone also did not establish a fix before the guide correction. Preserve these failures; see [the original stability-stage document at3ae7dc6e](https://github.com/Asanilo/OptiScaler-Linux-NR/blob/3ae7dc6ee94e8530245e4123047e39d0394867a4/docs/next-stage.md), current [capture diagnosis](nr-flicker-capture.md), and workspace raw `testlogs/nr-iteration-502c4140/`.
