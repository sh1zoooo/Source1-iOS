#!/usr/bin/env python3
"""Check that the optional compilation stage contains every selected source file."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Build directory containing libsource_*.a")
    parser.add_argument("--ar", default="ar", help="Target archive reader")
    parser.add_argument("--linked", action="store_true", help="Monolithic GameDLL build, with shared implementations owned by engine")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "cmake/cstrike-files.json").read_text())
    counts = {"cstrike_server": len(manifest["sources"]), "particles": 10,
              "dmxloader": 5, "choreoobjects": 5, "soundemittersystem": 3,
              "scenefilecache": 1}
    if args.linked:
        counts["cstrike_server"] -= 13
        counts["soundemittersystem"] -= 1
    for module, expected in counts.items():
        archive = args.directory / f"libsource_{module}.a"
        entries = subprocess.check_output([args.ar, "-t", str(archive)], text=True).splitlines()
        objects = [entry for entry in entries if entry.endswith(".o")]
        if len(objects) != expected:
            raise RuntimeError(f"{archive}: expected {expected} object files, found {len(objects)}")
        print(f"PASS {module}: {len(objects)}/{expected} object files")
    print("Compilation completeness verified. Runtime startup must be validated separately.")


if __name__ == "__main__":
    main()
