"""Check diagnostic preservation on a simulated launch timeout, not GPU output."""
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
from unittest.mock import patch

script = Path(__file__).resolve().parents[1] / "scripts/simulator_smoke.py"
previous = Path.cwd()
with tempfile.TemporaryDirectory(prefix="source-smoke-diagnostics-", dir=previous) as temporary:
    root = Path(temporary)
    log = root / "Documents/Source1IOS/runtime.log"
    log.parent.mkdir(parents=True)
    evidence = "Diagnostic-preservation test only; not engine or GPU validation.\n"
    log.write_text(evidence)
    commands = []

    def fake_simctl(command, **kwargs):
        action = command[2]
        commands.append(action)
        if action == "list":
            return json.dumps({"devices": {"runtime": [{"name": "iPhone 16e", "udid": "test", "state": "Booted", "isAvailable": True}]}})
        if action == "get_app_container":
            return str(root)
        if action == "launch":
            raise subprocess.TimeoutExpired(command, kwargs["timeout"])
        return ""

    try:
        os.chdir(root)
        with patch.object(sys, "argv", [str(script), "test.app"]), patch.object(subprocess, "check_output", fake_simctl), patch.object(subprocess, "run"), patch.object(Path, "home", return_value=root):
            try:
                runpy.run_path(str(script), run_name="__main__")
            except subprocess.TimeoutExpired:
                pass
            else:
                raise AssertionError("Launch timeout was incorrectly hidden")
        assert commands.index("get_app_container") < commands.index("launch")
        assert (root / "artifacts/simulator-runtime.log").read_text() == evidence
    finally:
        os.chdir(previous)
print("Launch-timeout diagnostic preservation passed (mocked simctl, no GPU claim)")
