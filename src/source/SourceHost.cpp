#include "SourceHost.hpp"
#include "quakedef.h"
#include "host.h"
#include "host_cmd.h"
#include "host_state.h"
#include "common.h"
#include "cmd.h"
#include "sys_dll.h"
#include "sv_main.h"
#include "eiface.h"
#include "server.h"
#include "server_class.h"
#include "filesystem.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include <fstream>

#ifdef SOURCE_GAME_LINK
extern IServerGameDLL* serverGameDLL;
extern IServerGameEnts* serverGameEnts;
extern IServerGameClients* serverGameClients;
extern CGameServer sv;
extern void SV_ShutdownGameDLL();
#endif

namespace {
bool report(const char* name, bool passed) {
    Msg("Source host self-test %s: %s\n", name, passed ? "PASS" : "FAIL");
    return passed;
}
}
namespace source1ios {
bool SourceHost::start(const std::filesystem::path& root) {
    if (started_ || host_initialized || !g_pQueuedLoader) return false;
    base_ = root.string();
    game_ = (root / "game").string();
    if (game_.size() >= sizeof(com_gamedir) || base_.size() >= sizeof(com_basedir)) return false;
    if (!g_pFullFileSystem || !g_pFullFileSystem->IsDirectory(game_.c_str())) return false;
    // Filesystem search paths are already mounted by SourceFiles. Keep stable
    // backing storage for Source's raw host_parms pointers throughout its life.
    host_parms.basedir = const_cast<char*>(base_.c_str());
    host_parms.mod = host_parms.game = const_cast<char*>(game_.c_str());
    host_parms.memsize = 128 * 1024 * 1024;
    V_strncpy(com_basedir, base_.c_str(), sizeof(com_basedir));
    V_strncpy(com_gamedir, game_.c_str(), sizeof(com_gamedir));
    Msg("Source host GAME directory: %s\n", com_gamedir);
#ifdef SOURCE_GAME_LINK
    // Authored resources for the offline demo, searched after mounted game
    // content. They do not replace the user's CS:S resource files.
    const auto fallback = root / "port-demo";
    std::error_code error;
    std::filesystem::create_directories(fallback / "scripts", error);
    if (error) return false;
    std::filesystem::create_directories(fallback / "resource", error);
    if (error) return false;
    const std::pair<const char*, const char*> resources[] = {
        {"scripts/surfaceproperties_manifest.txt", "\"surfaceproperties_manifest\" { \"file\" \"scripts/port_surfaceproperties.txt\" }\n"},
        {"scripts/port_surfaceproperties.txt", "\"default\" { \"density\" \"2000\" \"friction\" \"0.8\" \"elasticity\" \"0.1\" }\n"},
        {"scripts/soundscapes_manifest.txt", "\"soundscapes_manifest\" {}\n"},
        {"scripts/game_sounds_manifest.txt", "\"game_sounds_manifest\" {}\n"},
        {"resource/hltvevents.res", "\"hltvevents\" { \"hltv_status\" { \"clients\" \"long\" \"slots\" \"long\" \"proxies\" \"short\" } }\n"},
    };
    for (const auto& resource : resources) {
        std::ofstream output(fallback / resource.first, std::ios::binary | std::ios::trunc);
        output << resource.second;
        if (!output) return false;
    }
    g_pFullFileSystem->AddSearchPath(fallback.c_str(), "GAME", PATH_ADD_TO_TAIL);
    CommandLine()->CreateCmdLine("source1-ios -dedicated -insecure -noip -noipx -nobreakpad -noshaderapi -nosound -nowatchdog -NoLoadPluginsForClient");
    if (!ServerDLL_Load(true) || !serverGameDLL) return false;
    Msg("Source host: calling original Host_Init(true) with linked CS:S GameDLL, networking disabled\n");
#else
    CommandLine()->CreateCmdLine("source1-ios -dedicated -nogamedll -insecure -noip -noipx -nobreakpad -noshaderapi -nosound -nowatchdog");
    Msg("Source host: calling original Host_Init(true), -nogamedll, networking disabled\n");
#endif
    Host_Init(true);
    started_ = host_initialized;
    if (!started_) return false;
    // The offline demo owns its startup commands; discard external autoexec
    // configuration rather than executing it during the embedded lifecycle.
    Cbuf_Init();
    if (!selfTest()) { stop(); return false; }
#ifdef SOURCE_GAME_LINK
    Msg("Source Host_Init completed: original CS:S GameDLL initialized; original Host_RunFrame active.\n");
#else
    Msg("Source Host_Init completed: dedicated idle host; original Host_RunFrame active. Game DLL, maps and graphics remain pending.\n");
#endif
    return true;
}
void SourceHost::frame(float seconds) {
    if (started_ && host_initialized && seconds > 0) HostState_Frame(seconds);
}
bool SourceHost::selfTest() {
    if (!started_) return false;
    bool all = report("queued loader", g_pQueuedLoader && !g_pQueuedLoader->IsMapLoading());
    all &= report("Host_Init", host_initialized);
    const auto before = host_tickcount;
    for (unsigned i = 0; i < 8; ++i) frame(1.0f / 60);
    all &= report("Host_RunFrame idle ticks", host_tickcount > before);
#ifdef SOURCE_GAME_LINK
    all &= report("original CS:S GameDLL initialized", serverGameDLL && serverGameEnts && serverGameClients && sv.dll_initialized);
    unsigned classes = 0;
    bool player = false, weapon = false;
    for (auto* cls = serverGameDLL->GetAllServerClasses(); cls && classes < 4096; cls = cls->m_pNext) {
        ++classes;
        player |= !V_strcmp(cls->GetName(), "CCSPlayer");
        weapon |= !V_strcmp(cls->GetName(), "CAK47");
    }
    Msg("Source CS:S registry: %u original server classes\n", classes);
    all &= report("CS:S player and AK47 server classes", classes == 196 && player && weapon);
    int minimum = 0, maximum = 0, defaults = 0;
    serverGameClients->GetPlayerLimits(minimum, maximum, defaults);
    all &= report("CS:S original player limits", minimum == 1 && maximum >= 32 && defaults == 32);
    const float tick = serverGameDLL->GetTickInterval();
    all &= report("CS:S original tick interval", tick > 0 && tick < .1f);
#endif
    return all;
}
void SourceHost::stop() {
    if (!started_) return;
#ifdef SOURCE_GAME_LINK
    if (sv.IsActive()) {
        // Drain the original state machine before freeing engine edicts. It
        // owns LevelShutdown and GameShutdown, which remove game entities.
        HostState_GameShutdown();
        for (unsigned i = 0; i < 3 && (sv.IsActive() || HostState_IsGameShuttingDown()); ++i)
            HostState_Frame(0);
    }
    Host_Disconnect(true);
    SV_ShutdownGameDLL();
#endif
    Host_Shutdown();
#ifdef SOURCE_GAME_LINK
    ServerDLL_Unload();
#endif
    started_ = false;
    Msg("Source Host_Shutdown completed; host_initialized=%d\n", host_initialized);
    host_parms.basedir = host_parms.mod = host_parms.game = nullptr;
    com_gamedir[0] = com_basedir[0] = 0;
    base_.clear();
    game_.clear();
}
}
