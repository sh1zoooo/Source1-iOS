#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <mutex>
#include "SourceBridge.hpp"

namespace source1ios {
// Host services plus statically linked real Source core libraries.
class Runtime final {
public:
    ~Runtime() { stop(); }
    bool start(const std::filesystem::path& documents);
    void setActive(bool active);
    void frame(double seconds);
    void stop();
    void log(const std::string& message);
    bool executeSource(const std::string& command) { return source_.execute(command); }
    std::array<SourceVertex, 36> vertices(float aspect) const { return source_.vertices(aspect); }
    bool sourceReady() const { return source_.ready(); }
    bool running() const { return running_; }
    std::uint64_t frames() const { return frames_; }
    double elapsed() const { return elapsed_; }
    const std::filesystem::path& logPath() const { return logPath_; }
private:
    std::mutex logMutex_;
    std::ofstream log_;
    std::filesystem::path logPath_;
    bool running_ = false;
    bool active_ = false;
    std::uint64_t frames_ = 0;
    double elapsed_ = 0;
    SourceBridge source_;
};
}
