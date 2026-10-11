# C4 Helper — initial correctness and failed performance baseline

C3 archive commit: `1df65cd61395aba56aae07c0a0b1c7ed84accf5a` (A), `f9132dff6027d080bd15aafaccad3bcabb026a1b` (B). Integration owns `game-poc-20261011/integration/C4-CONTRACT.md`, SHA `18af8d771d22577e5b7806ed36213762a61bdb3db1b38d49324955e57fb8e0f6`. Old C2/C3/fault sources, builds and evidence remain frozen; this directory archives the first C4 artifact actually run by Integration.

## Reproduce offline build

```bash
cd /home/arinp/Code/nrbridge-design/game-poc-20261011/opti-worktree
python3 poc/game_nr/c4/build_c4.py
```

Only installed compiler, glslangValidator and spirv-val run. No Wine, Vulkan runtime, game, download or installation. `out/build.json` retains the C3 schema (`sources`, `readonly_source_pins`, `generated`, `build_tools`, `readonly_dependency_pins`, `runtime`, `artifact`); it additionally checks source pre/post equality, the reviewed C4 contract and frozen C3 source/generated/artifact identity. Exact compiler/SDK/model/forwarder/core/NVAPI/DXGI/GE paths and hashes are in that manifest. Build histories and the original runtime failures remain preserved. PE, SPIR-V, SDK, model and pixel artifacts are ignored and are not committed.

## Root-only runtime interface

`wine_game_nr_hot.exe resident|skipEvaluate RUN_DIR_WINDOWS SESSION`; the native process must remain the verified Unix ancestor and supply actual exporter PID. `srOnly` is B launcher bypass, not a Helper mode. Config and every control require `protocol_version:4` and exact `execution_mode:hot|diagnostic`; no missing-field inference or C3 peer fallback. Setup filenames and four resource roles/descriptors remain C3-compatible. The config also requires immutable history/params1, matching device/driver UUID, fixed 1920x1080 render/output, manual WhitePoint1/Passthrough0 and the canonical finite budgets before any allocation.

Resident4 uses case `smoke`; hot120 uses `static|motion`; hot1800/18000 uses `sustained`; skipEvaluate4 uses case `skipEvaluate`. Diagnostic is exactly4. One PID/device/feature/four images/slot, source frames strictly increasing; NR reset is first / actual source reset / source gap, otherwise continuous. Actual current and previous viewport/SR jitter0, full rect, depth forward/MVscale1, true prior SR initial-reset evidence and unknown game exposure remain required. No scale reduction or skipped inference is credited as speedup.

| execution_mode | actual validation | allocations/readback | completion scope |
|---|---|---|---|
| hot | API return1, real fenced GPU completion, active tuple; no pixel/alpha/coverage/signature check | no diagnostic pipeline/counter/full pixel buffers; pixel readback0 | `c4_hot_path`, `audit_kind:off`, `numerical_validation:NOT_CHECKED`, `pixel_validation:NOT_RUN`, `signature:NOT_RUN`, dump0 |
| diagnostic | inherited C3 all-pixel GPU counts and independent full CPU counts/FNV/signatures/alpha; every frame full | 82,944,000 pixel bytes +64 counter bytes per frame; three local dumps | `c4_diagnostic`, `audit_kind:full`; actual measured counters/digests only |

Both modes preserve raw NaN/output -1234 clears, actual production keep/proxy/raw/encode/Evaluate/resolve, one pass/WorkingScale1 and all original model parameters/flags. Hot Evaluate success does **not** establish numerical/transport/quality validity. Skip performs encode+poison, calls neither Evaluate nor resolve, publishes INVALID/model_valid=false/output_valid=false; native must reject by its normal predicate, preserve SR, and diagnose sentinel only in diagnostic mode.

Helper acquires all four GENERAL/EXTERNAL images after native input GPU completion, releases all four after its recorded work, and publishes completion only after the existing bounded fence. Native hot return acquires Output alone; diagnostic can acquire Color+Output. `consumed` preserves source/history/reset/audit identity; next slot reuse additionally needs B's actual prior merge GPU last-use completion. Timeout/peer abort never takes normal release; uncertain resources remain held until verified private process exit. Terminal release/retired/audited and public NGX shutdown/module unload are inherited unchanged.

## Timing scope

A persistent pool has six hot or eight diagnostic timestamps. Markers bracket acquire/poison, encode, actual recorded Evaluate commands, resolve and release; diagnostic adds full scan/copy intervals. Values plus availability are retrieved only after the existing fence, with 64-bit/availability flags and no WAIT_BIT/additional submission. Payload is96B hot /128B diagnostic when supported; zero if no pool. Invalid/unavailable queries yield null durations and NOT_AVAILABLE, never zero-time PASS. Masked unsigned delta handles timestampValidBits wrap; period and each query availability are reported.

