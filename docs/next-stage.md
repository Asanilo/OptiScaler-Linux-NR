# NR→SR stability stage

Primary target: DS2 / RTX 4060 Laptop / NVIDIA 615.71.09 / GE-Proton11-7 /
Wayland. The previous SR→NR build has a sustained flicker failure. The new build
is a candidate for testing; neither its appearance nor Linux acceptance is granted.

## Diagnosis and completed implementation (502c4140)

The frozen, unmodified y4m `7b7220bb` also reproduced post-SR flicker with the same
SF-v2 runtime and Detail/Intensity=1. Evidence remains in
`../testlogs/nr-comparison/baseline-post-sr-round-01`. This shows that the symptom
is not exclusive to the port, but does not establish its cause or exclude other
port regressions. The operator stopped the remaining six-condition matrix.
Baseline/off round 2 was prepared but never played; do not mark it completed.

- Actual command-list recording generations now retain model owners, COM
  resources, descriptor heaps and constants. Reset/final Release closes a
  generation; every actual queue submission must finish before retirement or
  slot reuse. Replay, concurrent Execute→Signal gaps and cross-queue dependencies
  are tracked. Tracking failure skips NR with a reason and retains unsafe objects.
- Internal contexts are isolated by device and actual SR handle, including handle
  zero. SR Release tears down its context. SDK shutdown drains submissions before
  forwarding core shutdown; a timeout/device loss keeps the core alive. The
  external, undocumented runtime's multi-device behaviour remains unvalidated.
- Meter/calibration/capture and NR timestamp readbacks use those completion
  tokens. Timestamp frequency comes from the actual submission queue. CPU frame
  counts are not a completion proof.
- A minimal every-frame Sky edit-history port reprojects with depth/motion and
  pre-SR jitter delta, rejects invalid history and bounds luminance edit change.
  Game Reset, rebuild, size/format/placement change invalidate history. Invalid
  history uses the fresh current NR result, preserving alpha. No skipped model
  frames, adaptive cache, extra temporal mix or low-temporal mix were added.
- Stabilisation defaults **off**, step limit **0.5 stops**, despeckle **off**.
  Detail/Intensity remain **1**. Its independent fenced GPU timer reports the
  last completed sample; this is neither a median nor whole-frame time.

Implementation commits: `c6542adf` (lifetime/context isolation), `37267926`
(stabiliser), followed by the reviewed drain/runtime-test correction. Final
source and artifact hashes are recorded in the delivered build manifest.

## Verification and limits

CPU orchestration tests call the same production `EvaluateWithNr` used by both
SR hook paths. ASan/UBSan passes; five intentional contract mutations fail.
The portable lifetime test covers every-queue completion, repeated submissions,
Execute→Signal pending state, discarded recordings and device removal.

Real NVIDIA D3D12 fixtures run through an independent GE-Proton prefix, not the
game prefix. The runtime fixture compiles production `NrGpuLifetime.cpp` with a
minimal real Execute hook adapter instead of the game's FG system. It verifies
actual Reset/Release/Execute/Signal hooks, delayed queues, bounded slot exhaustion,
replay on two queues, automatic cross-queue waiting, readback/model-owner
retirement and SDK drain. Early upload overwrite and owner retirement controls
must fail. This does not exercise vendor NGX, game-export handle routing or FG.

The shader fixture executes the exact production DXBC on NVIDIA hardware:
current-frame fallback, 0.5-stop bound, reset, depth/colour/out-of-frame rejection,
jitter checker and alpha. Reversed-depth reset is covered; complete reversed-Z
reprojection, production wrapper integration, appearance and motion trails still
need game evidence. The numerical tolerance includes FP16 history quantisation.

Local raw evidence: `../testlogs/nr-lifetime`, `../testlogs/nr-stabilizer`.
GPU fixture binaries, source hashes, compiler and Detours compatibility edits
are recorded in `../build/nr-gpu-final/gpu-build-manifest.json`.
Windows Release is built by the existing `Build (Fast)` workflow on `linux-nr`.
Models, game logs and downloaded toolchains are not uploaded to GitHub.

