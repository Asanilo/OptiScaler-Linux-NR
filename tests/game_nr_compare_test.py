"""Failure-path tests use disposable installations, never the real game directory."""

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("compare", Path(__file__).parents[1] / "tools/game_nr_compare.py")
compare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compare)


class ComparisonInstall(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.workspace = Path(self.temporary.name)
        self.game = self.workspace / "game"
        self.game.mkdir()
        (self.game / "DS2.exe").write_bytes(b"fixture exe")
        for name in compare.ALLOWED:
            (self.game / name).write_bytes(("old " + name).encode())
        self.model = patch.object(compare, "MODEL", compare.sha256(self.game / "nvngx_dlssnr.dll"))
        self.model.start()
        self.addCleanup(self.model.stop)
        self.process = patch.object(compare, "game_running", return_value=False)
        self.process.start()
        self.addCleanup(self.process.stop)
        self.manifest_path = self.workspace / "backups/ds2-26aea636/install-manifest.json"
        self.manifest_path.parent.mkdir(parents=True)
        self.manifest = {
            "game_directory": str(self.game),
            "files": [{"name": name, "installed_sha256": compare.sha256(self.game / name),
                       "original_sha256": None} for name in sorted(compare.ALLOWED)]}
        self.manifest_path.write_text(json.dumps(self.manifest))
        self.before = self.snapshot()
        for label, commit in [("baseline-7b7220bb", compare.BASELINE), ("26aea636", compare.PORTED)]:
            folder = self.workspace / "build" / label
            folder.mkdir(parents=True)
            files = []
            for name in ["OptiScaler.dll", "nvngx.dll_dlssnr.dll"]:
                p = folder / name
                p.write_bytes((label + name).encode())
                files.append({"name": name, "sha256": compare.sha256(p)})
            (folder / "build-manifest.json").write_text(json.dumps({"source_commit": commit, "files": files}))
        controls = self.workspace / "build/acceptance-controls"
        controls.mkdir()
        for name, pre in [("pre-sr", "true"), ("post-sr", "false")]:
            (controls / (name + ".ini")).write_text("[DlssNr]\nEnabled=true\nPreUpscale=" + pre + "\nPasses=1\n")

    def snapshot(self):
        return {name: (self.game / name).read_bytes() for name in compare.ALLOWED}

    def prepare(self, apply=False, mode="post-sr", round_number=1):
        return compare.prepare(self.workspace, self.manifest_path, "baseline", mode, round_number, apply)

    def test_preview_has_no_writes_and_no_acceptance(self):
        result = self.prepare()
        self.assertEqual(self.before, self.snapshot())
        self.assertFalse((self.workspace / "testlogs").exists())
        self.assertEqual(result["visual_acceptance"], "not tested")

    def test_running_game_is_preserved(self):
        with patch.object(compare, "game_running", return_value=True):
            with self.assertRaisesRegex(RuntimeError, "DS2 is running"):
                self.prepare(True)
        self.assertEqual(self.before, self.snapshot())

    def test_game_starting_during_backup_does_not_write(self):
        real_write = compare.write_atomic
        writes = []
        def observe(path, data):
            writes.append(path)
            return real_write(path, data)
        with patch.object(compare, "game_running", side_effect=[False, True]):
            with patch.object(compare, "write_atomic", side_effect=observe):
                with self.assertRaisesRegex(RuntimeError, "started while preparing"):
                    self.prepare(True)
        self.assertEqual(writes, [])
        self.assertEqual(self.before, self.snapshot())

    def test_changed_install_or_build_is_preserved(self):
        (self.game / "OptiScaler.ini").write_bytes(b"user changed setting")
        changed = self.snapshot()
        with self.assertRaisesRegex(RuntimeError, "Changed since"):
            self.prepare(True)
        self.assertEqual(changed, self.snapshot())
        (self.game / "OptiScaler.ini").write_bytes(self.before["OptiScaler.ini"])
        (self.workspace / "build/baseline-7b7220bb/OptiScaler.dll").write_bytes(b"changed build")
        with self.assertRaisesRegex(RuntimeError, "Build file changed"):
            self.prepare(True)
        self.assertEqual(self.before, self.snapshot())

    def test_partial_failure_restores_previous_files_and_manifest(self):
        previous_manifest = self.manifest_path.read_bytes()
        real_write = compare.write_atomic
        failed = False
        def fail_once(path, data):
            nonlocal failed
            if path == self.game / "nvngx.dll_dlssnr.dll" and not failed:
                failed = True
                raise OSError("injected disk failure")
            return real_write(path, data)
        with patch.object(compare, "write_atomic", side_effect=fail_once):
            with self.assertRaisesRegex(OSError, "injected"):
                self.prepare(True)
        self.assertEqual(self.before, self.snapshot())
        self.assertEqual(previous_manifest, self.manifest_path.read_bytes())

    def test_apply_retains_model_and_updates_manifest(self):
        result = self.prepare(True, "off")
        installed = json.loads(self.manifest_path.read_text())
        compare.verify_install(installed)
        self.assertEqual(self.before["nvngx_dlssnr.dll"], (self.game / "nvngx_dlssnr.dll").read_bytes())
        self.assertIn(b"Enabled=false", (self.game / "OptiScaler.ini").read_bytes())
        self.assertEqual(result["visual_acceptance"], "not tested")
        with self.assertRaisesRegex(RuntimeError, "Session already exists"):
            self.prepare(True, "off")

    def test_collect_rejects_stale_log_and_never_infers_a_pass(self):
        (self.game / "OptiScaler.log").write_bytes(b"previous run\n")
        result = self.prepare(True)
        session = Path(result["session_directory"])
        with self.assertRaisesRegex(RuntimeError, "No new log"):
            compare.collect(self.manifest_path, session, "no-flicker")
        (self.game / "OptiScaler.log").write_bytes(b"previous run\nnew run\n")
        collected = compare.collect(self.manifest_path, session, "no-flicker")
        self.assertEqual((session / "OptiScaler-run.log").read_bytes(), b"new run\n")
        self.assertEqual(collected["visual_acceptance"], "pending review")
        self.assertEqual(collected["performance_acceptance"], "not tested")
        with self.assertRaisesRegex(RuntimeError, "already collected"):
            compare.collect(self.manifest_path, session, "flicker")

    def test_collect_failure_is_recorded_after_rotated_log(self):
        (self.game / "OptiScaler.log").write_bytes(b"old log")
        result = self.prepare(True)
        session = Path(result["session_directory"])
        (self.game / "OptiScaler.log").write_bytes(b"new shorter log")
        collected = compare.collect(self.manifest_path, session, "flicker")
        self.assertEqual(collected["visual_acceptance"], "failed: operator reports sustained flicker")


if __name__ == "__main__":
    unittest.main()
