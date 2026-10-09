#!/usr/bin/env python3
"""Preview or roll back a recorded test install without removing changed files."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("backup", type=Path, help="directory containing install-manifest.json")
    parser.add_argument("--apply", action="store_true", help="perform rollback; default is preview")
    args = parser.parse_args()
    manifest_path = args.backup / "install-manifest.json"
    manifest = json.loads(manifest_path.read_text())
    game = Path(manifest["game_directory"])
    allowed = {"dxgi.dll", "nvngx.dll_dlssnr.dll", "nvngx_dlssnr.dll", "OptiScaler.ini"}
    entries = manifest["files"]
    if len({entry["name"] for entry in entries}) != len(entries):
        raise SystemExit("Duplicate destinations in manifest")
    actions = []
    for entry in entries:
        name = entry["name"]
        if name not in allowed:
            raise SystemExit(f"Unexpected destination: {name}")
        target = game / name
        if target.is_symlink():
            raise SystemExit(f"Refusing symlink: {name}")
        original = entry["original_sha256"]
        current = sha256(target) if target.exists() else None
        if current == original:
            print(f"Already restored: {name}")
            continue
        if current != entry["installed_sha256"]:
            raise SystemExit(f"Changed since installation: {name}; preserving all files, review manually")
        saved = args.backup / name
        if original is not None and (not saved.is_file() or sha256(saved) != original):
            raise SystemExit(f"Original backup missing or changed: {name}")
        actions.append((target, saved if original is not None else None))
        print(f"{'Restore original' if original is not None else 'Remove test file'}: {name}")
    if args.apply:
        for target, saved in actions:
            if saved is None:
                target.unlink()
            else:
                shutil.copy2(saved, target)
        manifest["installation_status"] = "rolled back"
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    else:
        print("Preview only. Add --apply to perform these actions after closing the game.")


if __name__ == "__main__":
    main()
