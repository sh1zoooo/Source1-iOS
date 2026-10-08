#!/usr/bin/env python3
"""Copy pinned upstream modules into the build directory and apply audited patches."""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import os
import fcntl

PIN = "ed8209cc35c61fbd8ddff8480962a01c981eef2f"
parser = argparse.ArgumentParser()
parser.add_argument("upstream", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--cstrike", action="store_true", help="Prepare the optional CS:S server compilation stage")
args = parser.parse_args()
# Reentrant CMake checks must not interleave copying and patching one output.
args.output.mkdir(parents=True, exist_ok=True)
prepare_lock = (args.output / ".prepare.lock").open("w")
fcntl.flock(prepare_lock, fcntl.LOCK_EX)
# Reconfiguration must not rebuild the whole SDK when audited patches are
# unchanged. Remember previously prepared files before copying original input.
patched_paths = set(re.findall(r'replace(?:_all)?\("([^"\n]+)"', Path(__file__).read_text()))
patched_paths.update(("public/dispcoll_preview.h", "public/dispcoll_preview.cpp",
                      "tier2/tier2.cpp", "tier3/tier3.cpp",
                      "game/shared/gamemovement.cpp", "game/shared/weapon_parse.cpp",
                      "game/shared/props_shared.cpp", "game/shared/mapentities_shared.cpp"))
previous = {}
for path in patched_paths:
    file = args.output / path
    if file.is_file():
        previous[path] = (file.read_bytes(), file.stat().st_mtime_ns)
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

if args.cstrike:
    for folder in ("game", "particles", "dmxloader", "choreoobjects", "soundemittersystem", "scenefilecache", "utils/common"):
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

def replace_all(path, old, new, expected):
    global patch_count
    file = args.output / path
    text = file.read_text(errors="surrogateescape")
    if text.count(old) != expected:
        raise RuntimeError(f"Patch count changed: {path}: expected {expected}")
    file.write_text(text.replace(old, new), errors="surrogateescape")
    patch_count += expected

# Friction snapshots are queried by player stress damage after simulation.
# IVP clears its temporary solver pointer at the end of impact processing;
# retain the actual computed surface normal with the persistent contact point.
replace("ivp/ivp_intern/ivp_friction.hxx",
        "    IVP_U_Float_Point last_contact_point_ws;",
        "    IVP_U_Float_Point last_contact_point_ws;\n    IVP_U_Float_Point last_surface_normal_ws;")
replace("ivp/ivp_intern/ivp_friction.cxx",
        "IVP_Contact_Point::IVP_Contact_Point( IVP_Mindist *md)\n{",
        "IVP_Contact_Point::IVP_Contact_Point( IVP_Mindist *md)\n{\n    last_surface_normal_ws.set_to_zero();")
replace("ivp/ivp_intern/ivp_mindist_friction.cxx",
        "    this->last_contact_point_ws.set( &info->contact_point_ws );",
        "    this->last_contact_point_ws.set( &info->contact_point_ws );\n    this->last_surface_normal_ws.set( &info->surf_normal );")
replace("ivp/ivp_intern/ivp_friction.cxx",
        "\t*normal = friction_handle->tmp_contact_info->surf_normal;",
        "\t*normal = friction_handle->last_surface_normal_ws;")

# The server uses studio metadata and collision, not desktop shader flags.
# Empty shaderapi materials have no compiled shader variables to inspect.
replace("engine/modelloader.cpp",
        "\tcase MDLCACHE_STUDIOHWDATA:\n\t\tComputeModelFlags( pModel, handle );",
        "\tcase MDLCACHE_STUDIOHWDATA:\n#ifndef SWDS\n\t\tComputeModelFlags( pModel, handle );\n#endif")

if args.cstrike:
    # Some imported maps contain a clip brush without a usable physics solid.
    # Source's stock entity dereferences the failed initialization on round cleanup.
    replace("game/server/bmodels.cpp",
            "\tCreateVPhysics();\n\tVPhysicsGetObject()->EnableCollisions( !m_bDisabled );",
            "\tCreateVPhysics();\n\tif (!VPhysicsGetObject()) { Warning(\"func_clip_vphysics missing collision solid: %s\\n\", STRING(GetModelName())); UTIL_Remove(this); return; }\n\tVPhysicsGetObject()->EnableCollisions( !m_bDisabled );")
    # The old monolithic Xbox branch concatenates an unexpanded function macro.
    # This stage contains one server game; use the ordinary unique class name.
    replace("game/shared/gamerules_register.h",
            "#if !defined(_STATIC_LINKED)",
            "#if !defined(_STATIC_LINKED) || defined(SOURCE_ENGINE_PORT)")
    # The context declarations are already guarded by NO_STEAM upstream;
    # match their lifecycle calls to the same offline configuration.
    for operation in ("Init", "Clear"):
        replace("game/server/gameinterface.cpp",
                f"#ifndef _X360\n\ts_SteamAPIContext.{operation}();",
                f"#if !defined(_X360) && !defined(NO_STEAM)\n\ts_SteamAPIContext.{operation}();")
    # Account-based vote history cannot operate without Steam identities.
    replace("game/server/vote_controller.cpp",
            "void CVoteController::TrackVoteCaller( CBasePlayer *pPlayer )\n{",
            "void CVoteController::TrackVoteCaller( CBasePlayer *pPlayer )\n{\n#ifdef NO_STEAM\n\treturn;\n#else")
    replace("game/server/vote_controller.cpp",
            "m_VoteCallers.Insert( steamID.ConvertToUint64(), gpGlobals->curtime + sv_vote_creation_timer.GetInt() );\n};",
            "m_VoteCallers.Insert( steamID.ConvertToUint64(), gpGlobals->curtime + sv_vote_creation_timer.GetInt() );\n#endif\n};")
    replace("game/server/vote_controller.cpp",
            "bool CVoteController::CanEntityCallVote( CBasePlayer *pPlayer, int &nCooldown )\n{",
            "bool CVoteController::CanEntityCallVote( CBasePlayer *pPlayer, int &nCooldown )\n{\n#ifdef NO_STEAM\n\tnCooldown = 0;\n\treturn false; // No authenticated identity in the offline port.\n#else")
    replace("game/server/vote_controller.cpp",
            "m_VoteCallers.Remove( iIdx );\n\t}\n\n\treturn true;\n};",
            "m_VoteCallers.Remove( iIdx );\n\t}\n\n\treturn true;\n#endif\n};")
    # Keep local money statistics; skip account-keyed market reporting offline.
    replace("game/server/cstrike/cs_gamestats.cpp",
            "\t\tIncrementStat(pPlayer, CSSTAT_MONEY_SPENT, moneySpent);\n\t\tif",
            "\t\tIncrementStat(pPlayer, CSSTAT_MONEY_SPENT, moneySpent);\n#ifndef NO_STEAM\n\t\tif")
    replace("game/server/cstrike/cs_gamestats.cpp",
            "m_MarketPurchases.AddToTail( new SMarketPurchases( steamIDForBuyer.ConvertToUint64(), moneySpent, pItemName ) );\n\t\t}",
            "m_MarketPurchases.AddToTail( new SMarketPurchases( steamIDForBuyer.ConvertToUint64(), moneySpent, pItemName ) );\n\t\t}\n#endif")
    # The edict change-info pointer is process-owned in a monolithic build.
    replace("game/server/gameinterface.cpp",
            "CSharedEdictChangeInfo *g_pSharedChangeInfo = NULL;",
            "extern CSharedEdictChangeInfo *g_pSharedChangeInfo;")
    replace("game/server/gameinterface.cpp",
            "IChangeInfoAccessor *CBaseEdict::GetChangeAccessor()\n{",
            "#ifndef SOURCE_GAME_LINK\nIChangeInfoAccessor *CBaseEdict::GetChangeAccessor()\n{")
    replace("game/server/gameinterface.cpp",
            "\nconst char *GetHintTypeDescription( CAI_Hint *pHint );",
            "\n#endif // SOURCE_GAME_LINK owns edict accessors in engine\nconst char *GetHintTypeDescription( CAI_Hint *pHint );")
    replace("game/shared/baseplayer_shared.cpp",
            "float IntervalDistance( float x, float x0, float x1 )",
            "static float IntervalDistance( float x, float x0, float x1 )")
    # The embedded GameDLL has no loadable module handle. Retain its original
    # plugin-helper interface; desktop third-party DLL plugins cannot load here.
    replace("engine/sv_plugin.cpp",
            "void CServerPlugin::LoadPlugins()\n{",
            "void CServerPlugin::LoadPlugins()\n{\n#ifdef SOURCE_GAME_LINK\n\tm_PluginHelperCheck = (IPluginHelpersCheck*)Sys_GetFactoryThis()(INTERFACEVERSION_PLUGINHELPERSCHECK, NULL);\n\treturn;\n#endif")
    # Static game systems survive DLLShutdown; do not add the same singleton
    # twice when the embedded host is started again in the same process.
    replace("game/shared/igamesystem.cpp",
            "void IGameSystem::Add( IGameSystem* pSys )\n{",
            "void IGameSystem::Add( IGameSystem* pSys )\n{\n#ifdef SOURCE_GAME_LINK\n\tif (s_GameSystems.Find(pSys) != s_GameSystems.InvalidIndex()) return;\n#endif")
    replace("game/shared/steamworks_gamestats.cpp",
            "\tif ( steamapicontext && steamapicontext->SteamUtils() )\n\t\treturn steamapicontext->SteamUtils()->GetServerRealTime();\n\telse",
            "#ifndef NO_STEAM\n\tif ( steamapicontext && steamapicontext->SteamUtils() )\n\t\treturn steamapicontext->SteamUtils()->GetServerRealTime();\n\telse\n#endif")
    for path, expected in (("game/shared/gamemovement.cpp", 3),
                           ("game/shared/weapon_parse.cpp", 2),
                           ("game/shared/props_shared.cpp", 3),
                           ("game/shared/mapentities_shared.cpp", 1)):
        replace_all(path, "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL)",
                    "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL) || defined(SOURCE_ENGINE_PORT)", expected)
    # Only callbacks require Steam; the per-frame virtual must exist offline too.
    replace("game/shared/steamworks_gamestats.cpp",
            "void CSteamWorksGameStatsUploader::FrameUpdatePostEntityThink()",
            "#endif // NO_STEAM callbacks\n\nvoid CSteamWorksGameStatsUploader::FrameUpdatePostEntityThink()")
    replace("game/shared/steamworks_gamestats.cpp",
            "\n#endif\n\n//-----------------------------------------------------------------------------\n// Purpose: Opens a session:",
            "\n//-----------------------------------------------------------------------------\n// Purpose: Opens a session:")
    replace("game/shared/steamworks_gamestats.cpp",
            "bool CSteamWorksGameStatsUploader::AccessToSteamAPI( void )\n{",
            "bool CSteamWorksGameStatsUploader::AccessToSteamAPI( void )\n{\n#ifdef NO_STEAM\n\treturn false;\n#else")
    replace("game/shared/steamworks_gamestats.cpp",
            "\treturn false;\n}\n\n//-----------------------------------------------------------------------------\n// Purpose: There's no guarantee",
            "\treturn false;\n#endif\n}\n\n//-----------------------------------------------------------------------------\n// Purpose: There's no guarantee")
    replace("game/shared/steamworks_gamestats.cpp",
            "ISteamGameStats* CSteamWorksGameStatsUploader::GetInterface( void )\n{",
            "ISteamGameStats* CSteamWorksGameStatsUploader::GetInterface( void )\n{\n#ifdef NO_STEAM\n\treturn NULL;\n#else")
    replace("game/shared/steamworks_gamestats.cpp",
            "// If we haven't returned already, then we can't get access to the interface\n\treturn NULL;\n}",
            "// If we haven't returned already, then we can't get access to the interface\n\treturn NULL;\n#endif\n}")

# The one shared studio implementation must initialize gameplay activities when
# the original game is linked; its engine-only mode remains unchanged otherwise.
replace("public/studio.cpp",
        "#if defined(SERVER_DLL) || defined(CLIENT_DLL) || defined(GAME_DLL)",
        "#if defined(SERVER_DLL) || defined(CLIENT_DLL) || defined(GAME_DLL) || defined(SOURCE_GAME_LINK)")
replace("public/stringregistry.cpp", "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL)",
        "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL) || defined(SOURCE_ENGINE_PORT)")
replace("public/editor_sendcommand.cpp", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)",
        "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB) || defined(SOURCE_ENGINE_PORT)")
