#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace source1ios {
// Host services for future Source modules. No Valve engine code is linked yet.
class Runtime final {
public:
    bool start(const std::filesystem::path& documents);
    void setActive(bool active);
    void frame(double seconds);
    void stop();
    void log(const std::string& message);
    bool running() const { return running_; }
    std::uint64_t frames() const { return frames_; }
    double elapsed() const { return elapsed_; }
    const std::filesystem::path& logPath() const { return logPath_; }
private:
    std::ofstream log_;
    std::filesystem::path logPath_;
    bool running_ = false;
    bool active_ = false;
    std::uint64_t frames_ = 0;
    double elapsed_ = 0;
};
}
