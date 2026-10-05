#include "SourceHost.hpp"
#include "quakedef.h"
#include "host.h"
#include "host_cmd.h"
#include "common.h"
#include "cmd.h"
#include "filesystem.h"
#include "filesystem/IQueuedLoader.h"
#include "tier2/tier2.h"
#include "tier0/icommandline.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"

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
    CommandLine()->CreateCmdLine("source1-ios -dedicated -nogamedll -insecure -noip -noipx -nobreakpad -noshaderapi -nosound -nowatchdog");
    Msg("Source host: calling original Host_Init(true), -nogamedll, networking disabled\n");
    Host_Init(true);
    started_ = host_initialized;
    if (!started_) return false;
    // This is an engine-only harness. Discard queued game configuration (valve.rc
    // and modsettings.cfg); it can contain commands that require a game DLL.
    Cbuf_Init();
    if (!selfTest()) { stop(); return false; }
    Msg("Source Host_Init completed: dedicated idle host; original Host_RunFrame active. Game DLL, maps and graphics remain pending.\n");
    return true;
}
void SourceHost::frame(float seconds) {
    if (started_ && host_initialized && seconds > 0) Host_RunFrame(seconds);
}
bool SourceHost::selfTest() {
    if (!started_) return false;
    bool all = report("queued loader", g_pQueuedLoader && !g_pQueuedLoader->IsMapLoading());
    all &= report("Host_Init", host_initialized);
    const auto before = host_tickcount;
    for (unsigned i = 0; i < 8; ++i) frame(1.0f / 60);
    all &= report("Host_RunFrame idle ticks", host_tickcount > before);
    return all;
}
void SourceHost::stop() {
    if (!started_) return;
    Host_Shutdown();
    started_ = false;
    Msg("Source Host_Shutdown completed; host_initialized=%d\n", host_initialized);
    host_parms.basedir = host_parms.mod = host_parms.game = nullptr;
    com_gamedir[0] = com_basedir[0] = 0;
    base_.clear();
    game_.clear();
}
}
