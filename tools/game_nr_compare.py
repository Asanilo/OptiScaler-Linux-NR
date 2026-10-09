#!/usr/bin/env python3
"""Prepare and collect a fixed DS2 NR comparison; default prepare is read-only.

This tool records installation/session evidence, never declares visual or GPU acceptance.
Close the game normally before --apply. The same user-supplied model stays installed.
"""

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

BASELINE = "7b7220bbb4994a9c8ae60cfc75a44cb67995efb8"
PORTED = "26aea636cc4cc8ec4e8425d3526155bccffc149b"
MODEL = "6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927"
ALLOWED = {"dxgi.dll", "nvngx.dll_dlssnr.dll", "nvngx_dlssnr.dll", "OptiScaler.ini"}
REPLACED = ("dxgi.dll", "nvngx.dll_dlssnr.dll", "OptiScaler.ini")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def game_running(proc=Path("/proc")):
    for directory in proc.iterdir():
        if not directory.name.isdecimal():
            continue
        try:
            if (directory / "comm").read_text().strip().lower() == "ds2.exe":
                return True
            # Some Wine loaders keep a generic comm; inspect argv, never print it.
            command = (directory / "cmdline").read_bytes().split(b"\0")
            if any(Path(os.fsdecode(arg)).name.lower() == "ds2.exe" for arg in command if arg):
                return True
        except FileNotFoundError:
            continue  # process exited during enumeration
        except PermissionError as error:
            raise RuntimeError("Cannot inspect processes; refusing to replace game files") from error
    return False


def write_atomic(path, data):
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def configuration(workspace, mode):
    # Both versions use exactly the same configuration for each placement.
    template = workspace / "build/acceptance-controls" / ("pre-sr.ini" if mode == "pre-sr" else "post-sr.ini")
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read_string(template.read_text())
    config["DlssNr"]["Enabled"] = "false" if mode == "off" else "true"
    import io
    output = io.StringIO()
    config.write(output, space_around_delimiters=False)
    return output.getvalue().encode()


def verify_install(manifest):
    game = Path(manifest["game_directory"])
    if not (game / "DS2.exe").is_file():
        raise RuntimeError("This comparison requires the recorded DS2 installation")
    entries = manifest["files"]
    if len(entries) != len(ALLOWED) or {x["name"] for x in entries} != ALLOWED:
        raise RuntimeError("Unexpected or duplicate installation entries")
    for entry in entries:
        target = game / entry["name"]
        if target.is_symlink() or not target.is_file():
            raise RuntimeError(f"Missing, non-file or symlink destination: {entry['name']}")
        if sha256(target) != entry["installed_sha256"]:
            raise RuntimeError(f"Changed since recorded installation: {entry['name']}; review first")
    if sha256(game / "nvngx_dlssnr.dll") != MODEL:
        raise RuntimeError("Model differs from the frozen SF-v2 control")
    return game


