#pragma once
#include <array>
#include <string>
#include "SourceFiles.hpp"
#include "SourceAppSystems.hpp"
#include "SourceHost.hpp"
#include "SourceMap.hpp"
#include "RenderTypes.hpp"

namespace source1ios {
class SourceBridge final {
public:
    using Logger = void (*)(void*, const char*);
    bool start(Logger logger, void* context, const std::filesystem::path& root);
    void stop();
    bool selfTest();
    bool execute(const std::string& command);
    void frame(double seconds);
    std::vector<SourceVertex> vertices(float aspect) const;
    const SourceTexture& texture() const { return map_.texture(); }
    std::uint64_t textureRevision() const { return map_.textureRevision(); }
    const SourceTexture& modelTexture() const { return map_.modelTexture(); }
    std::uint64_t modelTextureRevision() const { return map_.modelTextureRevision(); }
    void cameraLook(float yaw, float pitch) { map_.look(yaw, pitch); }
    void cameraMove(float forward, float right, float seconds) { map_.move(forward, right, seconds); }
    bool ready() const { return ready_; }
private:
    bool ready_ = false;
    bool ownsCore_ = false;
    SourceAppSystems systems_;
    SourceFiles files_;
    SourceHost host_;
    SourceMap map_;
    double elapsed_ = 0;
};
}