Rebuild isolated fixtures with the official LLVM-MinGW toolchain and unmodified
Microsoft Detours v4.0.1 (`e4bfd6b03e50de46b47abfbd1e46b384f0c5f833`):

```sh
python3 tools/build_nr_gpu_tests.py \
  --toolchain ../build/toolchain/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64 \
  --detours ../build/toolchain/detours-v4.0.1-original \
  --output ../build/nr-gpu-final
```

## Ordinary Steam operation

The user starts and operates DS2. The assistant installs only a verified proxy,
forwarder and configuration while the game is closed, preserves the same SF-v2
model, and records hashes/previous files in a separate rollback snapshot.
`tools/install_nr_iteration.py` previews by default and refuses running games,
changed installation/build files, duplicate backups or failed build provenance.
The old Python Steam wrapper is retained as historical tooling and is no longer
the recommended launch path. Do not repeat the frozen matrix to begin this A/B.

Keep GE-Proton11-7 selected. Steam Properties → Launch Options:

```text
WINEDLLOVERRIDES="dxgi=n,b" __NV_PRIME_RENDER_OFFLOAD=1 __VK_LAYER_NV_optimus=NVIDIA_only PROTON_LOG=1 PROTON_LOG_DIR="/home/arinp/Code/dlss5_linux/testlogs/nr-comparison/manual" %command%
```

`%command%` is expanded by Steam only. Start with Steam's Play button; from a
terminal the ordinary command is `steam -applaunch 3280350`.

For the new candidate, first check NR→SR with stabilisation off. In F10 →
DLSS Neural Rendering, select SR→NR in **Live placement comparison**, then toggle
**Edit stabilization (experimental)** off/on in the same saved scene. Wait after
each toggle and record stationary flicker and the same slow camera motion.
Preselect clothing/person, highlight boundaries and vegetation regions. Leave
Passes=1, WorkingScale=1, Detail/Intensity=1 and step=0.5; FG/dynamic resolution off.
Compare both appearance and trails. Return to NR→SR and repeat the switch as a
regression check. A surviving failure stays a failure in the record.

## Acceptance fixed before implementation

- A real D3D12 harness must test delayed queue completion, slot exhaustion,
  command-list Reset/repeated submission and safe retirement. CPU mocks do not
  satisfy this gate. Negative early-reuse/retirement controls must be detected.
- NR→SR: 60-minute play session; 20 enable/placement switches; 10 each of Alt-Tab,
  SR-quality changes and resolution changes. No crash, device loss, black frame,
  sustained flicker or continuously growing memory after warm-up.
- Select clothing/person, highlight boundaries and vegetation observation regions
  before stabiliser A/B. Retain failures; do not lower Detail or select only the
  winning regions. Record both appearance and motion trails.
- Three controlled performance repetitions: added stabiliser GPU median ≤0.5 ms,
  and whole-frame Present p95 deterioration ≤5%. Present intervals are explicitly
  distinguished from NR GPU cost. FG is off. Above limits: optimise further and
  leave the stabiliser default-off. Sparse last-sample logs cannot satisfy this.
- Report only DS2 local acceptance without another game's evidence. Every build
  carries source/hash/config provenance and a rollback record; each renderer
  change is separately reviewable and revertible.

## Outstanding gates

The `502c4140` DLL was installed and actually executed in DS2. The operator's
follow-up enabled the minimal stabiliser; it remained ineffective and post-SR
visual acceptance **failed**. Raw on/off and actual GPU-cost evidence is retained
in `../testlogs/nr-iteration-502c4140/stabilizer-followup-20261009T181009Z`.

The next port now implements [Sky's complete cache and anti-flicker](sky-cache.md),
including optional adaptive cadence. Its isolated production-wrapper GPU tests
and intentional defect controls pass. New DLL game appearance, extended NR→SR
stress/memory and controlled performance repetitions remain **pending**.
Native Vulkan, RR/FG/DualFeature and video remain outside this port's acceptance.
Source/Windows/GPU checks do not establish that DS2 flicker is fixed.
