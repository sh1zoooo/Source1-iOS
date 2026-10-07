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
    std::vector<SourceVertex> vertices(float aspect) const { return source_.vertices(aspect); }
    const SourceTexture& texture() const { return source_.texture(); }
    std::uint64_t textureRevision() const { return source_.textureRevision(); }
    const SourceTexture& lightmapTexture() const { return source_.lightmapTexture(); }
    const SourceTexture& modelTexture() const { return source_.modelTexture(); }
    std::uint64_t modelTextureRevision() const { return source_.modelTextureRevision(); }
    void cameraLook(float yaw, float pitch) { source_.cameraLook(yaw,pitch); }
    void cameraMove(float forward, float right, float seconds) { source_.cameraMove(forward,right,seconds); }
    bool startGame(const std::string& map="awp_lego_2") { return running_&&source_.startGame(map); }
    void stopGame() { source_.stopGame(); }
    void playerButton(unsigned button,bool pressed) { if(active_)source_.playerButton(button,pressed); }
    void thirdPerson(bool enabled) { source_.thirdPerson(enabled); }
    const PlayerState& playerState() const { return source_.playerState(); }
    const std::string& gameError() const { return source_.gameError(); }
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
