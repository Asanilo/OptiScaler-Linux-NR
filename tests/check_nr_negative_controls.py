#!/usr/bin/env python3
"""Mutation controls for the production NR orchestration, in a disposable directory."""

import argparse
import json
from pathlib import Path
import resource
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    header = (root / "OptiScaler/dlssnr/NrPreUpscale.h").read_text()
    changes = {
        "omit_restore": ("if (_substituted)", "if (false)"),
        "wrong_setter_type": ("if (_typed)", "if (false)"),
        "ignore_master_toggle": ("return enabled && mode !=", "return (enabled || true) && mode !="),
        "post_after_pre": ("postAllowed && !preRequested", "postAllowed"),
        "post_after_failed_sr": ("result == Success && postAllowed", "postAllowed"),
    }
    for old, new in changes.values():
        if header.count(old) != 1:
            raise SystemExit(f"Mutation target is no longer unique: {old}")
    variants = {"unchanged": header, **{name: header.replace(old, new, 1) for name, (old, new) in changes.items()}}
    report = {"scope": "production parameter/NR-SR orchestration; no vendor NGX or GPU coverage", "variants": {}}
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    with tempfile.TemporaryDirectory(prefix="nr-contract-mutations-") as temporary:
        folder = Path(temporary)
        target = folder / "OptiScaler/dlssnr/NrPreUpscale.h"
        target.parent.mkdir(parents=True)
        tests = folder / "tests"
        tests.mkdir()
        shutil.copy2(root / "tests/nr_pre_upscale_test.cpp", tests)
        for name, content in variants.items():
            target.write_text(content)
            compilation = subprocess.run([args.compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                                          str(tests / "nr_pre_upscale_test.cpp"), "-o", str(folder / "test")],
                                         capture_output=True, text=True)
            if compilation.returncode:
                raise SystemExit(f"{name} did not compile; this is not a valid negative control:\n{compilation.stderr}")
            test = subprocess.run([str(folder / "test")], capture_output=True, text=True)
            report["variants"][name] = {"exit_code": test.returncode, "output": (test.stdout + test.stderr).strip()}
            if (test.returncode == 0) != (name == "unchanged"):
                raise SystemExit(f"Unexpected control result: {name}, exit={test.returncode}")
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print("Production contract passed; all five intentional defects were detected.")


if __name__ == "__main__":
    main()
