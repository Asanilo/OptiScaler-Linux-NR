"""Wrapper checks never launch Steam or touch a real game installation."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1] / "tools"))
import game_nr_steam as wrapper


class SteamWrapper(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.workspace = Path(self.temporary.name)
        self.session = self.workspace / "case"
        self.session.mkdir()
        self.prepared = {"session_directory": str(self.session),
                         "execution_status": "not started", "visual_acceptance": "not tested"}

    def report(self):
        return json.loads((self.session / "session.json").read_text())

    def test_preparation_failure_never_launches(self):
        with patch.object(wrapper.compare, "prepare", side_effect=RuntimeError("DS2 is running")):
            with patch.object(wrapper.subprocess, "run") as launch:
                with self.assertRaisesRegex(RuntimeError, "DS2 is running"):
                    wrapper.run_case(self.workspace, "baseline", "pre-sr", 1, ["fixture"])
                launch.assert_not_called()

    def test_command_exit_collects_uncertain_without_inferring_success(self):
        for returncode in (0, 7):
            with self.subTest(returncode=returncode):
                with patch.object(wrapper.compare, "prepare", return_value=dict(self.prepared)):
                    with patch.object(wrapper.subprocess, "run", return_value=subprocess.CompletedProcess(["fixture"], returncode)) as launch:
                        with patch.object(wrapper.compare, "collect") as collect:
                            actual = wrapper.run_case(self.workspace, "ported", "off", 1, ["fixture", "two words"])
                            self.assertEqual(actual, returncode)
                            collect.assert_called_once_with(self.workspace / "backups/ds2-26aea636/install-manifest.json", self.session, "uncertain")
                            self.assertEqual(launch.call_args.args[0], ["fixture", "two words"])
                            self.assertEqual(launch.call_args.kwargs["env"]["PROTON_LOG_DIR"], str(self.session))
                            self.assertEqual(self.report()["visual_acceptance"], "not tested")

    def test_collection_refusal_preserves_evidence(self):
        with patch.object(wrapper.compare, "prepare", return_value=dict(self.prepared)):
            with patch.object(wrapper.subprocess, "run", return_value=subprocess.CompletedProcess(["fixture"], 0)):
                with patch.object(wrapper.compare, "collect", side_effect=RuntimeError("Close DS2 normally")):
                    wrapper.run_case(self.workspace, "baseline", "pre-sr", 1, ["fixture"])
        self.assertEqual(self.report()["collection_error"], "Close DS2 normally")
        self.assertEqual(self.report()["execution_status"], "not started")


if __name__ == "__main__":
    unittest.main()
