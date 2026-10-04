#!/usr/bin/env python3
"""Copy pinned upstream modules into the build directory and apply audited patches."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess

PIN = "ed8209cc35c61fbd8ddff8480962a01c981eef2f"
parser = argparse.ArgumentParser()
parser.add_argument("upstream", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
actual = subprocess.check_output(["git", "-C", str(args.upstream), "rev-parse", "HEAD"], text=True).strip()
if actual != PIN:
    parser.error(f"Source revision mismatch: expected {PIN}, got {actual}")
args.output.mkdir(parents=True, exist_ok=True)
for folder in ("public", "common", "tier0", "tier1", "mathlib", "vstdlib", "filesystem", "vpklib", "tier2", "appframework", "utils/lzma/C"):
    shutil.copytree(args.upstream / folder, args.output / folder, dirs_exist_ok=True)

def replace(path, old, new):
    file = args.output / path
    text = file.read_text()
    if text.count(old) != 1:
        raise RuntimeError(f"Patch context changed: {path}: {old[:70]}")
    file.write_text(text.replace(old, new))

# C++11 and later have a real nullptr keyword. Do not redefine it to integer 0.
replace("public/tier0/basetypes.h", "#if !defined(PLATFORM_GLIBC) && defined(LINUX)",
        "#if !defined(PLATFORM_GLIBC) && defined(LINUX) && (!defined(__cplusplus) || __cplusplus < 201103L)")
replace("tier0/assert_dialog.cpp", '#include "pch_tier0.h"',
        '#include "pch_tier0.h"\n#ifdef POSIX\n#include <execinfo.h>\n#endif')
replace("tier1/checksum_md5.cpp", "register unsigned int a, b, c, d;",
        "unsigned int a, b, c, d;")
# On ARM the fast counter counts nanoseconds, not variable CPU clock cycles.
replace("public/tier0/platform.h", "clock_gettime( CLOCK_REALTIME, &t);",
        "clock_gettime( CLOCK_MONOTONIC, &t);")
replace("tier0/cpu_posix.cpp", "uint64 CalculateCPUFreq()\n{",
        "uint64 CalculateCPUFreq()\n{\n#ifdef SOURCE_IOS\n\treturn 1000000000ULL; // Plat_Rdtsc uses nanoseconds on ARM64.\n#endif")
replace("tier0/cpu.cpp", "#elif defined ( __arm__ )",
        "#elif defined ( __arm__ ) || defined(__aarch64__)")
# Correct the baseline Linux timer's fractional subtraction as well.
replace("tier0/platform_posix.cpp", "( now.tv_nsec * 1e-9 )",
        "( (now.tv_nsec - start_time.tv_nsec) * 1e-9 )")
replace("filesystem/filesystem_stdio.cpp", "CFileSystem_Stdio g_FileSystem_Stdio;",
        "CFileSystem_Stdio g_FileSystem_Stdio;\nCreateInterfaceFn SourceFileSystem_GetFactory() { return Sys_GetFactoryThis(); }")
# Embedded VPK offsets must use the actual v1/v2 header size, not always v2.
replace("public/vpklib/packedstore.h", "int m_nDirectoryDataSize;", "int m_nDirectoryDataSize;\n\tint m_nDirectoryHeaderSize;")
replace("vpklib/packedstore.cpp", "m_nDirectoryDataSize = 0;", "m_nDirectoryDataSize = 0;\n\tm_nDirectoryHeaderSize = 0;")
replace("vpklib/packedstore.cpp", "uint32 nSizeOfHeader = dirFile.Tell();", "uint32 nSizeOfHeader = dirFile.Tell();\n\t\t\tm_nDirectoryHeaderSize = nSizeOfHeader;")
replace("vpklib/packedstore.cpp", "m_nDirectoryDataSize + sizeof( VPKDirHeader_t )", "m_nDirectoryDataSize + m_nDirectoryHeaderSize")
# Keep lifecycle rollback correct when an app-system fails partway through startup.
replace("public/appframework/IAppSystemGroup.h", "AppSystemGroupStage_t m_nErrorStage;", "AppSystemGroupStage_t m_nErrorStage;\n\tint m_nConnectedSystems = 0;\n\tint m_nInitializedSystems = 0;")
replace("appframework/AppSystemGroup.cpp", "\t}\n\treturn true;\n}\n\nvoid CAppSystemGroup::DisconnectSystems()", "\t\t++m_nConnectedSystems;\n\t}\n\treturn true;\n}\n\nvoid CAppSystemGroup::DisconnectSystems()")
replace("appframework/AppSystemGroup.cpp", "for (int i = m_Systems.Count(); --i >= 0; )\n\t{\n\t\tm_Systems[i]->Disconnect();\n\t}", "while (m_nConnectedSystems > 0)\n\t{\n\t\tm_Systems[--m_nConnectedSystems]->Disconnect();\n\t}")
replace("appframework/AppSystemGroup.cpp", "\t}\n\treturn INIT_OK;\n}\n\nvoid CAppSystemGroup::ShutdownSystems()", "\t\t++m_nInitializedSystems;\n\t}\n\treturn INIT_OK;\n}\n\nvoid CAppSystemGroup::ShutdownSystems()")
replace("appframework/AppSystemGroup.cpp", "for (int i = m_Systems.Count(); --i >= 0; )\n\t{\n\t\tm_Systems[i]->Shutdown();\n\t}", "while (m_nInitializedSystems > 0)\n\t{\n\t\tm_Systems[--m_nInitializedSystems]->Shutdown();\n\t}")
replace("appframework/AppSystemGroup.cpp", "case PREINITIALIZATION:\n\tcase INITIALIZATION:\n\t\tgoto disconnect;", "case INITIALIZATION:\n\t\tbreak;\n\tcase PREINITIALIZATION:\n\t\tgoto disconnect;")
replace("appframework/AppSystemGroup.cpp", "case CREATION:\n\tcase CONNECTION:\n\t\tgoto destroy;", "case CONNECTION:\n\t\tgoto disconnect;\n\tcase CREATION:\n\t\tgoto destroy;")
replace("appframework/AppSystemGroup.cpp", "\tDestroy();\n}", "\tDestroy();\n\ts_pCurrentAppSystem = GetParent();\n}")
(args.output / "port-revision.json").write_text(json.dumps({"upstream": PIN, "patches": 20}, indent=2))
print(f"Prepared real Source modules from {PIN}")
