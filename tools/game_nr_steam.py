#!/usr/bin/env python3
"""Steam launch wrapper for one frozen comparison, with automatic log collection.

Usage: python3 game_nr_steam.py baseline pre-sr 1 -- %command%
The operator launches and exits the game. Visual results remain uncertain until
reported separately; this script never interprets a successful exit as a pass.
"""

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
import sys

import game_nr_compare as compare


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def run_case(workspace, version, mode, round_number, command):
    manifest_path = workspace / "backups/ds2-26aea636/install-manifest.json"
    prepared = compare.prepare(workspace, manifest_path, version, mode, round_number, apply=True)
    session = Path(prepared["session_directory"])
    report_path = session / "session.json"
    environment = os.environ.copy()
    environment.update({"WINEDLLOVERRIDES": "dxgi=n,b",
                        "__NV_PRIME_RENDER_OFFLOAD": "1",
                        "__VK_LAYER_NV_optimus": "NVIDIA_only",
                        "PROTON_LOG": "1", "PROTON_LOG_DIR": str(session)})
    prepared.update({"steam_wrapper_started_at": timestamp(),
                     "launch_control": "operator launches through Steam; wrapper collects logs",
                     "observation_timing": "operator paced; timed acceptance not assumed"})
    report_path.write_text(json.dumps(prepared, indent=2) + "\n")
    # The per-case transcript also records refusal to collect if Steam returns
    # while the actual game is still running. Never kill the game or switch then.
    with (session / "launcher.log").open("w") as transcript:
        try:
            result = subprocess.run(command, env=environment, stdout=transcript,
                                    stderr=subprocess.STDOUT, check=False)
        except OSError as error:
            prepared["launcher_error"] = str(error)
            report_path.write_text(json.dumps(prepared, indent=2) + "\n")
            raise
        report = json.loads(report_path.read_text())
        report.update({"steam_command_returned_at": timestamp(),
                       "steam_command_returncode": result.returncode})
        report_path.write_text(json.dumps(report, indent=2) + "\n")
        try:
            compare.collect(manifest_path, session, "uncertain")
        except (RuntimeError, OSError, ValueError, KeyError) as error:
            report = json.loads(report_path.read_text())
            report["collection_error"] = str(error)
            report_path.write_text(json.dumps(report, indent=2) + "\n")
            print(f"Collection requires review: {error}", file=transcript)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", choices=["baseline", "ported"])
    parser.add_argument("mode", choices=["off", "pre-sr", "post-sr"])
    parser.add_argument("round", type=int, choices=[1, 2, 3])
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command or command == ["%command%"]:
        parser.error("Use this wrapper in Steam launch options with -- %command%")
    try:
        return run_case(Path(__file__).resolve().parents[2], args.version,
                        args.mode, args.round, command)
    except (RuntimeError, OSError, ValueError, KeyError) as error:
        print(f"Comparison not launched: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
