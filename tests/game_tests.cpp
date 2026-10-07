#include "Runtime.hpp"
#include "eiface.h"
#include "server_class.h"
#include "tier1/interface.h"
#include "server.h"
#include <filesystem>
#include <cstring>
#include <iostream>
#include <stdexcept>
extern CGameServer sv;

int main() {
    auto root = std::filesystem::temp_directory_path() / "source1ios-cstrike-probe";
    try {
        source1ios::Runtime host;
        if (!host.start(root)) throw std::runtime_error("Native host startup failed with linked CS:S");
        auto factory = Sys_GetFactoryThis();
        auto* game = static_cast<IServerGameDLL*>(factory(INTERFACEVERSION_SERVERGAMEDLL, nullptr));
        auto* clients = static_cast<IServerGameClients*>(factory(INTERFACEVERSION_SERVERGAMECLIENTS, nullptr));
        auto* ents = factory(INTERFACEVERSION_SERVERGAMEENTS, nullptr);
        if (!game || !clients || !ents) throw std::runtime_error("Original GameDLL interfaces missing");
        unsigned count = 0;
        bool player = false, weapon = false;
        for (auto* cls = game->GetAllServerClasses(); cls; cls = cls->m_pNext) {
            if (++count > 4096) throw std::runtime_error("Server class registry cycle");
            player |= !std::strcmp(cls->GetName(), "CCSPlayer");
            weapon |= !std::strcmp(cls->GetName(), "CAK47");
        }
        int minimum = 0, maximum = 0, defaults = 0;
        clients->GetPlayerLimits(minimum, maximum, defaults);
        if (!player || !weapon || minimum != 1 || maximum < 32 || defaults != 32)
            throw std::runtime_error("CS:S classes or player limits did not match the original game");
        const float tick = game->GetTickInterval();
        if (!(tick > 0 && tick < .1f)) throw std::runtime_error("Invalid game tick interval");
        std::cout << "Original CS:S interfaces and " << count << " server classes: PASS\n";
        auto maps = root / "Source1IOS/game/maps";
        std::filesystem::create_directories(maps);
        std::filesystem::copy_file(root / "Source1IOS/selftest/__source1ios_geometry.bsp", maps / "port_room.bsp", std::filesystem::copy_options::overwrite_existing);
        if (!sv.SpawnServer("port_room", "maps/port_room.bsp", nullptr))
            throw std::runtime_error("Original server did not spawn the test room");
        std::cout << "Original CGameServer test BSP spawn: PASS (LevelInit/activation remain separate)\n";
        host.stop();
        std::filesystem::remove_all(root);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
