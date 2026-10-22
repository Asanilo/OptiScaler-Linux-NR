# PoC branch archive map, 2026-10-22

All `poc/*` branch refs were removed from the local repository and from
`origin`. This file is the short recovery map; historical provenance files keep
their original branch names unchanged.

Final `linux-nr` HEAD at cleanup: `eb14452060e686ab798eaa4deed82ff1fc263d30`.

## Merged branches (no tag; recover from `linux-nr` ancestry)

These tips are ancestors of `linux-nr` and therefore remain reachable without
an extra ref. Recreate a branch only if it is needed for inspection:

```sh
git switch -c <original-name> <tip-sha>
```

| Original branch | Tip SHA | Unique commits |
| --- | --- | --- |
| `poc/game-nr-20261011` | `5ebb2dc037cb5b4e7f85c791ac44cf1a760da2d7` | 0 |
| `poc/nr-runtime-20261010` | `eaada4c2667a26947f6b934008870025e3e1cfb5` | 0 |
| `poc/nr-async-pre-sr-20261014` | `57b62ecc31cb5abbb4cde9225bc034c1caa05a92` | 0 |
| `poc/nr-async-pre-sr-play-20261014` | `f527d8f2f12d474a66bbb4c1e231d6cd9b673476` | 0 |

`poc/nr-runtime-20261010` had no origin branch; the other three remote tips
matched these SHAs and were ancestors of `origin/linux-nr` before deletion.

## Unique branches (lightweight archive tags)

Each tag is a lightweight tag at the branch tip and was pushed to `origin`
before the remote branch was deleted. The tag keeps the full commit ancestry,
including the unique PoC files.

```sh
# inspect without creating a branch
git log --oneline archive/poc/<name>

# recreate the old branch name when needed
git switch -c <original-name> archive/poc/<name>
```

| Original branch | Tip SHA | Archive tag | Unique commits | Unique content |
| --- | --- | --- | --- | --- |
| `poc/nr-async-play-20261014` | `86837c4ae4a24d5655bf200f5955896fa80c333e` | `archive/poc/nr-async-play-20261014` | 1 | `poc/game_nr/async-play/` |
| `poc/nr-pre-sr-game-20261013` | `a2045e2c95a0e2311f8da9d7637070f3735f238f` | `archive/poc/nr-pre-sr-game-20261013` | 1 | `poc/game_nr/pre-sr-hot120/` |
| `poc/nr-sky-antiflicker-20261014` | `1efad4fb5580102a5ec46343a1783b17e065b462` | `archive/poc/nr-sky-antiflicker-20261014` | 1 | `poc/game_nr/sky-antiflicker/` |
| `poc/nr-sky-antiflicker-masked-20261014` | `2a9790164a24796c76f88f7edc9100aa59f98195` | `archive/poc/nr-sky-antiflicker-masked-20261014` | 3 (contains the anti-flicker tip) | `poc/game_nr/sky-once-audit/` and the preceding candidates |

Verified after tagging: `git rev-parse <tag>^{commit}` equals the tip, the
unique directory is readable from the tag, and the remote tag resolves to the
same SHA. No remote-only PoC commit existed at deletion time.

## Not removed

- `linux-nr`
- `dlss-neural-rendering`
- `dev/nrbridge-helper-convergence-20261019` (`713f8ac9`, frozen Helper)
- `ci/linux-nr-build` (`fc876dde`, unique CI workflow)
- `nrbridge/helper-freeze-20261022`
- `origin/baseline/proton-20261009` (frozen remote baseline)