replace("engine/sys_dll.cpp",
        "\tCSysModule *pDLL = NULL;\n\n\t// check signature",
        "\tCSysModule *pDLL = NULL;\n#ifdef SOURCE_GAME_LINK\n\tif (Q_stricmp(szDllFilename, \"server\" DLL_EXT_STRING)) return false;\n\tg_iServerGameDLLVersion = 0;\n\tg_ServerFactory = Sys_GetFactoryThis();\n#else\n\t// check signature")
replace("engine/sys_dll.cpp", "\tg_ServerFactory = Sys_GetFactory( pDLL );\n\tif ( g_ServerFactory )",
        "\tg_ServerFactory = Sys_GetFactory( pDLL );\n#endif // SOURCE_GAME_LINK\n\tif ( g_ServerFactory )")
replace("engine/sys_dll.cpp", "\tif ( !g_GameDLL )\n\t\treturn;",
        "#ifndef SOURCE_GAME_LINK\n\tif ( !g_GameDLL )\n\t\treturn;\n#endif")
replace("engine/sys_dll.cpp", "\tFileSystem_UnloadModule( g_GameDLL );",
        "\tif (g_GameDLL) FileSystem_UnloadModule( g_GameDLL );")

# Projected rotational speeds below use sqrt(1.001 - dot^2), which can
# exceed the unprojected rotation speed. Keep the event solver's upper
# bound conservative, including exactly axial sphere/box contacts.
replace("ivp/ivp_collision/ivp_mindist.cxx",
        "mim.worst_case_speed = mim.sum_max_surface_rot_speed + core0->current_speed + core1->current_speed;",
        "mim.worst_case_speed = (mim.sum_max_surface_rot_speed + core0->current_speed + core1->current_speed) * 1.001 + P_DOUBLE_EPS;")

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
# iOS/APFS is case-sensitive; enable the same fallback used on Linux.
replace_all("filesystem/filesystem_stdio.cpp", "#if defined(LINUX) || defined(PLATFORM_BSD)",
            "#if defined(LINUX) || defined(PLATFORM_BSD) || defined(SOURCE_IOS)", 3)