def prepare(workspace, manifest_path, version, mode, round_number, apply=False):
    manifest = json.loads(manifest_path.read_text())
    game = verify_install(manifest)
    build = workspace / "build" / ("baseline-7b7220bb" if version == "baseline" else "26aea636")
    build_manifest = json.loads((build / "build-manifest.json").read_text())
    expected_commit = BASELINE if version == "baseline" else PORTED
    if build_manifest["source_commit"] != expected_commit:
        raise RuntimeError("Build provenance differs from frozen comparison")
    source_files = {"dxgi.dll": build / "OptiScaler.dll", "nvngx.dll_dlssnr.dll": build / "nvngx.dll_dlssnr.dll"}
    # Existing build manifests have either a files list or a file mapping.
    recorded = build_manifest["files"]
    recorded = {x["name"]: x for x in recorded} if isinstance(recorded, list) else recorded
    for target, source in source_files.items():
        entry = recorded[source.name]
        expected = entry["sha256"] if isinstance(entry, dict) else entry
        if source.is_symlink() or sha256(source) != expected:
            raise RuntimeError(f"Build file changed: {source.name}")
    ini = configuration(workspace, mode)
    session = workspace / "testlogs/nr-comparison" / f"{version}-{mode}-round-{round_number:02d}"
    planned = {name: sha256(source) for name, source in source_files.items()}
    planned["OptiScaler.ini"] = hashlib.sha256(ini).hexdigest()
    result = {"version": version, "mode": mode, "round": round_number,
              "source_commit": expected_commit, "model_sha256": MODEL,
              "planned_files": planned, "session_directory": str(session),
              "installation_status": "preview", "execution_status": "not started",
              "visual_acceptance": "not tested", "performance_acceptance": "not tested"}
    if not apply:
        return result
    if game_running():
        raise RuntimeError("DS2 is running; save and exit normally before applying")
    if session.exists():
        raise RuntimeError("Session already exists; preserve it and use a new round")
    session.mkdir(parents=True)
    previous = session / "previous-install"
    previous.mkdir()
    (previous / "install-manifest.json").write_bytes(manifest_path.read_bytes())
    for name in REPLACED:
        shutil.copy2(game / name, previous / name)
    # Keep offsets/provenance without deleting the game's existing log.
    log = game / "OptiScaler.log"
    if log.exists():
        data = log.read_bytes()
        (session / "log-before.txt").write_bytes(data)
        result["log_before_sha256"] = hashlib.sha256(data).hexdigest()
    contents = {name: source.read_bytes() for name, source in source_files.items()}
    contents["OptiScaler.ini"] = ini
    for name, data in contents.items():
        if hashlib.sha256(data).hexdigest() != planned[name]:
            raise RuntimeError(f"Source changed during preparation: {name}")
    changed = []
    try:
        # Verify again after backup, before any destination is changed.
        verify_install(manifest)
        if game_running():
            raise RuntimeError("DS2 started while preparing installation")
        for name, data in contents.items():
            write_atomic(game / name, data)
            changed.append(name)
        for entry in manifest["files"]:
            if entry["name"] in planned:
                entry["installed_sha256"] = planned[entry["name"]]
        manifest["build_commit"] = expected_commit
        manifest["installation_status"] = f"comparison {version}/{mode}; results pending"
        manifest["ci_run"] = 37946226862 if version == "baseline" else 37940214870
        manifest["comparison_session"] = str(session)
        write_atomic(manifest_path, (json.dumps(manifest, indent=2) + "\n").encode())
    except BaseException:
        for name in changed:
            write_atomic(game / name, (previous / name).read_bytes())
        if changed:
            write_atomic(manifest_path, (previous / "install-manifest.json").read_bytes())
        result["installation_status"] = "failed; previous files restored"
        (session / "session.json").write_text(json.dumps(result, indent=2) + "\n")
        raise
    result["installation_status"] = "installed; game launch pending"
    (session / "session.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def collect(manifest_path, session, observation):
    manifest = json.loads(manifest_path.read_text())
    if Path(manifest.get("comparison_session", "")).resolve() != session.resolve():
        raise RuntimeError("This session is not the current recorded installation")
    if game_running():
        raise RuntimeError("Close DS2 normally before collecting a completed run")
    verify_install(manifest)
    report_path = session / "session.json"
    report = json.loads(report_path.read_text())
    if report["execution_status"] != "not started":
        raise RuntimeError("Session already collected; preserving its results")
    log = Path(manifest["game_directory"]) / "OptiScaler.log"
    raw = log.read_bytes() if log.exists() else b""
    before_path = session / "log-before.txt"
    before = before_path.read_bytes() if before_path.exists() else b""
    appended = bool(before) and raw.startswith(before)
    run_log = raw[len(before):] if appended else raw
    if not run_log or raw == before:
        raise RuntimeError("No new log evidence; cannot mark this session collected")
    (session / "OptiScaler-run.log").write_bytes(run_log)
    report["execution_status"] = "log collected; controls and actual path require review"
    report["log_sha256"] = hashlib.sha256(run_log).hexdigest()
    report["log_mode"] = "appended" if appended else "new or rotated"
    report["operator_observation"] = observation
    # A human observation and a successful launch do not prove acceptance or attribution.
    report["visual_acceptance"] = "failed: operator reports sustained flicker" if observation == "flicker" else "pending review"
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path(__file__).resolve().parents[2])
    commands = parser.add_subparsers(dest="command", required=True)
    prepare_parser = commands.add_parser("prepare")
    prepare_parser.add_argument("--version", choices=["baseline", "ported"], required=True)
    prepare_parser.add_argument("--mode", choices=["off", "pre-sr", "post-sr"], required=True)
    prepare_parser.add_argument("--round", type=int, choices=[1, 2, 3], required=True)
    prepare_parser.add_argument("--apply", action="store_true")
    collect_parser = commands.add_parser("collect")
    collect_parser.add_argument("session", type=Path)
    collect_parser.add_argument("--observation", choices=["flicker", "no-flicker", "uncertain"], required=True)
    args = parser.parse_args()
    workspace = args.workspace.resolve()
    manifest_path = workspace / "backups/ds2-26aea636/install-manifest.json"
    try:
        if args.command == "prepare":
            result = prepare(workspace, manifest_path, args.version, args.mode, args.round, args.apply)
        else:
            result = collect(manifest_path, args.session.resolve(), args.observation)
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        parser.exit(1, f"{error}\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
