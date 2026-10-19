# Standalone NRBridge runtime-model Helper

`nrbridge/` owns the current Helper entry/frame, minimal transport and Vulkan adapter closure, protocol specification, CPU protocol/retirement tests, shutdown shim, and independent build recipe. The imported production behavior is unchanged: immutable origin720 Encode/Evaluate/Resolve, existing tuple/sidecar and parameter versions, strict drain/rebuild ACK, actual GPU fences and shared/FD retirement. `source-provenance.json` records the precise imported sources and include relocations. The Native implementation now delegates to this repository; its removed duplicate sources remain in Git history. Legacy source copies remain in Git; old Helper/forwarder binaries were retired during the 2026-10-19 current-only cleanup. Only the selected current runtime set is deployed.

```bash
python3 nrbridge/build.py --check-only
python3 nrbridge/build.py
python3 nrbridge/protocol/export.py --output /absolute/new/protocol-export
```

The build uses the locked LLVM MinGW compiler, external Vulkan/NGX SDK and nlohmann JSON headers. `--llvm-root`, `--vulkan-include`, `--ngx-sdk-root` and `--json-root` independently select prepared roots containing the same locked bytes; missing or altered locked inputs are rejected before compiler invocation. No SDK, model, system library, old `poc` tree or frozen closure is copied. Source/dependency discovery and generated headers stay under `build/convergence/runtime-model-prepare`; the PE, host test binaries/logs and full manifest are written to a new content address under `build/helper-runtime-model`. The identity covers Helper/protocol/build/test sources, compiler/frontend/linker, actual transitive headers, persistent link libraries, generation inputs, commands, and the original explicit runtime defaults. Existing complete outputs are verified, and incomplete/different outputs are preserved and refused.

The build compiles/runs wire, runtime-model and runtime-config CPU checks, runs the copied retirement and builder tests, validates the locked SPIR-V and compiles the PE. These checks do not load a Windows DLL or prove GPU behavior. Run the CPU tests directly with `python3 -m unittest discover -s nrbridge/tests -p 'test_*.py'`.

`nrbridge/protocol` is the physical protocol source. The export contains the byte-identical `native_runtime_model.h`, the wire parser with only a local generated-header include, the unchanged effective JSON and its deterministically generated `pre_sr_spec.h`. `protocol-export.json` lists versions and SHA256 values. Native consumes the exported copies and records their hashes; it does not directly include files across repositories. No ABI/wire/sidecar field has changed.

Optional resource selection extends the existing CLI:

```text
wine_runtime_model.exe MODE RUN_DIR_WINDOWS SESSION [--runtime-config RUNTIME_JSON_WINDOWS]
```

The optional JSON must have exactly five string fields: `schema: "nrbridge-helper-runtime-v1"`, `model`, `forwarder`, `ngx_dir`, `ge_files`. The four paths are absolute canonical ASCII Linux paths; the first two select specific DLLs. Helper maps them through the existing Wine `Z:` mapping. The core is `ngx_dir/_nvngx.dll`; NVAPI and DXGI derive from the selected GE `files` directory. Helper still requires the exact NVAPI enable/override settings and checks `NVIDIA_WINE_DLL_DIR` and `WINEDLLPATH` against the selected directories. Runner verifies the selected DLL hashes using runtime-only pins before launch; resource selection never enters job/model wire tuples. Omitting this option preserves all current default paths and versions. The manifest records those defaults without packaging private binaries.

The regular build uses the exact committed production SPIR-V (`7b44aeb...`), validates it with `spirv-val`, and regenerates a C header with byte-identical array contents. `shaders/dlssnr-frozen.hlsl` preserves the narrow historical source at the last SPIR-V update (`c3f70dc7`), while the repository's current HLSL has later motion-offset changes and is deliberately not substituted. The original DXC flags and generation recipe are recorded. `shaders/regenerate.py` can verify an explicitly supplied native Linux DXC by SHA and requires its output to reproduce the frozen SPIR-V; no suitable Linux DXC was available in this validation. HLSL→SPIR-V recompilation is therefore **NOT_RUN**. No Wine, DXC download, shader upgrade, runtime DLL installation or default selection change occurs.

Integration validation on 2026-10-19 used source `cea5160aab380272dfe52ab2db31691e77531c18`, PE content address `7f69c1e556810b2b`, independent forwarder `9a8251430ccfa68a`, and Native baseline `563580452e46ea253cecbf75c24ac65579ee031b`. Explicit pinned runtime inputs completed Helper off/skip and persistent/skip (24 jobs each), then 24 actual Evaluate jobs (23 captured, one obsolete), with normal GPU/FD/Helper retirement. The original build manifest remains a build receipt, not a rewritten GPU result. Run evidence lives in the private development root; the current engineering plan is in the Native repository `docs/NRBRIDGE_PLAN.md`. This limited integration regression does not prove visual quality, hot performance, unrestricted long sessions, or resolution of historical GPU faults.

## Workspace and portability status (2026-10-20)

The Helper has been merged into the existing `linux-nr` long-term branch; the daily linked worktree uses `dev/nrbridge-helper-convergence-20261019`; this Helper is built independently of Native game sources. Native consumes a versioned protocol export, and runtime communication still requires the matched protocol and actual GPU/FD retirement. This is source/build separation, not removal of the Helper from the NR runtime.

The build's default LLVM-MinGW, Vulkan/NGX SDK and JSON roots still refer to prepared directories in the local Code workspace. Paths can be selected with the documented options, but the locked contents must match. Output roots remain local absolute paths. A fresh clone on another machine needs those inputs prepared; proprietary models/SDKs and game data are not supplied by this Git repository. Native's `tools/nr_dev/README.md` lists the complete local input layout.

The local `OptiScaler-Linux-NR/.worktree/nrbridge` is a linked Git worktree whose Git metadata lives in `dlss5_linux/OptiScaler-Linux-NR/.git`. Do not delete that original repository while this worktree is in use. The old nr-runtime worktree was removed normally; its SDK input path and unique source files remain preserved. The current build defaults now use the byte-verified SDK and JSON roots in `/home/arinp/Code/nrbridge-dev/inputs/workspace-return-20261020/{ngx-sdk,nlohmann}`; the compiler and Vulkan sysroot remain locked external inputs. This is not a live legacy build environment. Native/Helper implementation and runtime protocol are unchanged by this directory move.