# Windows caches contain mixed-case directory names as well as file names.
# Resolve each component; the upstream helper only checks the final directory.
case_source = (args.upstream / "filesystem/linux_support.cpp").read_text()
case_start = case_source.index("bool findFileInDirCaseInsensitive(")
replace("filesystem/linux_support.cpp", case_source[case_start:], r'''bool findFileInDirCaseInsensitive( const char *file, char* output, size_t bufSize)
{
    if(!file || !output || !bufSize)return false;output[0]=0;
    if(strlen(file)>=MAX_PATH)return false;
    char path[MAX_PATH];path[0]=file[0]=='/'?'/':'.';path[1]=0;
    const char* cursor=file;
    while(*cursor){while(*cursor=='/')++cursor;if(!*cursor)break;
        const char* end=strchr(cursor,'/');size_t length=end?size_t(end-cursor):strlen(cursor);
        if(!length||length>=MAX_PATH)return false;
        char component[MAX_PATH];memcpy(component,cursor,length);component[length]=0;
        char match[MAX_PATH]={0};char exact[MAX_PATH];
        int n=snprintf(exact,sizeof(exact),"%s/%s",path,component);struct stat info;
        if(n>0&&n<int(sizeof(exact))&&!stat(exact,&info))V_strncpy(match,component,sizeof(match));
        DIR* dir=match[0]?nullptr:opendir(path);if(!match[0]&&!dir)return false;
        for(dirent* entry=dir?readdir(dir):nullptr;entry;entry=readdir(dir))if(!strcasecmp(entry->d_name,component)){
            if(!match[0]||!strcmp(entry->d_name,component)||strcmp(entry->d_name,match)<0)V_strncpy(match,entry->d_name,sizeof(match));
            if(!strcmp(entry->d_name,component))break;
        }
        if(dir)closedir(dir);if(!match[0])return false;
        size_t current=strlen(path),addition=strlen(match);bool separator=current&&path[current-1]!='/';
        if(current+separator+addition>=sizeof(path))return false;
        if(separator)path[current++]='/';memcpy(path+current,match,addition+1);cursor+=length;
    }
    if(strlen(path)>=bufSize)return false;V_strncpy(output,path,bufSize);return true;
}
''')
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
replace("engine/l_studio.cpp", '#include "render_pch.h"', '#include "render_pch.h"\nclass IStudioDataCache;\nextern IStudioDataCache* g_pStudioDataCache;')
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
# The desktop process exits after shutdown; the UIKit host can restart in place.
replace("engine/host.cpp", '\t\t\tSys_Error( "Host_Shutdown (bottom):  _heapchk() != _HEAPOK\\n" );\n\t\t}\n#endif\n\t}\n}', '\t\t\tSys_Error( "Host_Shutdown (bottom):  _heapchk() != _HEAPOK\\n" );\n\t\t}\n#endif\n\t}\n\tshutting_down = false;\n}')
replace("engine/zone.cpp", '\tg_HunkMemoryStack.FreeAll();\n\n\t// This disconnects', '\tg_HunkMemoryStack.Term();\n\n\t// This disconnects')

