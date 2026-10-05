#include "Runtime.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        ("source1ios-test-" + std::to_string(
            std::filesystem::file_time_type::clock::now().time_since_epoch().count()));
    try {
        source1ios::Runtime host;
        host.frame(0.016);
        check(host.frames() == 0, "Stopped host accepted a frame");
        check(host.start(directory), "Could not start host");
        check(host.sourceReady(), "Real Source modules did not start");
        check(host.executeSource("source_host_selftest"), "Original Host_Init / idle frame contracts failed");
        check(host.executeSource("source_engine_selftest"), "Engine subsystem contracts failed");
        check(host.executeSource("source_assets_selftest"), "Assets/physics contracts failed");
        check(!host.executeSource("map test"), "Game command exposed without a game DLL");
        check(host.executeSource("source_app_selftest"), "Appframework contracts failed");
        check(host.executeSource("source_fs_selftest"), "Original filesystem contracts failed");
        check(host.executeSource("source_selftest"), "Source contracts failed");
        check(host.executeSource("ios_rotation_speed 0"), "Source ConVar command failed");
        const auto before = host.vertices(1);
        host.frame(0.01);
        const auto after = host.vertices(1);
        check(before[0].position[0] == after[0].position[0], "Rotation ignored Source ConVar");
        check(!host.executeSource("missing_source_command"), "Unknown command accepted");
        check(!host.start(directory), "Duplicate start accepted");
        host.frame(0.016);
        host.setActive(false);
        host.frame(1);
        check(host.frames() == 2, "Background host advanced simulation");
        host.setActive(true);
        host.frame(30);
        check(std::abs(host.elapsed() - 0.126) < 1e-9, "Background interval was not clamped");
        host.frame(std::numeric_limits<double>::quiet_NaN());
        host.frame(std::numeric_limits<double>::infinity());
        host.frame(-1);
        check(host.frames() == 3, "Invalid delta was accepted");
        const auto logPath = host.logPath();
        host.stop();
        host.stop();
        std::ifstream file(logPath);
        std::string contents((std::istreambuf_iterator<char>(file)), {});
        check(contents.find("Host paused") != std::string::npos, "Pause log missing");
        check(contents.find("Host stopped after 3 frames") != std::string::npos, "Shutdown log missing");
        check(contents.find("Source Host_Shutdown completed; host_initialized=0") != std::string::npos, "Original host did not shut down");
        check(contents.find("Source host GAME directory: " + (directory / "Source1IOS" / "game").string()) != std::string::npos, "Host path differs from the mounted game path");
        check(contents.find("Recursive shutdown") == std::string::npos, "Recursive shutdown guard persisted");
        check(host.start(directory), "Restart failed");
        check(host.frames() == 0 && host.elapsed() == 0, "Restart retained simulation state");
        check(host.executeSource("source_host_selftest"), "Original host failed after restart");
        host.stop();
        const auto badPath = directory / "regular-file";
        std::ofstream(badPath) << "not a directory";
        check(!host.start(badPath), "Invalid document directory accepted");
        check(!host.running(), "Failed startup left host running");
        std::filesystem::remove_all(directory);
        std::cout << "Runtime contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
