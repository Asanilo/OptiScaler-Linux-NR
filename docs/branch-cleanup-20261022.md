# Branch and worktree cleanup, 2026-10-22

Scope: `Asanilo/OptiScaler-Linux-NR` only. No branch, tag, worktree or build
copy was created for archival; every removal is recorded here by tip SHA.

> Final follow-up on the same day removed every remaining `poc/*` branch. See
> the authoritative recovery table in
> [`poc-branch-archive-20261022.md`](poc-branch-archive-20261022.md).

## Removed

| Ref | Tip | Reason |
| --- | --- | --- |
| `dev/sky-sync-20261022` (local + origin) | `48e4c5798b8e1e92a3a6de5110a5b433e2337e58` | 0 commits ahead of `linux-nr`; merged via `e69470d4`; worktree removed first |
| `poc/nr-highfps-cache-20261013` (local + origin) | `12199b80662871a4a0d210a8e89ae37c44a5865f` | fully merged into `linux-nr`; no unique commit and no document/provenance reference |

Both tips remain reachable: the sky-sync tip is the second parent of merge
`e69470d4`, and the high-FPS PoC tip is an ancestor of `linux-nr`. They can be
recovered by SHA without a branch.

The `dev/sky-sync` worktree was removed manually after `git worktree remove`
refused because it contained the `external/spdlog` submodule. It was clean
(only ignored `build/` and `__pycache__/`), and `git worktree prune` removed its
admin metadata. This reclaimed about 290 MiB of duplicate checkout.

## Kept

| Branch | Tip | Unique vs `linux-nr` | Reason |
| --- | --- | --- | --- |
| `linux-nr` | `91d76f56` | 0 | stable development line, current NR/FG/Linux/Vulkan work |
| `dlss-neural-rendering` | `7b7220bb` | 0 | default mainline baseline, tracked by origin |
| `dev/nrbridge-helper-convergence-20261019` | `713f8ac9` | 2 | frozen Helper source and its worktree; tag `nrbridge/helper-freeze-20261022` |
| `ci/linux-nr-build` | `fc876dde` | 1 | unmerged Linux NR CI workflow; local only |

## PoC branches (final state)

All eight `poc/*` refs were removed locally and from `origin` after the
recovery check. The four merged tips are ancestors of `linux-nr`; the four
unique tips have lightweight `archive/poc/*` tags. Exact SHAs, tags and
recovery commands are in
[`poc-branch-archive-20261022.md`](poc-branch-archive-20261022.md).
Historical provenance keeps the old branch names unchanged.

`origin/baseline/proton-20261009` still points at `7b7220bb`, the same commit as
`dlss-neural-rendering`. It is retained as the frozen remote baseline named in
the handoff documents.

## Remaining structure

- Worktrees: 2
  - `OptiScaler-Linux-NR` on `linux-nr`
  - `OptiScaler-Linux-NR/.worktree/nrbridge` on the frozen Helper branch
- Local branches: 4 (`linux-nr`, `dlss-neural-rendering`,
  `dev/nrbridge-helper-convergence-20261019`, `ci/linux-nr-build`).
- Origin branches: 4 (the same minus the local-only CI branch), plus the
  frozen `baseline/proton-20261009` reference and origin's symbolic `HEAD`.
- PoC recovery refs: 4 lightweight tags under `archive/poc/`; no archive
  branch, extra worktree or full build copy was created.

## Hardlink audit

The earlier dedupe had linked 112.6 MiB of evidence. All remaining hardlinks
were detached on 2026-10-22 because logs, PNGs and DLL snapshots can be
rewritten in place by a rerun or a deployment tool, and one in-place write
would silently change every linked path.

- Before the final detach: 97 hardlinked files (`.log`, `.png`, `.raw`, `.dll`,
  `.ini`, `.txt`, `.json`).
- After: no hardlinked file remains under `testlogs/` or `backups/`.
- Receipts: `docs/maintenance/break-links-20261022T024450Z.json` and
  `break-links-20261022T024507Z.json`.
- `dedupe` now defaults to archived, non-rewritable suffixes only, skips
  `.ini/.txt/.json/.csv/.yaml/.yml/.dll/.exe/.log/.png` and files newer than
  seven days. `--allow-suffix` is an explicit opt-in and should not be used on
  live evidence.
