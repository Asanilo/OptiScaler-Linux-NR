# NR→SR stability stage

Primary delivery is direct NR→SR on DS2 / RTX 4060 Laptop / NVIDIA 615.71.09 /
GE-Proton11-7 / Wayland. SR→NR is an experimental path with a reported sustained
flicker failure. It remains visible and separately reported; it does not block
an independently accepted NR→SR delivery.

## Ordered gates

1. Compare the frozen y4m baseline `7b7220bb` with the frozen port `26aea636`, each
   with NR off, NR→SR and SR→NR. Keep the same SF-v2 model, save, camera, SR quality
   and exposure. Passes=1, WorkingScale=1, Detail/Intensity=1. FG, dynamic resolution,
   vsync and frame caps off. Each condition starts a fresh process, warms up for
   10 seconds, observes a stationary camera for 60 seconds, then the same slow
   movement for 30 seconds. Repeat three rounds. Do not change the renderer to
   make this attribution experiment pass.
2. After attribution, implement queue-submission/fence based ownership for model
   handles, textures, descriptors and constants. Recording generations retain
   references until Reset/destruction; each submission retains them until its
   queue completes. No reuse based on evaluate counts. Isolate per-device/SR
   feature state; tracking failures skip NR with an explicit reason.
3. Port Sky's necessary edit-history/reprojection/stabilisation code without
   skipped model frames. Independent stabiliser switch defaults off; initial
   step limit 0.5 stops, despeckle and additional temporal/low-temporal mixing
   off. Pre-SR reprojection includes jitter delta. Reset on game Reset, rebuild,
   size/format or placement change. Invalid history uses this frame's result.
4. Complete real GPU tests and the game acceptance below before delivery.

The anti-flicker/synchronisation renderer changes are not enabled before gate 1.
Cache skip/adaptive acceleration, FG/RR/DualFeature, video and native Vulkan are
later stages. A baseline failure is attribution evidence, never an acceptance pass.

## Implemented preparation

- Native and internal SR hook paths now both call production `EvaluateWithNr`.
  The five-frame contract test calls that same function rather than reproducing
  the production branch in the test. Cases cover typed/untyped restoration,
  create/skip frames, failed SR, exceptions, post ordering, and independent blocks.
- Five intentional mutations must compile and then fail the tests: no restoration,
  wrong setter, ignored master switch, post NR after requested pre NR, and post
  NR after failed SR. This tests the orchestration contract, not the actual vendor
  NGX implementation or the surrounding exported hook's handle routing.
- `tools/game_nr_compare.py` verifies build/model/installation hashes and previews
  replacements by default. It refuses a running game or changed installation,
  backs up each case and restores changed files on a partial failure. Only the
  proxy, forwarder and configuration are replaced; the model remains constant.
  Collection preserves new per-case log evidence and a user observation without
  automatically granting visual/performance acceptance.

From the development repository:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  tests/nr_pre_upscale_test.cpp -o /tmp/nr-orchestration-test
ASAN_OPTIONS=detect_leaks=0 /tmp/nr-orchestration-test
python3 tests/check_nr_negative_controls.py
python3 -m unittest discover -s tests -p '*_test.py' -v

# Preview a frozen baseline post-SR case. --apply requires a normally closed game.
python3 tools/game_nr_compare.py prepare --version baseline --mode post-sr --round 1
python3 tools/game_nr_compare.py prepare --version baseline --mode post-sr --round 1 --apply
# Launch with the same documented Proton/PRIME/dxgi override, then save and exit.
python3 tools/game_nr_compare.py collect \
  ../testlogs/nr-comparison/baseline-post-sr-round-01 --observation flicker
```

Use modes `off`, `pre-sr`, `post-sr`, versions `baseline`, `ported`, rounds 1–3.
Installed configuration must stay unchanged; if it is saved/modified, collection
stops for review instead of silently treating different settings as comparable.
Game SR/vsync/frame-cap settings and camera operations still require an operator.
Capture metrics, controls, start/duration confirmation and raw results must be
reviewed before acceptance. A collected log is not proof that a full run occurred.

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
  leave the stabiliser default-off.
- Report only DS2 local acceptance without another game's evidence. Every build
  carries source/hash/config provenance and a rollback record; each renderer
  change is separately reviewable and revertible.

## Current outstanding work

The six-condition live experiment, actual GPU lifetime changes/tests, stabiliser
port and long-session/performance gates are **not completed**. No success is
inferred from the new contract/tool tests. Preparation can proceed while awaiting
the user's normal game exit and participation in the controlled camera experiment.