`evaluate_cpu_call_ms` measures the actual API call; `gpu_fence_wait_ms` measures only vkWaitForFences; `gpu_submit_and_wait_ms` separately includes command end/submission/fence scope. `control_input_read_ms` includes bounded input wait/read/parse. Because the current publication time is known only after publication, `CONTROL_TIMING` logs real JSON serialize+atomic publication and subsequent consumed wait with the frame identity. `frame_elapsed_ms` starts after input acceptance and ends before complete publication; it excludes input/consumed waits and must not be called end-to-end latency. The initial artifact uses coarse `GetTickCount64` for `frame_elapsed_ms` and frame deadline, despite the QPC label for other CPU phases. The actual GE shared TickCount is sampled on a16ms server-update budget: 81/120 elapsed records are zero while QPC waits/GPU intervals are nonzero. Those coarse elapsed records cannot benchmark wall time. QueryPerformanceCounter gives monotonic CPU phase durations; separate GPU queues/devices do not share comparable absolute timestamps. Evaluate GPU intervals include commands/dependencies/scheduling, not a pure NR model benchmark.

GPU cold120/control60/GPU10/frame30 second limits are failure bounds, not acceptable interactive latency. Removing diagnostics changes instrumentation overhead, not the model. Build success is not game correctness, near60FPS, matched90FPS SR performance, sustained play, image quality, load/exit recovery or C4 playable acceptance. Root alone runs those serial gates; accepted and failed baseline evidence is indexed below.

## License and dependencies

New Helper/adapter/diagnostic code preserves GPL-3.0-or-later provenance from OptiScaler PoC; read-only transport/control utility headers retain their MIT notices. Vulkan headers, nlohmann, NGX SDK and installed NVIDIA/GE runtime dependencies are referenced and pinned, not redistributed here. No model/PE content is copied into the source archive. Full dependency provenance remains in C2 manifest and C3 archival evidence index.

## Initial host results — artifact 9af794 / build 935ece

`initial-evidence-index.json` retains exact small-record paths/hashes; `initial-build-index.json` pins the original build. Root archived all executable artifacts in `baselines/c4-first-hot-20261011/manifest.json`; no PE, model, SPIR-V or pixel dump is committed here. `C4-CONTRACT.baseline.md` preserves canonical18af without editing Root's contract.

Diagnostic4 normal passed real Evaluate/resolve, full finite/written/source-alpha/transport and actual Present checks. Diagnostic skip4 passed zero Evaluate/resolve, INVALID rejection and SR presentation. Hot4 and hot120 passed actual API/GPU/tuple/4B finite-range guard, unique Present and normal lifecycle gates; hot does not claim diagnostic pixel/alpha/signature validity. The first two diagnostic attempts missed first INPUT60s, exited3 with zero Evaluate/resolve, and remain BLOCKED_CONTROL_WAIT rather than model failures.

All120 contiguous hot NR sources2195..2314 were evaluated/resolved/presented. The full119 interval window, with no excluded interior stalls, measured22.006110 FPS (mean45.441924ms); matched same-artifact/private Clinic HDR DLAA j0 SR-only measured65.366219 FPS. The near60FPS goal failed. The historical90+ setting is not proven equivalent to this profile. Helper GPU total mean14.281977ms and recorded Evaluate interval13.323071ms include queue dependencies; neither proves pure NR performance or input-to-photon latency. Source-to-selection mean36.606499ms includes native game/SR submission wait, peer control and conversion, and cannot be replaced by a14ms model-only number.

QPC/poll optimization is a subsequent additive experiment, not evidence that the failed baseline achieved60FPS. All resolution, model, shared identity, GPU completion and per-frame inference requirements remain unchanged. Sustained play/load/recovery and C5/C6 remain NOT_RUN.

## Additive QPC frame-clock revision — not yet run

Following Root `integration/C4-POLL-REVISION.md`, the new C4 source uses the existing QueryPerformanceCounter function for frame start,30s frame deadline and fractional `frame_elapsed_ms`. `frame_elapsed_clock` and `frame_elapsed_scope` explicitly identify the same input-accepted to completion-record snapshot interval, excluding input waiting, publication and consumed waiting. Cold-init/setup/control clocks and budgets remain unchanged; this is clock correctness, not a claimed speedup. The9af baseline and invalid coarse elapsed records remain archived above. No GPU/model/shader/resource/acceptance logic changes. This intermediate source revision has not been separately built or run.
