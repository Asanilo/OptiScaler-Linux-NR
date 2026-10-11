# C3 continuous real-game NR — canonical experimental contract

Integration owns this document. 2026-10-11. IMPLEMENTATION_AUTHORIZED; runtime NOT_RUN. This extends the accepted C2 contract (SHA256 `2a46defd341f87270f3d5afaf45cec4a24012f5e3bdd5fdd8617a40f0cd2c214`), not a frozen production ABI. Where unchanged, C2 resource formats, actual game profile, strict wire types, dependency pins, ownership, failure and private-launch rules apply. C2 originals remain frozen.

## Scope and modes

CLI `resident|skipEvaluate RUN_DIR_WINDOWS SESSION`. New C3 sources/artifacts only. One session, generation 0, one shared resource set, one in-flight frame, resident Wine PID/device/model/feature/pipelines/four images. No per-frame model initialization or shared allocation. No SR in Helper. Real HDR Run, DLAA 1920x1080, jitter zero from startup, actual Depth/MV same texel, manual WhitePoint1/exposure unknown: exactly C2 narrow profile.

`game-config.json` retains C2 fields and deadlines, with mode resident or skipEvaluate, `frame_count` and `max_frames` equal to 4 or 120, `diagnostic_case` one of smoke/static/motion/skipEvaluate, `history_version:1`, `params_version:1`. smoke=resident/4; static and motion=resident/120; skipEvaluate=skipEvaluate/4 initially. Root can add a 120-frame negative with the same contract if evidence warrants it. No silent reduction of successful frame count. Terminal release/retire/audit frame is frame_count and source is the last processed source_game_frame. Setup remains frame0/source0. Filenames retain actual `-g0`, per-frame files use `-fN`.

Private game launch uses `BB_FPS=uncap` (existing delta-time mode) plus `BB_FPS_LIMIT=90` (existing launcher FPS limit). Do not use `BB_FPS=90`, which selects a fixed-timestep patch, as a substitute for the limiter. Verify actual setting/log, not only an outer env later overridden. This is a cap, not a promise of 90 FPS or a count of test frames. User reports original game above 90 FPS; measured comparable capped SR baseline is still required. C4 reports actual NR loss and aims toward 60 FPS without hidden resolution/inference changes.

## Frame identity and history

For each frame N=0..frame_count-1, exact C2 envelope plus `prev_source_game_frame` (0 for first), `history_version:1`, `params_version:1`, `nr_reset` integer0/1 and `reset_reason` string. First => reset1/reason first. Subsequently actual source_reset => reset1/sr_reset; otherwise source_game_frame != previous+1 => reset1/source_gap; otherwise reset0/continuous. Source IDs strictly increase. Keep real SR reset/history values; do not reset mature SR to satisfy NR. New session creates a new NR feature. Fixed history_version identifies this session's history epoch; do not increment it for an ordinary reset.

Helper validates native-derived reset against actual input sequence and echoes the published wire representation, not in-memory signed/unsigned JSON variants. Completion and consumption echo exact current identity, source and previous source, history/parameter version and audit kind. Strict active tuple, deadline, GPU-completion and model-valid predicate is common to both modes. No stale or expired result can be selected, even if the tuple otherwise matches.

## Ownership and slot reuse

Native writes four acquired shared images, releases GENERAL/EXTERNAL, submits the current scheduler tick and observes bounded GPU completion before INPUT_READY. Helper acquires all four, runs its work, validates, releases all four GENERAL/EXTERNAL and observes fence before complete.

Canonical native alpha strategy: on return acquire **Color and Output** from EXTERNAL before reading either. Native GPU validator samples shared Color and Output and compares original source alpha bits; never read released Color without acquire. Compare source signature against the one recorded at native input. Only a valid resident result converts Output to separate local return97. Release both Color and Output back to EXTERNAL, observe actual GPU completion, then publish consumed. Depth/MV remain EXTERNAL after Helper release until next native input acquisition.

Helper waits for consumed and then for the next INPUT_READY; consumed does not authorize another model operation or reuse of native local-return memory. Native cannot overwrite the slot/local return for N+1 until N consumed was sent and the actual last GPU merge use of local return N completed. Record that scheduler tick; do not block waiting for an unsubmitted tick. Flush changes command buffers: reacquire handles and preserve the C2 actual local-return-to-compute-read barrier. If the next frame must bypass until safe, log actual source gap and reset; do not call that bypass a valid NR output. Every accepted frame must have a real merge/guest-buffer/Present ledger. No previously selected image may silently carry into a later source frame.

Final local use completes before native imports are destroyed and terminal NATIVE_RELEASED is sent. Preserve full same-state FD inventories and original payload retirement evidence, then Helper retirement/audit/model shutdown and actual exit/reap. Continuous resource retention is intentional; validate absence of growth and exact teardown, not a fabricated per-frame close.

## Per-frame real work and GPU validation

Every resident frame: raw RGB poison NaN; output RGBA poison -1234; production encode; actual Evaluate once/result1; resolve once; actual GPU completion. skipEvaluate keeps encode/poison but Evaluate/result/resolve0, INVALID, no selected output. It uses the same acceptance predicate then a separate diagnostic path proving rejection and SR Present. Do not treat raw model alpha as a validity requirement.

