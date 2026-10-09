#!/usr/bin/env python3
"""Capture source selections from the pinned ClientMod Waf scripts."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

PIN = "5f8bea18c72467601d7919ba86d57b06e5e2d677"
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "third_party/clientmod"

def discover(platform="linux", graphics=False):
    actual = subprocess.check_output(["git", "-C", str(SOURCE), "rev-parse", "HEAD"], text=True).strip()
    if actual != PIN:
        raise RuntimeError(f"ClientMod revision mismatch: {actual}")
    sys.modules["waflib"] = SimpleNamespace(Utils=SimpleNamespace(), Configure=SimpleNamespace())
    spec = importlib.util.spec_from_file_location("vpc_parser", SOURCE / "scripts/waifulib/vpc_parser.py")
    vpc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(vpc)
    sys.modules["vpc_parser"] = vpc
    result = {"upstream": PIN, "modules": {}}
    folders = ("engine", "materialsystem", "materialsystem/shaderapidx9", "togles",
               "inputsystem", "launcher", "video", "datamodel", "appframework") if graphics else (
               "vgui2/vgui_controls", "vgui2/matsys_controls", "vgui2/src", "gameui", "game/client",
               "vgui2/vgui_surfacelib", "vguimatsurface", "materialsystem/stdshaders")
    for folder in folders:
        env = SimpleNamespace(DEST_OS=platform, GAMES="cstrike", PREFIX="", LIBDIR="", MSVC_SUBSYSTEM="",
                              INCLUDES_SDL2=[], INCLUDES_FC=[], INCLUDES_FT2=[],
                              DEFINES=["POSIX", "OSX" if platform == "darwin" else "LINUX", "PLATFORM_64BITS", "DISABLE_STEAM", "USE_SDL"],
                              SUBPROJECT_PATH=[str(SOURCE / folder)], SDL=True, DEDICATED=False,
                              GL=True, TOGLES=True)
        captured = []
        bld = SimpleNamespace(env=env, stlib=lambda **kw: captured.append(kw),
                              shlib=lambda **kw: captured.append(kw), get_taskgen_count=lambda: 1)
        namespace = {}
        exec(compile((SOURCE / folder / "wscript").read_text(), folder+"/wscript", "exec"), namespace)
        namespace["build"](bld)
        if len(captured) != 1:
            raise RuntimeError(f"Unexpected Waf outputs: {folder}")
        data = captured[0]
        normalized = {}
        for field in ("source", "includes"):
            normalized[field] = [str((SOURCE / folder / p).resolve().relative_to(SOURCE)) for p in data[field]]
        normalized["source"] = list(dict.fromkeys(normalized["source"]))
        for p in normalized["source"]:
            if not (SOURCE / p).is_file():
                raise RuntimeError(f"Missing original translation unit: {p}")
        normalized["defines"] = data["defines"]
        normalized["dependencies"] = data["use"]
        result["modules"][data["name"]] = normalized
    return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--graphics", action="store_true")
    args = parser.parse_args()
    data = discover("ios" if args.graphics else "linux", args.graphics)
    darwin = data if args.graphics else discover("darwin")
    for name, module in data["modules"].items():
        for field in ("source", "includes", "defines"):
            if module[field] != darwin["modules"][name][field]:
                raise RuntimeError(f"Darwin source selection needs its own manifest: {name}/{field}")
    path = ROOT / "cmake" / ("clientmod-graphics-files.json" if args.graphics else "clientmod-files.json")
    if args.write:
        path.write_text(json.dumps(data, indent=2)+"\n")
    elif json.loads(path.read_text()) != data:
        raise RuntimeError("ClientMod source manifest drift")
    for name, module in data["modules"].items():
        print(f"{name}: {len(module['source'])} original translation units")
