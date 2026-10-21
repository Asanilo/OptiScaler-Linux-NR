#!/usr/bin/env python3
"""Workspace housekeeping for the OptiScaler Linux NR development root.

The Git repository lives in this directory; models, SDKs, build receipts,
deployment tools and raw test evidence live in the parent workspace.  This
script only touches the parent workspace and defaults to dry-run for every
destructive action.

Commands
--------
report          Print sizes, duplicate groups and cache candidates.
dedupe          Hardlink byte-identical files under testlogs/ and backups/.
prune-pycache   Remove regenerable __pycache__/*.pyc below this repository.
prune-builds    List (or with --apply --yes remove) stale content-addressed
                build directories while keeping the current build and receipts.
git-gc          Run a normal `git gc` (safe: keeps reflogs and reachable data).

Every applied action writes a JSON receipt under docs/maintenance/ in the
workspace so cleanup stays auditable.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_WORKSPACE = REPO.parent

# Evidence is immutable.  Build receipts and toolchains are excluded because
# hardlinking mutable manifests would make a later edit silently change both
# paths; the savings there are also small.
DEDUPE_ROOTS = ("testlogs", "backups")
SKIP_DIR_NAMES = {".git", "node_modules", "__pycache__"}

# Build directories that are always retained by prune-builds.
KEEP_BUILD_NAMES = {
    "toolchain",
    "eee98821",
    "26aea636",
    "native-shader-compiler",
    "nr-lifetime-prefix",
    "acceptance-controls",
}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def human(n: int) -> str:
    value = float(n)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if value < 1024 or unit == "TiB":
            return f"{value:.1f} {unit}"
        value /= 1024
    return f"{n} B"


def walk_files(root: Path):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
        for name in filenames:
            path = Path(dirpath) / name
            try:
                st = path.lstat()
            except OSError:
                continue
            if path.is_file() and not path.is_symlink():
                yield path, st


def receipt(workspace: Path, command: str, payload: dict) -> Path:
    out = workspace / "docs" / "maintenance"
    out.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    path = out / f"{command}-{stamp}.json"
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    return path


def duplicate_groups(workspace: Path, roots=DEDUPE_ROOTS):
    by_size = defaultdict(list)
    for root_name in roots:
        root = workspace / root_name
        if not root.is_dir():
            continue
        for path, st in walk_files(root):
            if st.st_size >= 4096:
                by_size[st.st_size].append(path)
    groups = []
    for size, paths in by_size.items():
        if len(paths) < 2:
            continue
        by_hash = defaultdict(list)
        for path in paths:
            try:
                by_hash[sha256(path)].append(path)
            except OSError:
                continue
        for digest, same in by_hash.items():
            if len(same) > 1:
                groups.append({"size": size, "sha256": digest, "paths": [str(p) for p in sorted(same)]})
    groups.sort(key=lambda g: g["size"] * (len(g["paths"]) - 1), reverse=True)
    return groups


def cmd_report(args) -> int:
    workspace = args.workspace
    print(f"workspace: {workspace}")
    print(f"repository: {REPO}")
    print("\nTop-level sizes:")
    entries = []
    for child in sorted(workspace.iterdir()):
        if child.name == ".git":
            continue
        try:
            size = sum(st.st_size for _, st in walk_files(child)) if child.is_dir() else child.stat().st_size
        except OSError:
            continue
        entries.append((size, child.name))
    for size, name in sorted(entries, reverse=True):
        print(f"  {human(size):>10}  {name}")

    print("\nDuplicate groups (testlogs/backups, >=4 KiB):")
    groups = duplicate_groups(workspace)
    reclaim = sum(g["size"] * (len(g["paths"]) - 1) for g in groups)
    for g in groups[:12]:
        print(f"  {human(g['size']):>10} x{len(g['paths'])}  {g['paths'][0]}")
        for extra in g["paths"][1:]:
            print(f"  {'':>10}      {extra}")
    if len(groups) > 12:
        print(f"  ... {len(groups) - 12} more groups")
    print(f"  reclaimable by hardlink: {human(reclaim)} across {len(groups)} groups")

    print("\nRegenerable caches:")
    pycache = list(REPO.rglob("__pycache__"))
    pyc_bytes = 0
    for directory in pycache:
        for path, st in walk_files(directory):
            pyc_bytes += st.st_size
    print(f"  __pycache__ dirs: {len(pycache)} ({human(pyc_bytes)})")

    build = workspace / "build"
    if build.is_dir():
        print("\nBuild cache directories (content-addressed receipts):")
        for child in sorted(build.iterdir()):
            if not child.is_dir():
                continue
            size = sum(st.st_size for _, st in walk_files(child))
            manifests = list(child.glob("*manifest*.json"))
            keep = child.name in KEEP_BUILD_NAMES
            print(f"  {human(size):>10}  {child.name:<34} manifests={len(manifests)} keep={keep}")

    print("\nGit objects:")
    try:
        out = subprocess.check_output(["git", "count-objects", "-vH"], cwd=REPO, text=True)
        for line in out.splitlines():
            print(f"  {line}")
    except (OSError, subprocess.CalledProcessError) as exc:
        print(f"  unavailable: {exc}")
    return 0


def cmd_dedupe(args) -> int:
    workspace = args.workspace
    groups = duplicate_groups(workspace)
    reclaim = sum(g["size"] * (len(g["paths"]) - 1) for g in groups)
    print(f"{len(groups)} duplicate groups, {human(reclaim)} reclaimable")
    if not args.apply:
        for g in groups[:10]:
            print(f"  would keep {g['paths'][0]}")
            for extra in g["paths"][1:]:
                print(f"       link {extra}")
        if len(groups) > 10:
            print(f"  ... {len(groups) - 10} more groups")
        print("dry-run: pass --apply to replace duplicate inodes with hardlinks")
        return 0

    linked = []
    errors = []
    for g in groups:
        canonical = Path(g["paths"][0])
        for raw in g["paths"][1:]:
            duplicate = Path(raw)
            try:
                if canonical.stat().st_dev != duplicate.stat().st_dev:
                    errors.append({"path": raw, "error": "cross-device"})
                    continue
                before = sha256(duplicate)
                duplicate.unlink()
                os.link(canonical, duplicate)
                after = sha256(duplicate)
                if after != before:
                    errors.append({"path": raw, "error": "hash changed after link"})
                else:
                    linked.append({"kept": str(canonical), "linked": raw, "size": g["size"], "sha256": after})
            except OSError as exc:
                errors.append({"path": raw, "error": str(exc)})
    payload = {"command": "dedupe", "groups": len(groups), "reclaimable": reclaim,
               "linked": linked, "errors": errors,
               "note": "hardlinked paths are byte-identical; treat evidence as immutable"}
    path = receipt(workspace, "dedupe", payload)
    print(f"linked {len(linked)} paths, reclaimed {human(sum(x['size'] for x in linked))}, errors {len(errors)}")
    print(f"receipt: {path}")
    return 0 if not errors else 1


def cmd_prune_pycache(args) -> int:
    workspace = args.workspace
    targets = []
    for base in (REPO, workspace):
        for directory in base.rglob("__pycache__"):
            if ".git" in directory.parts:
                continue
            targets.append(directory)
    # De-duplicate nested hits from the workspace-wide scan.
    targets = sorted(set(targets))
    files = [(p, st.st_size) for d in targets for p, st in walk_files(d)]
    print(f"{len(targets)} __pycache__ dirs, {len(files)} files, {human(sum(s for _, s in files))}")
    if not args.apply:
        for d in targets[:20]:
            print(f"  would remove {d}")
        print("dry-run: pass --apply to remove regenerable bytecode")
        return 0
    removed = []
    for d in targets:
        try:
            size = sum(st.st_size for _, st in walk_files(d))
            shutil.rmtree(d)
            removed.append({"path": str(d), "bytes": size})
        except OSError as exc:
            print(f"  failed {d}: {exc}")
    path = receipt(workspace, "prune-pycache", {"command": "prune-pycache", "removed": removed})
    print(f"removed {len(removed)} dirs, {human(sum(x['bytes'] for x in removed))}")
    print(f"receipt: {path}")
    return 0


def cmd_prune_builds(args) -> int:
    workspace = args.workspace
    build = workspace / "build"
    if not build.is_dir():
        print("no build directory")
        return 1
    candidates = []
    for child in sorted(build.iterdir()):
        if not child.is_dir() or child.name in KEEP_BUILD_NAMES:
            continue
        manifests = list(child.glob("*manifest*.json"))
        size = sum(st.st_size for _, st in walk_files(child))
        mtime = max((st.st_mtime for _, st in walk_files(child)), default=0)
        candidates.append((mtime, size, child, manifests))
    candidates.sort(reverse=True)
    keep_recent = candidates[: args.keep]
    stale = candidates[args.keep:]
    print(f"keeping {len(keep_recent)} recent scratch dirs plus {len(KEEP_BUILD_NAMES)} pinned names")
    for mtime, size, child, manifests in stale:
        age = (time.time() - mtime) / 86400 if mtime else 0
        print(f"  stale {human(size):>10}  {child.name:<34} age={age:.1f}d manifests={len(manifests)}")
    print(f"stale total: {human(sum(s for _, s, _, _ in stale))}")
    if not args.apply:
        print("dry-run: pass --apply --yes to remove stale scratch directories")
        return 0
    if not args.yes:
        print("refusing to remove build directories without --yes")
        return 2
    removed = []
    for _, size, child, manifests in stale:
        # A directory with a manifest is a build receipt; only remove it when
        # it has no manifest at all (pure scratch) unless --include-receipts.
        if manifests and not args.include_receipts:
            print(f"  skip receipt-bearing {child.name}")
            continue
        shutil.rmtree(child)
        removed.append({"path": str(child), "bytes": size})
    path = receipt(workspace, "prune-builds", {"command": "prune-builds", "removed": removed})
    print(f"removed {len(removed)} dirs, {human(sum(x['bytes'] for x in removed))}")
    print(f"receipt: {path}")
    return 0


def cmd_git_gc(args) -> int:
    if not args.apply:
        print("dry-run: pass --apply to run `git gc` (reflogs and reachable objects kept)")
        subprocess.run(["git", "count-objects", "-vH"], cwd=REPO)
        return 0
    before = subprocess.check_output(["git", "count-objects", "-v"], cwd=REPO, text=True)
    subprocess.run(["git", "gc"], cwd=REPO, check=True)
    after = subprocess.check_output(["git", "count-objects", "-v"], cwd=REPO, text=True)
    path = receipt(args.workspace, "git-gc", {"command": "git-gc", "before": before, "after": after})
    print(f"receipt: {path}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--workspace", type=Path, default=DEFAULT_WORKSPACE)
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser("report", help="print sizes and duplicate/cache candidates")
    p.set_defaults(func=cmd_report)

    p = sub.add_parser("dedupe", help="hardlink byte-identical evidence files")
    p.add_argument("--apply", action="store_true")
    p.set_defaults(func=cmd_dedupe)

    p = sub.add_parser("prune-pycache", help="remove regenerable Python bytecode")
    p.add_argument("--apply", action="store_true")
    p.set_defaults(func=cmd_prune_pycache)

    p = sub.add_parser("prune-builds", help="remove stale scratch build directories")
    p.add_argument("--apply", action="store_true")
    p.add_argument("--yes", action="store_true")
    p.add_argument("--keep", type=int, default=12, help="recent scratch dirs to keep")
    p.add_argument("--include-receipts", action="store_true", help="also remove receipt-bearing dirs")
    p.set_defaults(func=cmd_prune_builds)

    p = sub.add_parser("git-gc", help="compact Git object database")
    p.add_argument("--apply", action="store_true")
    p.set_defaults(func=cmd_git_gc)

    args = parser.parse_args()
    args.workspace = args.workspace.resolve()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
