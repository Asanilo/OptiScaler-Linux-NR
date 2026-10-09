#!/usr/bin/env python3
"""Run production NR fixtures in a dedicated Proton prefix, never start a game.

Requires the fixture's own report and expected result; launcher success alone is
not a pass. Evidence/output directories must be new, preserving previous failures.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ("build", "output", "proton", "prefix", "steam"):
        parser.add_argument("--" + flag, type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    build, output = args.build.resolve(), args.output.resolve()
    if output.exists():
        raise SystemExit("Use a new evidence directory")
    if not args.prefix.resolve().is_relative_to(repo.parent / "build"):
        raise SystemExit("Use a dedicated fixture prefix under the workspace build directory")
    provenance = json.loads((build / "gpu-build-manifest.json").read_text())
    for path, expected in provenance["source_sha256"].items():
        if sha(repo / path) != expected:
            raise SystemExit("Source changed since fixture build: " + path)
    for entry in provenance["files"]:
        if sha(build / entry["name"]) != entry["sha256"]:
            raise SystemExit("Fixture binary changed: " + entry["name"])
    output.mkdir(parents=True)
    (output / "gpu-build-manifest.json").write_text(json.dumps(provenance, indent=2) + "\n")
    cases = [
        ("cache", "nr_cache_gpu.exe", [], "PASS: production Sky cache", 0),
        ("break-fresh", "nr_cache_gpu.exe", ["--break-fresh"], "FAIL: cached edit uses fresh game frame", 1),
        ("break-regional", "nr_cache_gpu.exe", ["--break-regional"], "FAIL: pre-SR low-only temporal path", 1),
        ("break-jitter", "nr_cache_gpu.exe", ["--break-jitter"], "FAIL: jitter shifts edit across boundary", 1),
        ("lifetime-runtime", "nr_lifetime_runtime_gpu.exe", ["--runtime"], "PASS: actual Reset/Release/Execute/Signal", 0),
        ("unsafe-reuse", "nr_lifetime_runtime_gpu.exe", ["--unsafe-reuse"], "FAIL:", 1),
        ("unsafe-retire", "nr_lifetime_runtime_gpu.exe", ["--unsafe-retire"], "FAIL:", 1),
        ("legacy-stabilizer", "nr_stabilizer_gpu.exe", [], "PASS: production shader", 0),
    ]
    results = []
    for name, binary, flags, expected, expected_exit in cases:
        folder = output / name
        folder.mkdir()
        report = folder / "report.txt"
        win_report = "Z:" + str(report)
        if binary == "nr_cache_gpu.exe":
            arguments = ["--report", win_report] + flags
        elif binary == "nr_stabilizer_gpu.exe":
            arguments = [win_report]
        else:
            arguments = flags + [win_report]
        env = os.environ.copy()
        env.update(STEAM_COMPAT_DATA_PATH=str(args.prefix.resolve()),
                   STEAM_COMPAT_CLIENT_INSTALL_PATH=str(args.steam.resolve()),
                   __NV_PRIME_RENDER_OFFLOAD="1", __VK_LAYER_NV_optimus="NVIDIA_only",
                   WINEDEBUG="-all", PROTON_LOG="1", PROTON_LOG_DIR=str(folder), SteamAppId="0", SteamGameId="0")
        with (folder / "launcher.txt").open("w") as launcher:
            completed = subprocess.run([str(args.proton.resolve()), "run", str(build / binary)] + arguments,
                                       env=env, stdout=launcher, stderr=subprocess.STDOUT, timeout=180)
        text = report.read_text(errors="replace") if report.exists() else ""
        ok = (completed.returncode == expected_exit and expected in text and "NVIDIA" in text and
              ("FAIL:" not in text if expected_exit == 0 else "PASS:" not in text))
        result = {"case": name, "exit_code": completed.returncode, "expected_exit": expected_exit,
                  "expected_report": expected, "passed": ok, "binary_sha256": sha(build / binary),
                  "report_sha256": sha(report) if report.exists() else None}
        results.append(result)
        (output / "results.json").write_text(json.dumps({"cases": results,
            "all_expected_outcomes_detected": len(results) == len(cases) and all(r["passed"] for r in results),
            "limits": "Controlled NR input. Vendor NGX, game routing, appearance, FG and FPS acceptance not tested."}, indent=2) + "\n")
        print(json.dumps(result), flush=True)
        if not ok:
            print(text[-4000:], flush=True)
            raise SystemExit("Unexpected outcome in " + name)


if __name__ == "__main__":
    main()