All frames run full-pixel GPU validation, not sampled pixels. 1920x1080 totals: pixels2073600, rawRGB6220800, RGBA8294400. A's tiny readback is 16 uint32 /64 bytes in this order:

| index | value |
|---|---|
|0|rawRGBfinite|
|1|rawRGBwritten (equals finite for NaN poison)|
|2|outRGBAfinite|
|3|outRGBAwritten (finite and not sentinel)|
|4|sourceRGBAfinite|
|5|sourceAlphaFinite|
|6|alphaBitsEqual source versus output|
|7|sourceXor|
|8|sourceSum|
|9|outXor|
|10|outSum|
|11|visitedPixels|
|12|reserved zero|
|13|rawRGBNaN|
|14|outSentinelCount|
|15|reserved zero|

Native has no private raw image; raw indices0/1/13 are zero/not-applicable there and never used to claim native model inspection. Its source/output/alpha/signature/visited counts must be real GPU checks. Workgroup64 shared reduction then global atomics; zero counters before dispatch, explicit shader-write to host-read visibility and bounded fence before CPU read. Resident requires all finite/written counts, source alpha equality, rawNaN0/outputSentinel0; skip requires rawNaN6220800 and outputSentinel8294400 on Wine, native sentinel8294400, no acceptance. Log per-frame API counts plus cumulative counts separately.

Signature `rgba32f_mix32_xor_sum_v1`: for each row-major component c, word_index=4*(y*1920+x)+c, bits=floatBitsToUint(actual texelFetch component). All arithmetic uint32 wrap:

```
h = bits XOR (word_index * 0x9e3779b9)
h ^= h >> 16; h *= 0x7feb352d;
h ^= h >> 15; h *= 0x846ca68b;
h ^= h >> 16;
xor32 ^= h; sum32 += h;
```

No frame salt. Wire uint32 fields `input_signature_xor32`, `input_signature_sum32`, `output_signature_xor32`, `output_signature_sum32`, with `signature_algorithm:"rgba32f_mix32_xor_sum_v1"` in input and complete. Input contains the two input signature fields; complete contains all four. Native input versus Wine actual source, and Wine output versus native actual output must match. These noncryptographic signatures prove transport consistency only; model validity requires the independent API/poison/fence checks above. Never use changing hashes as proof of motion or inference.

## Full audit schedule and completion fields

`audit_full = (frame_count==4) || (N==0) || ((N+1)%30==0) || (N==frame_count-1)`. For120:0/29/59/89/119. Both sides derive and reject mismatch. `audit_kind` is full or gpu_full_scan in input/complete/consumed and logs. Full audit preserves accepted C2 three local readbacks raw/sourceColor/output (82944000B), raw dumps, independent input/output FNV1a64 over33177600 bytes and source-alpha audit. Full audit source is actual shared sourceColor, not clamped keep. Non-full frames omit FNV/dump fields rather than writing invented values. No CPU pixels cross processes.

Completion retains C2 numeric model/stat evidence, mapped from actual GPU counters for non-full frames; source_alpha_preserved is alphaBitsEqual. Add actual signature fields, source_rgba_finite, raw_rgb_nan, output_sentinel_count, visited_pixels, audit_kind, previous source/history/params/reset_reason echo. `reset` equals current nr_reset (not always1). Full frames additionally carry C2 real FNV fields. resident OUTPUT_VALID requires per-frame Evaluate1/result1/resolve1 and counters; skip INVALID has Evaluate/result/resolve0 and poison counts. `evaluate_calls_total` and `resolve_calls_total` are cumulative actual counts (N+1 for resident, zero for skip), separate from per-frame calls. Both record measured phase durations, audit overhead and GPU completion separately. Diagnostic wall time is not a pure NR performance benchmark.

## Bounds, negative tests and acceptance

C2 cold120s/control60s/GPU10s/frame30s budgets remain finite. Root outer bound is explicit per case and never changes inner limits. Deadline check must reject a completion observed after expiration even if the file exists. Quarantine active resources/tuple on failure, preserve original SR, native.abort, no slot reuse. Known own GPU completion plus peer drain or verified private Helper exit is required before cleanup; abnormal exit is not normal release. Only root performs pidfd fault injection/GPU/game tests; agents build/review only. Root owns serial GPU lock and private process/prefix cleanup.

Gates: smoke4 accepted actual Present; static120; motion120 with real input/camera motion evidence and screenshots; skip4 rejected outputs with SR Present; controlled timeout/late (expired complete rejected) and actual Helper exit with SR fallback. Keep distinct normal lifecycle versus abnormal cleanup evidence. If an interface defect prevents one gate, report it specifically; no C3 PASS from the positive counter alone. Capture actual presentation images on predetermined full-audit frames; note the existing next-VideoOut association and scheduler boundaries honestly.

All source/build inputs/runtime dependencies/Git SHA/logs/failed attempts retained. C2 baseline commits: bbport 2b0cb9206ee96668f146ea8b9cfd5aec8b572467; Helper archive 228b5d845e42af1a68ec3201fc524e4cf4407197. No stable repo/system/game install changes, no ABI freeze, no external semaphore repair, no FG. C4/C5/C6 proceed after C3 evidence passes within existing authorization.
