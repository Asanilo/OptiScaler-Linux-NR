# Workspace maintenance and incremental builds

Updated 2026-10-22. This describes the shared development workspace around the
Git repository (`dlss5_linux/`), what was cleaned, what must never be cleaned,
and the two local tools that make daily work incremental.

## Layout and ownership

| Path | Role | Cleanup policy |
| --- | --- | --- |
| `OptiScaler-Linux-NR/` | Only development repository | Tracked source; keep |
| `OptiScaler-Linux-NR/.worktree/nrbridge` | Current Helper worktree at `713f8ac9` | Keep until Bloodborne migration completes |
| `OptiScaler-Linux-NR/build_out/` | Untracked originals referenced by Helper/forwarder provenance | Keep; do not treat as scratch |
| `dll/` | User-supplied NR/FG/FSR runtimes and source archives (979.8 MiB) | Keep; not regenerable |
| `build/toolchain/` | Locked LLVM-MinGW, Detours, clang-format (606 MiB) | Keep; required by local builds |
| `build/eee98821/` | Current deployed build (DLL + ini + manifest, 25 MiB) | Keep |
| `build/<hash>/`, `build/nr-*/` | Per-iteration receipts and fixtures | Keep receipts; only manifest-less scratch is prunable |
| `build/cache/` | New content-addressed local compile cache | Prunable, regenerated on demand |
| `backups/` | Install manifests, configs and rollback logs (9.4 MiB) | Keep; evidence |
| `testlogs/` | Raw logs, frames and acceptance evidence (1.5 GiB) | Keep; deduplicate only |
| `docs/` | Handoff, state, cleanup and maintenance receipts | Keep |

## What was done on 2026-10-22

- Fixed the Helper source at commit `713f8ac9`, tree `6cdc70ff`, with the
  annotated tag `nrbridge/helper-freeze-20261022`. See
  [`nrbridge-helper-freeze-20261022.md`](nrbridge-helper-freeze-20261022.md).
- Hardlinked 104 byte-identical evidence files across `testlogs/` and
  `backups/` (21 duplicate groups, 112.6 MiB reclaimed). Paths, contents and
  mtimes are unchanged; only the inode is shared. Receipt:
  `docs/maintenance/dedupe-20261021T161536Z.json`.
- Removed 5 regenerable `__pycache__` directories (165.3 KiB). Receipt:
  `docs/maintenance/prune-pycache-20261021T161536Z.json`.
- Compacted Git objects from 16 packs / 137.4 MiB to 3 packs / 134.9 MiB with a
  normal `git gc` (reflogs and all reachable commits kept). Receipt:
  `docs/maintenance/git-gc-20261021T161547Z.json`.
- No branch, tag, worktree, build receipt, install backup or raw test evidence
  was deleted. The 5.2 MiB of stale scratch build directories is receipt-bearing
  and was deliberately retained.

## Daily incremental check

```sh
python3 tools/dev_check.py          # cached CPU tests + Python suites + negative controls
python3 tools/dev_check.py --no-cache
python3 tools/dev_check.py --only nr_pre_upscale_test
python3 tools/dev_check.py --nrbridge   # also run nrbridge/build.py --check-only
```

The cache lives in `build/cache/cpu-tests/<key>/`. The key covers the compiler
identity, flags, include paths, test sources and every `.h/.hpp/.inl/.hlsl`
under `OptiScaler/` and `tests/`. A warm run reuses binaries only after
re-hashing them; a changed header forces a rebuild. Measured on 2026-10-22:
5.6 s cold, 2.1 s warm for five CPU tests plus the Python suites.

This is the local counterpart to the Windows MSVC `Build (Fast)` workflow. It
does not replace the MSVC link check, GPU fixtures or in-game acceptance.

## Workspace maintenance

```sh
python3 tools/workspace_maintenance.py report
python3 tools/workspace_maintenance.py dedupe            # dry-run
python3 tools/workspace_maintenance.py dedupe --apply
python3 tools/workspace_maintenance.py prune-pycache --apply
python3 tools/workspace_maintenance.py prune-builds      # dry-run
python3 tools/workspace_maintenance.py prune-cache       # dry-run
python3 tools/workspace_maintenance.py break-links --apply
python3 tools/workspace_maintenance.py git-gc --apply
```

Rules enforced by the tool:

- `dedupe` only touches `testlogs/` and `backups/`, only files >= 4 KiB, and
  only byte-identical files on the same device. Evidence is treated as
  immutable; do not edit a hardlinked log in place.
- `prune-builds` always keeps `toolchain`, `eee98821`, `26aea636`,
  `native-shader-compiler`, `nr-lifetime-prefix` and `acceptance-controls`, plus
  the 12 most recent scratch directories. Receipt-bearing directories are
  skipped unless `--include-receipts` is passed explicitly.
- `break-links` gives mutable snapshots their own inode. The 2026-10-22 audit
  went further: every remaining hardlink (`.log`, `.png`, `.raw`, `.dll`,
  `.ini`, `.txt`, `.json`) was detached, so no evidence path can be changed in
  place by a rerun or deployment script that happens to reuse a path.
- `dedupe` now defaults to archived, non-rewritable suffixes only: it skips
  `.ini/.txt/.json/.csv/.yaml/.yml/.dll/.exe/.log/.png` and files newer than
  seven days. Use `--allow-suffix` for an explicit exception; do not use it on
  live or regenerated evidence.
- `prune-cache` removes stale content-addressed entries under `build/cache/`
  (`cpu-tests/<key>` and `nr-gpu-*`), keeping the newest four CPU keys and two
  GPU fixture builds and never touching anything younger than 14 days.
- `dll/`, `build_out/`, `.worktree/`, `build/toolchain/` and Git metadata are
  never cleanup targets.

Suggested cadence: `dev_check.py` before every commit; `dedupe --apply` after
adding a new test-evidence batch; `prune-cache --apply` monthly or after a
header-heavy refactor; `report` monthly; `git gc --apply` after large history
imports or branch cleanup.

## Cache retention

- Keep the current deployed build (`build/eee98821/`) and every directory with
  a `*manifest*.json` build receipt.
- Keep the locked toolchain and SDK inputs; they are inputs, not caches.
- Prune only manifest-less scratch directories and `build/cache/`.
- Never prune a directory referenced by `docs/game-references.json`,
  `backups/*/install-manifest.json` or a Helper provenance file.
