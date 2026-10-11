# C3 resident game NR helper — build candidate

Integration owns the only contract at `/home/arinp/Code/nrbridge-design/game-poc-20261011/integration/C3-CONTRACT.md`. This new implementation reuses the accepted C2 archive; frozen C2/G3 artifacts and stable sources are untouched. The initial candidate has now passed the narrow C3 gates through Root serial execution; see the evidence index below.

Build only:

```sh
cd /home/arinp/Code/nrbridge-design/game-poc-20261011/opti-worktree
python3 poc/game_nr/c3/build_c3.py
```

The script validates existing compiler/SDK/runtime pins, compiles GLSL, runs static SPIR-V validation, compiles the Windows PE helper, and records sources/generated inputs/tools/read-only dependencies and raw logs in `out/build.json` and timestamped `out/build-history/`. It does not invoke Wine, Vulkan runtime, a game or GPU. No new dependency is installed. Models, PE runtime and generated outputs are excluded from Git. Licensing remains as described in `../LICENSE-NOTES.md`; new diagnostic/helper/adapter sources use GPL-3.0-or-later, inherited transport/control remain MIT.

CLI: `wine_game_nr_resident.exe resident|skipEvaluate RUN_DIR_WINDOWS SESSION`. The private native ancestor supplies the verified Unix PID; existing absolute GE64 loader and pinned DXGI/NVAPI/NGX model dependencies apply. Root supplies all runtime tests and GPU locking. Helper never runs SR/FG. Initial milestones are resident smoke4/static120/motion120 and skipEvaluate4, followed by root-owned timeout/late/helper-exit negatives.

One helper PID/device/feature/four DMA-BUF images remains allocated throughout generation0. Each input has exact C2 identity plus dynamic previous source/history/params/reset reason and audit kind. Duplicates/backwards sources fail. Reset is first1, then true SR reset or source gap, otherwise0. Production shader remains unchanged; only a quiescent, reset command buffer permits descriptor-pool reuse. All four shared images are acquired from EXTERNAL and returned GENERAL/EXTERNAL before actual GPU fence and completion. Consumed is required before another input. Native additionally owns the actual merge-last-use gate; Helper cannot infer its completion from an unsubmitted tick.

Every frame poisons raw RGB with NaN and output RGBA with -1234, records real Evaluate/resolve, and scans every pixel with `validate_frame.comp`. A 64-byte GPU counter/signature readback proves finite/write coverage, alpha bit preservation and source/output transport signatures. Signatures use the exact position-mixed uint32 xor/sum canonical and are not cryptographic or proof of inference. Model validity also requires actual Evaluate1/result1/resolve1/poison coverage/fence. skipEvaluate publishes INVALID with no evaluate/resolve, preserved poisons, and no model-valid claim.

Full audit frames are all smoke/skip4 frames; for120, 0/29/59/89/119. Full audit retains raw16F + actual shared sourceColor32F + output32F (82,944,000 bytes), independent CPU counters/signatures, byte FNV and local dumps. Non-full frames omit FNV/dump fields, scan every GPU pixel and read only64 bytes. CPU pixels never cross the process boundary. Keep is not required to equal raw Color; production encode clamp remains unchanged.

C2 finite bounds remain cold120s/control60s/GPU10s/frame30s. Failure holds/quarantines resources and records private process exit separately from normal release. Normal shutdown requires real fences, shared retirement and audit, then feature release, diagnostic destruction, production resource destruction, model/core shutdown and DXGI/NVAPI release. No raw host FD is closed by PE CRT.

GPU timestamp segments cover encode including acquire/poison, Evaluate command region, resolve, full-image validation, optional GPU copies and release. Timestamp boundaries include barriers and command scheduling; they are not pure NR performance. CPU Evaluate call time, fence wait, CPU stats/digests/dumps and full frame wall time are separate. Native game cap is `BB_FPS=uncap` + `BB_FPS_LIMIT=90`; it is not a 90-frame test or achieved FPS. C4 aims near60FPS at unchanged resolution and one real inference per effective frame, reporting any measured gap.

Accepted status: compiler/GLSL/SPIR-V validation PASS; Root smoke4/static120/motion3-120/skip4/timeout/earlyexit/real-late gates PASS in the fixed profile. The two earlier motion attempts remain BLOCKED in `../c3-evidence/index.json`. C4/C5/C6 NOT_RUN; no quality/performance/playable certification.

Archive: `../c3-evidence/index.json` pins actual reports and raw evidence paths; small copies preserve the canonical and original build provenance. Motion3 Wine cold84/final83 is not full equality: coldfd80 had no readable identity; remaining83 identities and active/final agree, while the four real exported DMA_BUF payloads were retired. Static global game FD churn is separate from owned transport resource retirement. Late complete has real model/fence/data evidence but is never consumed after quarantine; abnormal cleanup is not normal FD reclamation.
