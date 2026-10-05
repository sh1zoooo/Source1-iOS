#!/usr/bin/env python3
"""Verify C++ and Metal startup on an available iPhone simulator."""
import json
from pathlib import Path
import subprocess
import sys
import time
def simctl(*args):
    return subprocess.check_output(["xcrun", "simctl", *args], text=True).strip()
devices = json.loads(simctl("list", "devices", "available", "--json"))["devices"]
iphones = [d for group in devices.values() for d in group
           if d["name"].startswith("iPhone") and d.get("isAvailable", False)]
if not iphones:
    raise SystemExit("No available iPhone simulator runtime installed")
device = next((d for d in iphones if d["name"] == "iPhone 16e"), iphones[0])
udid = device["udid"]
bundle = "io.github.sh1zoooo.source1ios"
print(f"Simulator: {device['name']} ({udid})", flush=True)
try:
    if device["state"] != "Booted":
        simctl("boot", udid)
    simctl("bootstatus", udid, "-b")
    simctl("install", udid, sys.argv[1])
    simctl("launch", udid, bundle, "--port-smoke")
    container = Path(simctl("get_app_container", udid, bundle, "data"))
    log = container / "Documents" / "Source1IOS" / "runtime.log"
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        text = log.read_text() if log.exists() else ""
        if "First Metal frame completed on GPU" in text and "Source core initialized: tier0/tier1/mathlib/vstdlib" in text and "Source filesystem initialized: filesystem_stdio/vpklib" in text and "Source appframework initialized: CAppSystemGroup" in text and "Source engine linked: dedicated engine" in text and "Source dependencies initialized: materialsystem/shaderapiempty" in text and "Source engine app-system connected and initialized" in text and "Source Host_Init completed: dedicated idle host" in text and "Source host self-test Host_RunFrame idle ticks: PASS" in text and "Source assets/physics self-test physics ragdoll joint under impulse: PASS" in text and "Source BSP preview ready" in text and "Source VTF preview texture uploaded to Metal" in text and "Source simulator runtime contracts: PASS" in text:
            if "FAIL" in text:
                raise RuntimeError(f"Source core self-test failed:\n{text}")
            time.sleep(5)
            text = log.read_text()
            if "Source BSP polygons loaded: 114 triangles from __source1ios_displacement.bsp" not in text:
                raise RuntimeError("Displacement demo was not loaded for the GPU smoke test")
            if "FAIL" in text or text.count(": PASS") != 131:
                raise RuntimeError(f"Expected two sets of 65 Source checks, runtime contracts and a completed GPU frame:\n{text}")
            if "Source studio model loaded: 8 source vertices, 12 triangles, 1 meshes" not in text:
                raise RuntimeError("Static MDL/VVD/VTX model was not staged for Metal")
            simctl("launch", udid, "com.apple.Preferences")
            time.sleep(2)
            simctl("launch", udid, bundle)
            time.sleep(3)
            text = log.read_text()
            if "Host paused" not in text or "Host resumed" not in text or "FAIL" in text:
                raise RuntimeError(f"Foreground/background contracts failed:\n{text}")
            Path("artifacts").mkdir(exist_ok=True)
            Path("artifacts/simulator-runtime.log").write_text(text)
            simctl("io", udid, "screenshot", "artifacts/simulator.png")
            print(text)
            break
        time.sleep(1)
    else:
        raise RuntimeError(f"C++ / Metal startup timed out. Log:\n{text}")
finally:
    Path("artifacts").mkdir(exist_ok=True)
    if "log" in globals() and log.exists():
        Path("artifacts/simulator-runtime.log").write_text(log.read_text())
    crash_directory=Path.home()/"Library/Logs/DiagnosticReports"
    if crash_directory.exists():
        for crash in crash_directory.glob("Source1IOS*.ips"):
            Path("artifacts",crash.name).write_bytes(crash.read_bytes())
    subprocess.run(["xcrun", "simctl", "shutdown", udid], check=False)
