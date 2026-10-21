#!/usr/bin/env python3
"""Daily incremental CPU regression check.

Compiles the host-side NR tests with the system C++ compiler, caches the
resulting binaries by content (sources + headers + compiler + flags), runs the
Python suites and the NR negative-control audit.  Unchanged inputs reuse the
cached binary instead of recompiling, which is the local equivalent of the
Windows CI build for day-to-day work.

Usage:
    python3 tools/dev_check.py                 # incremental, all CPU + Python checks
    python3 tools/dev_check.py --no-cache      # force recompilation
    python3 tools/dev_check.py --only nr_pre_upscale_test
    python3 tools/dev_check.py --nrbridge      # also run nrbridge/build.py --check-only
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
CACHE = REPO / "build" / "cache" / "cpu-tests"
CXX = os.environ.get("CXX", "g++")
BASE_FLAGS = ["-std=c++17", "-Wall", "-Wextra", "-Werror",
              "-fsanitize=address,undefined", "-g", "-O1"]
INCLUDES = ["-I" + str(REPO), "-I" + str(REPO / "tests"),
            "-I" + str(REPO / "OptiScaler"), "-I" + str(REPO / "OptiScaler" / "include")]

TESTS = [
    ("nr_pre_upscale_test", ["tests/nr_pre_upscale_test.cpp"], []),
    ("nr_motion_jitter_test", ["tests/nr_motion_jitter_test.cpp"], []),
    ("nr_cache_cadence_test", ["tests/nr_cache_cadence_test.cpp"], []),
    ("nr_gpu_timing_test", ["tests/nr_gpu_timing_test.cpp"], []),
    ("nr_lifetime_test", ["tests/nr_lifetime_test.cpp"], []),
]

HEADER_SUFFIXES = {".h", ".hpp", ".inl", ".hlsl"}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def compiler_identity() -> str:
    try:
        out = subprocess.check_output([CXX, "--version"], text=True, stderr=subprocess.STDOUT)
    except OSError as exc:
        raise SystemExit(f"compiler not available: {CXX}: {exc}")
    return out.splitlines()[0]


def header_digest() -> str:
    h = hashlib.sha256()
    roots = [REPO / "tests", REPO / "OptiScaler"]
    for root in roots:
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix in HEADER_SUFFIXES and ".git" not in path.parts:
                h.update(str(path.relative_to(REPO)).encode())
                h.update(b"\0")
                h.update(sha256_file(path).encode())
                h.update(b"\n")
    return h.hexdigest()


def cache_key(name: str, sources, extra, hdrs: str, cc: str) -> str:
    h = hashlib.sha256()
    h.update(cc.encode())
    h.update(b"\0")
    h.update(hdrs.encode())
    h.update(b"\0")
    h.update(" ".join(BASE_FLAGS + INCLUDES + extra).encode())
    for source in sources:
        h.update(str(source).encode())
        h.update(b"\0")
        h.update(sha256_file(REPO / source).encode())
        h.update(b"\n")
    return h.hexdigest()[:24]


def build_one(name: str, sources, extra, hdrs: str, cc: str, use_cache: bool):
    key = cache_key(name, sources, extra, hdrs, cc)
    target = CACHE / key / name
    meta = CACHE / key / "meta.json"
    if use_cache and target.is_file() and meta.is_file():
        try:
            recorded = json.loads(meta.read_text())
        except (OSError, json.JSONDecodeError):
            recorded = {}
        if recorded.get("sha256") == sha256_file(target):
            return target, "cache"
    CACHE.mkdir(parents=True, exist_ok=True)
    tmpdir = Path(tempfile.mkdtemp(prefix="nrbuild-", dir=str(REPO / "build" / "cache")))
    binary = tmpdir / name
    command = [CXX] + BASE_FLAGS + INCLUDES + extra + [str(REPO / s) for s in sources] + ["-o", str(binary)]
    started = time.time()
    print(f"[build] {name}: {' '.join(command[:4])} ...")
    proc = subprocess.run(command, cwd=REPO, text=True, capture_output=True)
    if proc.returncode != 0:
        print(proc.stdout)
        print(proc.stderr)
        shutil.rmtree(tmpdir, ignore_errors=True)
        raise SystemExit(f"compile failed: {name}")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.move(str(binary), str(target))
    meta.write_text(json.dumps({
        "name": name, "sources": sources, "extra": extra,
        "compiler": cc, "key": key, "sha256": sha256_file(target),
        "seconds": round(time.time() - started, 2),
        "command": command,
    }, indent=2) + "\n")
    shutil.rmtree(tmpdir, ignore_errors=True)
    return target, "built"


def run_binary(path: Path) -> bool:
    env = dict(os.environ)
    env["ASAN_OPTIONS"] = env.get("ASAN_OPTIONS", "detect_leaks=0")
    print(f"[run] {path.name}")
    proc = subprocess.run([str(path)], cwd=REPO, env=env, text=True)
    return proc.returncode == 0


def run_python() -> bool:
    print("[run] python unittest discover")
    ok = subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py"],
                        cwd=REPO).returncode == 0
    print("[run] NR negative controls")
    ok = subprocess.run([sys.executable, "tests/check_nr_negative_controls.py"], cwd=REPO).returncode == 0 and ok
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--no-cache", action="store_true", help="force recompilation")
    parser.add_argument("--only", action="append", default=[], help="run only the named CPU test(s)")
    parser.add_argument("--nrbridge", action="store_true", help="also run nrbridge/build.py --check-only")
    parser.add_argument("--skip-python", action="store_true")
    args = parser.parse_args()

    cc = compiler_identity()
    hdrs = header_digest()
    print(f"compiler: {cc}")
    print(f"header digest: {hdrs[:16]}")
    selected = [t for t in TESTS if not args.only or t[0] in args.only]
    if args.only:
        missing = set(args.only) - {t[0] for t in TESTS}
        if missing:
            raise SystemExit(f"unknown test(s): {', '.join(sorted(missing))}")

    failures = []
    started = time.time()
    for name, sources, extra in selected:
        try:
            binary, how = build_one(name, sources, extra, hdrs, cc, not args.no_cache)
        except SystemExit as exc:
            failures.append(f"{name}: {exc}")
            continue
        print(f"[{how}] {name}")
        if not run_binary(binary):
            failures.append(f"{name}: runtime failure")

    if not args.skip_python:
        if not run_python():
            failures.append("python suite")

    if args.nrbridge:
        print("[run] nrbridge --check-only")
        if subprocess.run([sys.executable, "nrbridge/build.py", "--check-only"], cwd=REPO).returncode != 0:
            failures.append("nrbridge check-only")

    elapsed = time.time() - started
    if failures:
        print(f"\nFAILED ({elapsed:.1f}s):")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print(f"\nPASS: {len(selected)} CPU tests" + ("" if args.skip_python else " + Python suites") + f" ({elapsed:.1f}s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
