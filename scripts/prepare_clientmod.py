#!/usr/bin/env python3
"""Prepare pinned ClientMod code with explicit, checked platform adaptations."""
import argparse
import fcntl
import os
from pathlib import Path
import shutil
import subprocess

PIN = "5f8bea18c72467601d7919ba86d57b06e5e2d677"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("upstream", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("sdk", type=Path)
args = parser.parse_args()
actual = subprocess.check_output(["git", "-C", str(args.upstream), "rev-parse", "HEAD"], text=True).strip()
if actual != PIN:
    parser.error(f"ClientMod revision mismatch: {actual}")
args.output.mkdir(parents=True, exist_ok=True)
lock = (args.output / ".prepare.lock").open("w")
fcntl.flock(lock, fcntl.LOCK_EX)
paths = ["gameui/ModMenu/ClientModMenuWindow.cpp", "game/client/cdll_client_int.cpp",
         "game/shared/gamerules_register.h", "vgui2/src/system_posix.cpp",
         "game/shared/cstrike/achievements_cs.cpp"]
previous = {}
for path in paths:
    file = args.output / path
    if file.exists():
        previous[path] = (file.read_bytes(), file.stat().st_mtime_ns)
for folder in ("game", "gameui", "vgui2", "common", "public"):
    shutil.copytree(args.upstream / folder, args.output / folder, dirs_exist_ok=True)
# ClientMod extends this interface with its original filled radar polygon method.
# Preserve that interface for the paired ClientMod surface implementation.
surface = Path("VGuiMatSurface/IMatSystemSurface.h")
(args.output / "compat" / surface.parent).mkdir(parents=True, exist_ok=True)
shutil.copy2(args.upstream / "public" / surface, args.output / "compat" / surface)

def replace(path, old, new, count=1):
    file = args.output / path
    text = file.read_text()
    if text.count(old) != count:
        raise RuntimeError(f"ClientMod patch context changed: {path}: {old[:70]!r}")
    file.write_text(text.replace(old, new))

# The public fork includes a header which does not exist and is never used.
replace(paths[0], '#include "ClientModInfo.h"',
        '// Public upstream references an absent, unused ClientModInfo.h.')
replace(paths[2], "#if !defined(_STATIC_LINKED)",
        "#if !defined(_STATIC_LINKED) || defined(SOURCE_ENGINE_PORT)")
# Use the SDK's actual touch ABI adapter, then ClientMod's original ProcessEvent.
marker = "void CHLClient::IN_TouchEvent("
original = (args.upstream / paths[1]).read_text()
sdk = (args.sdk / paths[1]).read_text()
if original.count(marker) != 1 or sdk.count(marker) != 1:
    raise RuntimeError("ClientMod touch adapter function context changed")
# Both pinned versions end with this function. Verify that invariant explicitly.
old_adapter = original[original.index(marker):]
new_adapter = sdk[sdk.index(marker):]
if not old_adapter.rstrip().endswith("}") or not new_adapter.rstrip().endswith("}"):
    raise RuntimeError("ClientMod touch adapter tail changed")
replace(paths[1], old_adapter, new_adapter)
replace(paths[1], "IN_TouchEvent( uint data, uint data2, uint data3, uint data4 )",
        "IN_TouchEvent( int data, int data2, int data3, int data4 )")

# Match callback definitions to their existing NO_STEAM declaration guard.
replace(paths[4], "CAchievement_Meta::CAchievement_Meta() :\n\tm_CallbackUserAchievement( this, &CAchievement_Meta::Steam_OnUserAchievementStored )",
        "CAchievement_Meta::CAchievement_Meta()\n#ifndef NO_STEAM\n\t: m_CallbackUserAchievement( this, &CAchievement_Meta::Steam_OnUserAchievementStored )\n#endif")
replace(paths[4], "void CAchievement_Meta::Steam_OnUserAchievementStored(",
        "#ifndef NO_STEAM\nvoid CAchievement_Meta::Steam_OnUserAchievementStored(")
replace(paths[4], "void CAchievement_Meta::AddRequirement(",
        "#endif\n\nvoid CAchievement_Meta::AddRequirement(")

# Retain the original SDL clipboard implementation on iOS; Carbon is macOS-only.
replace(paths[3], "#ifdef OSX", "#if defined(OSX) && !defined(SOURCE_IOS)", count=9)
# UIApplication opens URLs on iOS, where spawning desktop shell tools is invalid.
replace(paths[3], 'using namespace vgui;',
        'using namespace vgui;\n#ifdef SOURCE_IOS\nextern "C" void ClientModIOSOpenURL(const char *url);\n#endif')
replace(paths[3], '\tpid_t pid = fork();',
        '#ifdef SOURCE_IOS\n\tClientModIOSOpenURL(file);\n#else\n\tpid_t pid = fork();')
replace(paths[3], '\t\tAssert( !"execlp failed" );\n\t}\n}',
        '\t\tAssert( !"execlp failed" );\n\t}\n#endif\n}')

# Reconfiguration must not recompile unchanged patched translation units.
for path, (data, modified) in previous.items():
    file = args.output / path
    if file.read_bytes() == data:
        os.utime(file, ns=(modified, modified))
print("ClientMod: checked touch ABI, game-rules macro and iOS system adapters")
