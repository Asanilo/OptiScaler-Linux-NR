# NRBridge Helper source freeze (2026-10-22)

The NRBridge Helper will be migrated into the Bloodborne repository by a
separate Codex session. This record fixes the exact source that migration must
consume. It is a freeze record, not a rewrite: no Helper source, shader,
forwarder, protocol or dependency was changed for it.

## Frozen source

| Item | Value |
| --- | --- |
| Repository | `Asanilo/OptiScaler-Linux-NR` |
| Branch | `dev/nrbridge-helper-convergence-20261019` |
| Commit | `713f8ac99959fcdf470b2600c2645a1e5b24b7c8` |
| Commit subject | `nrbridge: declare actual daily job capacity and unbounded wall in bootstrap` |
| `nrbridge/` tree | `6cdc70ff18a31609eeba8721ce6d9d08c6665a9e` |
| Freeze tag | `nrbridge/helper-freeze-20261022` (annotated, points at the commit above) |
| Linked worktree | `OptiScaler-Linux-NR/.worktree/nrbridge` |
| Tracked files | 30 (270,924 bytes) |
| Base on `linux-nr` | `99bfd217` (`Use locked data-root SDK inputs after moving Helper into the fork worktree`) |

The branch is already on `origin` at the same commit. The two commits ahead of
`linux-nr` add the bounded daily ring transport and its capacity declaration;
they are part of the current Helper and are included in the freeze.

A per-file manifest with Git blob IDs and SHA-256 values is in
[`nrbridge-helper-freeze-20261022.json`](nrbridge-helper-freeze-20261022.json).

## What the frozen tree contains

- `nrbridge/runtime/`: entry/frame, transport, runtime config, low-resolution
  production adapter, Wine runtime-model helper, fixture utilities.
- `nrbridge/protocol/`: physical protocol sources, effective spec, protocol
  export, `native_runtime_model.h`, `pre_sr_wire.h`.
- `nrbridge/forwarder/`: shutdown bridge, parameter ABI, independent build
  recipe and source provenance.
- `nrbridge/shaders/`: frozen production HLSL and SPIR-V regeneration recipe.
  The production SPIR-V is the committed frozen artifact; do not substitute the
  repository's later HLSL.
- `nrbridge/retirement.py`, `nrbridge/build.py`, `nrbridge/build_support.py`,
  `nrbridge/shaders.py`, and the CPU/protocol/retirement tests.
- `nrbridge/source-provenance.json` and `nrbridge/forwarder/source-provenance.json`
  record imported source hashes and include relocations.

## Migration contract

1. Consume exactly commit `713f8ac9` (or its `nrbridge/` tree
   `6cdc70ff`). Do not migrate from a working-tree copy.
2. Do not delete or rewrite `nrbridge/`, the linked worktree, the frozen
   shader, the forwarder, the protocol export or the provenance JSON.
3. Re-export the protocol for the consuming repository:
   `python3 nrbridge/protocol/export.py --output /absolute/new/protocol-export`.
   Native must consume the exported copies, not include across repositories.
4. Verify before and after migration:
   `python3 -m unittest discover -s nrbridge/tests -p 'test_*.py'` and
   `python3 nrbridge/build.py --check-only`.
5. Locked external inputs must stay available until migration is complete:
   - `/home/arinp/Code/nrbridge-dev/inputs/workspace-return-20261020/{ngx-sdk,nlohmann}`
   - the LLVM-MinGW toolchain under the shared workspace `build/toolchain`
   - the external Vulkan/NGX SDK and JSON headers documented in
     `nrbridge/README.md`.
6. Keep `OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010` until the
   forwarder provenance has been independently re-established; it holds the
   untracked originals referenced by `forwarder/source-provenance.json`.

## Known limits

The freeze covers source and build recipe, not proprietary models, SDKs, game
data or a GPU run. The Helper CPU checks do not prove Windows DLL loading, GPU
behavior, visual quality or long-session stability. The frozen SPIR-V is the
last committed production artifact; HLSL-to-SPIR-V recompilation was recorded
as `NOT_RUN` because no matching Linux DXC was available.
