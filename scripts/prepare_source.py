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
for name, pin in {
    "thirdparty": "c5b901ecef515ea068fa8b8a19ca5cd5353905cb",
    "ivp": "47533475e01cbff05fbc3bbe8b4edc485f292cea",
}.items():
    actual = subprocess.check_output(["git", "-C", str(args.upstream / name), "rev-parse", "HEAD"], text=True).strip()
    if actual != pin:
        parser.error(f"Source dependency {name} mismatch: expected {pin}, got {actual}; initialize its pinned submodule")
args.output.mkdir(parents=True, exist_ok=True)
for folder in ("public", "common", "tier0", "tier1", "mathlib", "vstdlib", "filesystem", "vpklib", "tier2", "appframework", "engine", "tier3", "bitmap", "utils/lzma/C", "utils/bzip2", "datacache", "studiorender", "vtf", "materialsystem", "vphysics", "ivp"):
    shutil.copytree(args.upstream / folder, args.output / folder, dirs_exist_ok=True)

patch_count = 0
def replace(path, old, new):
    global patch_count
    file = args.output / path
    text = file.read_text(errors="surrogateescape")
    if text.count(old) != 1:
        raise RuntimeError(f"Patch context changed: {path}: {old[:70]}")
    file.write_text(text.replace(old, new), errors="surrogateescape")
    patch_count += 1

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
# A single static executable owns these shared implementations exactly once.
replace("tier0/commandline.cpp", "static CCommandLine g_CmdLine;\nICommandLine *CommandLine()\n{", "ICommandLine *CommandLine()\n{\n\tstatic CCommandLine g_CmdLine; // Initialize before engine global constructors use it.")
replace("engine/sys_dll.cpp", "#include <Carbon/Carbon.h>", "#ifndef SOURCE_IOS\n#include <Carbon/Carbon.h>\n#endif")
replace("engine/sys_dll.cpp", "#elif OSX\n\tstruct mstats memstats = mstats( );", "#elif defined(SOURCE_IOS)\n\tmalloc_statistics_t stats = {};\n\tmalloc_zone_statistics(malloc_default_zone(), &stats);\n\tMsg(\"Allocated %.2f MB, #blocks = %u\\n\", stats.size_in_use / (1024.0 * 1024.0), stats.blocks_in_use);\n#elif OSX\n\tstruct mstats memstats = mstats( );")
replace("engine/sys_dll.cpp", "#endif\n#include <sys/sysctl.h>\n#elif defined(PLATFORM_BSD)", "#endif\n#include <sys/sysctl.h>\n#ifdef SOURCE_IOS\n#include <malloc/malloc.h>\n#endif\n#elif defined(PLATFORM_BSD)")
# Desktop DLLs own some duplicate globals and studio hooks. In one executable
# console variables and studio support have a single owning implementation.
replace("datacache/datacache.cpp", 'ConVar developer( "developer", "0", FCVAR_INTERNAL_USE );', 'extern ConVar developer;')
replace("materialsystem/cmaterialsystem.cpp", 'ConVar mat_debugalttab( "mat_debugalttab", "0", FCVAR_CHEAT );', 'extern ConVar mat_debugalttab;')
replace("datacache/mdlcache.cpp", 'const studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const', '#ifndef SOURCE_ENGINE_PORT\nconst studiohdr_t *studiohdr_t::FindModel( void **cache, char const *pModelName ) const')
replace("datacache/mdlcache.cpp", 'return g_MDLCache.GetStudioHdr( VoidPtrToMDLHandle( cache ) );\n}', 'return g_MDLCache.GetStudioHdr( VoidPtrToMDLHandle( cache ) );\n}\n#endif')
replace("studiorender/studiorendercontext.cpp", 'const vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void *pModelData )', '#ifndef SOURCE_ENGINE_PORT\nconst vertexFileHeader_t * mstudiomodel_t::CacheVertexData( void *pModelData )')
replace("studiorender/studiorendercontext.cpp", 'return g_pStudioDataCache->CacheVertexData( (studiohdr_t *)pModelData );\n}', 'return g_pStudioDataCache->CacheVertexData( (studiohdr_t *)pModelData );\n}\n#endif')
replace("engine/l_studio.cpp", 'Assert( pModelData == NULL );\n\treturn s_ModelRender.CacheVertexData();', 'if (pModelData) return g_pStudioDataCache->CacheVertexData((studiohdr_t*)pModelData);\n\treturn s_ModelRender.CacheVertexData();')
replace("engine/l_studio.cpp", '#include "render_pch.h"', '#include "render_pch.h"\nextern IStudioDataCache* g_pStudioDataCache;')
# Use the original linked headless shader API instead of desktop dylib loading.
replace("materialsystem/cmaterialsystem.cpp", 'm_ShaderHInst = Sys_LoadModule( pShaderDLL );', '#ifdef SOURCE_ENGINE_PORT\n\tif (!Q_stricmp(pShaderDLL, "shaderapiempty")) return Sys_GetFactoryThis();\n#endif\n\tm_ShaderHInst = Sys_LoadModule( pShaderDLL );')
# Shared static tier libraries retain interfaces until their final user disconnects.
replace("tier1/tier1.cpp", 'static bool s_bConnected = false;', 'static unsigned s_nConnections = 0;')
replace("tier1/tier1.cpp", 'if ( s_bConnected )\n\t\treturn;\n\n\ts_bConnected = true;', 'if (s_nConnections++ != 0) return;')
replace("tier1/tier1.cpp", 'if ( !s_bConnected )\n\t\treturn;', 'if (!s_nConnections || --s_nConnections != 0) return;')
replace("tier1/tier1.cpp", '\ts_bConnected = false;', '')
for tier in (2, 3):
    replace(f"tier{tier}/tier{tier}.cpp", f'void ConnectTier{tier}Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )\n{{', f'static unsigned s_nTier{tier}Connections = 0;\nvoid ConnectTier{tier}Libraries( CreateInterfaceFn *pFactoryList, int nFactoryCount )\n{{\n\tif (s_nTier{tier}Connections++ != 0) return;')
    replace(f"tier{tier}/tier{tier}.cpp", f'void DisconnectTier{tier}Libraries()\n{{', f'void DisconnectTier{tier}Libraries()\n{{\n\tif (!s_nTier{tier}Connections || --s_nTier{tier}Connections != 0) return;')
