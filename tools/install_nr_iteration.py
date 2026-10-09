#!/usr/bin/env python3
"""Preview/apply a verified iteration to the recorded DS2 installation.

This is an offline installation tool, never a Steam launch wrapper. It retains
the same SF-v2 model and creates a separate rollback snapshot for this iteration.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil

from game_nr_compare import MODEL, REPLACED, game_running, sha256, verify_install, write_atomic


def install(build, manifest_path, backup, apply=False):
    manifest_raw = manifest_path.read_bytes()
    manifest = json.loads(manifest_raw)
    game = verify_install(manifest)
    provenance = json.loads((build / "build-manifest.json").read_text())
    commit = provenance["source_commit"]
    if not re.fullmatch(r"[0-9a-f]{40}", commit) or provenance.get("conclusion") != "success":
        raise RuntimeError("A successful build with a full source commit is required")
    contents = {}
    sources = {"dxgi.dll": build / "OptiScaler.dll", "nvngx.dll_dlssnr.dll": build / "nvngx.dll_dlssnr.dll",
               "OptiScaler.ini": build / "OptiScaler.ini"}
    for name, source in sources.items():
        if source.is_symlink() or not source.is_file():
            raise RuntimeError(f"Missing or symlink source: {source.name}")
        data = source.read_bytes()
        if hashlib.sha256(data).hexdigest() != provenance["files"][source.name]["sha256"]:
            raise RuntimeError(f"Source hash changed: {source.name}")
        contents[name] = data
    result = {"source_commit": commit, "ci_run": provenance["ci_run"], "model_sha256": MODEL,
              "game_directory": str(game), "rollback_directory": str(backup),
              "planned_files": {name: hashlib.sha256(data).hexdigest() for name, data in contents.items()},
              "acceptance": "pending actual game observation, long session and performance", "applied": False}
    if not apply:
        return result
    if backup.exists() or game_running():
        raise RuntimeError("Backup already exists or DS2 is running; preserving installation")
    backup.mkdir(parents=True)
    previous = {name: (game / name).read_bytes() for name in REPLACED}
    rollback = copy.deepcopy(manifest)
    for entry in rollback["files"]:
        entry["original_sha256"] = entry["installed_sha256"]
        shutil.copy2(game / entry["name"], backup / entry["name"])
    (backup / "install-manifest.before.json").write_bytes(manifest_raw)
    log = game / "OptiScaler.log"
    if log.is_file():
        shutil.copy2(log, backup / "OptiScaler-before.log")
    try:
        if game_running():
            raise RuntimeError("DS2 started during preparation")
        verify_install(manifest)
        for name in REPLACED:
            write_atomic(game / name, contents[name])
        for entry in manifest["files"]:
            if entry["name"] in sources:
                entry["source"] = str(sources[entry["name"]])
                entry["installed_sha256"] = result["planned_files"][entry["name"]]
        manifest.update(build_commit=commit, ci_run=provenance["ci_run"],
                        installation_status="iteration installed; game acceptance pending",
                        iteration_backup=str(backup), comparison_session=None)
        for entry in rollback["files"]:
            entry["installed_sha256"] = next(e["installed_sha256"] for e in manifest["files"] if e["name"] == entry["name"])
        rollback.update(build_commit=commit, installation_status="iteration rollback snapshot")
        write_atomic(backup / "install-manifest.json", (json.dumps(rollback, indent=2) + "\n").encode())
        verify_install(manifest)
        write_atomic(manifest_path, (json.dumps(manifest, indent=2) + "\n").encode())
        result["applied"] = True
    except BaseException:
        for name, data in previous.items():
            write_atomic(game / name, data)
        write_atomic(manifest_path, manifest_raw)
        result["failure"] = "Previous installation restored; preserve backup for review"
        (backup / "iteration.json").write_text(json.dumps(result, indent=2) + "\n")
        raise
    (backup / "iteration.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("backup", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(install(args.build.resolve(), args.manifest.resolve(), args.backup.resolve(), args.apply), indent=2))
    except (RuntimeError, OSError, KeyError, ValueError) as error:
        parser.exit(1, f"Stopped: {error}\n")


if __name__ == "__main__":
    main()
