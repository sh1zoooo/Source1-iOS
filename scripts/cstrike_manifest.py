#!/usr/bin/env python3
"""Reproduce the reviewed CS:S source list using the pinned upstream VPC reader."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "third_party/source"
PIN = "ed8209cc35c61fbd8ddff8480962a01c981eef2f"
VPCS = ["server_base.vpc", "server_cstrike.vpc", "nav_mesh.vpc"]


def discover(platform):
    spec = importlib.util.spec_from_file_location(
        "pinned_vpc", UPSTREAM / "scripts/waifulib/vpc_parser.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    env = SimpleNamespace(DEFINES=["POSIX", platform, "PLATFORM_64BITS", "DISABLE_STEAM"],
                          SUBPROJECT_PATH=[str(UPSTREAM / "game/server")])
    data = module.parse_vpcs(env, VPCS, "../..")
    for field in ("sources", "includes"):
        data[field] = [str((UPSTREAM / "game/server" / p).resolve().relative_to(UPSTREAM))
                       for p in data[field]]
    if len(data["sources"]) != len(set(data["sources"])):
        raise RuntimeError("Duplicate game translation units")
    for path in data["sources"]:
        if not (UPSTREAM / path).is_file():
            raise RuntimeError(f"Missing game source: {path}")
    # Match upstream Waf: protection macros conflict with the standard library.
    data["defines"].remove("PROTECTED_THINGS_ENABLE")
    data["defines"] += ["DISABLE_STEAM=1", "NO_STEAM=1"]
    return {"upstream": PIN, "vpcs": ["game/server/" + p for p in VPCS], **data}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="Replace the reviewed manifest")
    args = parser.parse_args()
    actual = subprocess.check_output(["git", "-C", str(UPSTREAM), "rev-parse", "HEAD"], text=True).strip()
    if actual != PIN:
        parser.error(f"Expected upstream {PIN}, found {actual}")
    data = discover("LINUX")
    if data != discover("OSX"):
        raise RuntimeError("Linux and Apple VPC selections differ; audit separately")
    path = ROOT / "cmake/cstrike-files.json"
    if args.write:
        path.write_text(json.dumps(data, indent=2) + "\n")
    elif json.loads(path.read_text()) != data:
        raise RuntimeError("CS:S manifest drift; review upstream selection before --write")
    print(f"CS:S manifest verified: {len(data['sources'])} original translation units; Linux/Apple identical")


if __name__ == "__main__":
    main()
