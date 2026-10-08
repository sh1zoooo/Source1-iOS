#!/usr/bin/env python3
"""Verify original ClientMod source coverage, independently of runtime startup."""
import argparse
from collections import Counter
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("directory", type=Path)
parser.add_argument("--ar", default="ar")
parser.add_argument("--ios", action="store_true")
args = parser.parse_args()
manifest = json.loads((ROOT / "cmake/clientmod-files.json").read_text())
for name, module in manifest["modules"].items():
    sources = [p for p in module["source"] if p != "public/tier0/memoverride.cpp"]
    if name == "GameUI":
        sources.extend(["gameui/ModMenu/ClientModMainMenu.cpp", "gameui/ModMenu/ClientModMenuWindow.cpp"])
    if name == "vgui2" and args.ios:
        sources.append("src/ios/ClientModPlatform.mm")
    # CMake's Makefiles generator retains .cpp/.mm in archive members; Xcode
    # uses only the source stem. Verify names and multiplicities in both cases.
    expected = Counter(Path(p).stem for p in sources)
    archive = args.directory / f"libclientmod_{name}.a"
    entries = subprocess.check_output([args.ar, "-t", str(archive)], text=True).splitlines()
    actual = Counter()
    for entry in entries:
        if entry.endswith(".o"):
            stem = Path(entry).stem
            if stem.endswith((".cpp", ".c", ".mm")):
                stem = Path(stem).stem
            actual[stem] += 1
    if actual != expected:
        raise RuntimeError(f"{archive}: missing={dict(expected-actual)}, unexpected={dict(actual-expected)}")
    print(f"PASS {name}: {sum(actual.values())} original source objects and platform adapters")
print("Compilation coverage verified. These archives are not yet active in the IPA.")
