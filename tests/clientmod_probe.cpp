#include "Runtime.hpp"
#include "tier0/platform.h"
#include "filesystem.h"
#include "host_cmd.h"
#include "host.h"
#include "cmd.h"
#include "server.h"
#include "server_class.h"
#include "icvar.h"
#include "tier1/convar.h"
#include "tier2/tier2.h"
#include "eiface.h"
#include "tier1/interface.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdlib>
#ifdef __linux__
#include <csignal>
#include <execinfo.h>
#include <unistd.h>
#endif
extern CGameServer sv;

int main(int argc, char** argv) {
#ifdef __linux__
    // Diagnostics for original SDK faults during explicit manual integration.
    const auto diagnostic = [](int signal) {
        void* frames[48]; const int count = backtrace(frames, 48);
        backtrace_symbols_fd(frames, count, STDERR_FILENO); _exit(128+signal);
    };
    std::signal(SIGSEGV, diagnostic);
    std::signal(SIGABRT, diagnostic);
#endif
    try {
        if (argc != 3) throw std::runtime_error("Usage: clientmod_probe DOCUMENTS MAP_BASENAME");
        std::string map = argv[2];
        if (map.empty() || map.size() > 64 || map.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos)
            throw std::runtime_error("Invalid map basename");
        const auto documents = std::filesystem::absolute(argv[1]);
        if (!std::filesystem::is_directory(documents / "Source1IOS/content"))
            throw std::runtime_error("User-supplied content directory missing");
        source1ios::Runtime runtime;
        for (unsigned cycle = 1; cycle <= 2; ++cycle) {
            if (!runtime.start(documents)) throw std::runtime_error("Original host startup failed");
            if (auto* hibernate = g_pCVar->FindVar("sv_hibernate_when_empty")) hibernate->SetValue(0);
            std::cerr << "ClientMod probe: original Host_NewGame(" << map << ")\n";
            if (!Host_NewGame(map.data(), false, false)) throw std::runtime_error("Host_NewGame rejected map");
            if (!sv.IsActive()) throw std::runtime_error("Game server was not activated");
            unsigned live = 0, spawns = 0;
            for (int i = 0; i < sv.num_edicts; ++i) {
                if (sv.edicts[i].IsFree()) continue;
                const char* name = sv.edicts[i].GetClassName();
                if (!name || !*name) continue;
                ++live;
                spawns += std::string(name) == "info_player_terrorist" || std::string(name) == "info_player_counterterrorist";
            }
            if (!live || !spawns) throw std::runtime_error("Original game entities/spawns missing");
            // An empty server intentionally does not advance simulation ticks.
            // The optional fake client uses the original ClientPutInServer path,
            // without needing a generated navigation mesh or external networking.
            if (std::getenv("SOURCE_CLIENTMOD_PLAYER")) {
                auto* engine = static_cast<IVEngineServer*>(Sys_GetFactoryThis()(INTERFACEVERSION_VENGINESERVER, nullptr));
                auto* player = engine ? engine->CreateFakeClient("Source1IOS probe") : nullptr;
                auto* networkable = player ? player->GetNetworkable() : nullptr;
                auto* cls = networkable ? networkable->GetServerClass() : nullptr;
                if (!cls || std::string(cls->GetName()) != "CCSPlayer")
                    throw std::runtime_error("Original fake-client/player creation failed");
            }
            const int before = sv.GetTick();
            const int hostBefore = host_tickcount;
            for (unsigned i = 0; i < 300; ++i) runtime.frame(1.0 / 66.0);
            if (!sv.IsActive() || host_tickcount <= hostBefore) throw std::runtime_error("Active server frame loop did not advance");
            if (std::getenv("SOURCE_CLIENTMOD_PLAYER") && sv.GetTick() <= before)
                throw std::runtime_error("Player simulation did not advance");
            std::cout << "ClientMod original LevelInit/activation: PASS; cycle=" << cycle << "; entities=" << live
                      << "; team_spawns=" << spawns << "; host_ticks=" << host_tickcount-hostBefore
                      << "; simulation_ticks=" << sv.GetTick() - before << '\n';
            runtime.stop();
            if (sv.IsActive()) throw std::runtime_error("Server remained active after shutdown");
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