replace("public/collisionutils.cpp", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB) || defined(SOURCE_ENGINE_PORT)")
replace("public/dt_recv.cpp", "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL)", "#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL) || defined(SOURCE_ENGINE_PORT)")
replace("public/dt_send.cpp", "#if !defined(_STATIC_LINKED) || defined(GAME_DLL)", "#if !defined(_STATIC_LINKED) || defined(GAME_DLL) || defined(SOURCE_ENGINE_PORT)")
replace("mathlib/IceKey.cpp", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB)", "#if !defined(_STATIC_LINKED) || defined(_SHARED_LIB) || defined(SOURCE_ENGINE_PORT)")
shutil.copytree(args.upstream / "thirdparty/stb", args.output / "thirdparty/stb", dirs_exist_ok=True)
# Imported preview trees need independently reclaimable storage. Compile Source's
# tool-mode displacement collision implementation, not the engine's hunk-backed
# ABI. Rename its types/exports so both genuine implementations can coexist.
disp_names = ["CDispCollTree", "CDispCollTriCache", "CDispCollTri", "CDispCollHelper",
              "CDispCollNode", "CDispCollLeaf", "CDispVector", "DispCollTrees_Alloc",
              "DispCollTrees_Free", "DispCollPlaneIndex_t", "CPlaneIndexHashFuncs",
              "g_DispCollPlaneIndexHash", "DISPCOLL_COMMON_H", "RayDispOutput_t",
              "rayleaflist_t", "MAX_DISP_AABB_NODES", "MAX_AABB_LIST"]
for suffix in ("h", "cpp"):
    text = (args.upstream / f"public/dispcoll_common.{suffix}").read_text()
    for name in disp_names:
        text = re.sub(r"\b" + re.escape(name) + r"\b", "Port" + name, text)
    text = text.replace('"dispcoll_common.h"', '"dispcoll_preview.h"')
    (args.output / f"public/dispcoll_preview.{suffix}").write_text(text)
(args.output / "port-revision.json").write_text(json.dumps({"upstream": PIN, "patches": patch_count}, indent=2))
for path, (content, modified) in previous.items():
    file = args.output / path
    if file.is_file() and file.read_bytes() == content:
        os.utime(file, ns=(file.stat().st_atime_ns, modified))
print(f"Prepared real Source modules from {PIN}")