replace("materialsystem/cmaterialsystem.cpp", 'void CMaterialSystem::DestroyShaderAPI()\n{', 'void CMaterialSystem::DestroyShaderAPI()\n{\n\tm_ShaderAPIFactory = NULL;')
replace("materialsystem/cmaterialsystem.cpp", 'int len = Q_strlen( pShaderAPIDLL ) + 1;', 'delete[] m_pShaderDLL;\n\tint len = Q_strlen( pShaderAPIDLL ) + 1;')
# Remember static commands across host reconnects; dynamic objects must not leave
# dangling entries in the pending constructor list or the remembered set.
replace("tier1/convar.cpp", 'static bool s_bRegistered = false;', 'static bool s_bRegistered = false;\nstatic std::vector<ConCommandBase*>* s_PortCommands = nullptr;')
replace("tier1/convar.cpp", '#include "tier1/convar.h"', '#include "tier1/convar.h"\n#include <vector>\n#include <algorithm>')
replace("tier1/convar.cpp", 'ConCommandBase::~ConCommandBase( void )\n{\n}', 'ConCommandBase::~ConCommandBase( void )\n{\n\tif (s_PortCommands) { auto& commands = *s_PortCommands; commands.erase(std::remove(commands.begin(), commands.end(), this), commands.end()); }\n}')
replace("tier1/convar.cpp", 'pNext = pCur->m_pNext;\n\t\tpCur->AddFlags', 'pNext = pCur->m_pNext;\n\t\tif (!s_PortCommands) s_PortCommands = new std::vector<ConCommandBase*>;\n\t\tif (std::find(s_PortCommands->begin(), s_PortCommands->end(), pCur) == s_PortCommands->end()) s_PortCommands->push_back(pCur);\n\t\tpCur->AddFlags')
replace("tier1/convar.cpp", '\tg_pCVar->ProcessQueuedMaterialThreadConVarSets();\n\tConCommandBase::s_pConCommandBases = NULL;', '\tif (s_PortCommands) for (auto* command : *s_PortCommands) if (!command->IsRegistered()) { command->AddFlags(s_nCVarFlag); command->Init(); }\n\tg_pCVar->ProcessQueuedMaterialThreadConVarSets();\n\tConCommandBase::s_pConCommandBases = NULL;')
replace("tier1/convar.cpp", 'if ( !( m_nFlags & FCVAR_UNREGISTERED ) )', 'if ( !s_bRegistered && !( m_nFlags & FCVAR_UNREGISTERED ) )')
replace("public/collisionutils.cpp", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB) || defined(SOURCE_ENGINE_PORT)")
replace("public/dt_recv.cpp", "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL)", "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL) || defined(SOURCE_ENGINE_PORT)")
replace("public/dt_send.cpp", "#if !defined(_STATIC_LINKED) || defined(GAME_DLL)", "#if !defined(_STATIC_LINKED) || defined(GAME_DLL) || defined(SOURCE_ENGINE_PORT)")
replace("mathlib/IceKey.cpp", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB) || defined(SOURCE_ENGINE_PORT)")
shutil.copytree(args.upstream / "thirdparty/stb", args.output / "thirdparty/stb", dirs_exist_ok=True)
(args.output / "port-revision.json").write_text(json.dumps({"upstream": PIN, "patches": patch_count}, indent=2))
print(f"Prepared real Source modules from {PIN}")
