#pragma once
#include <array>
#include <string>
#include "SourceFiles.hpp"
#include "SourceAppSystems.hpp"
#include "SourceHost.hpp"
#include "SourceMap.hpp"
#include "RenderTypes.hpp"
#include "SourcePlayer.hpp"
#include "SourceMobile.hpp"
#include "PracticeMaps.hpp"

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
    const SourceTexture& lightmapTexture() const { return map_.lightmapTexture(); }
    const SourceTexture& modelTexture() const { return map_.modelTexture(); }
    std::uint64_t modelTextureRevision() const { return map_.modelTextureRevision(); }
    void cameraLook(float yaw, float pitch) { if(player_.active())player_.look(yaw,pitch);else map_.look(yaw,pitch); }
    void cameraMove(float forward, float right, float seconds) { if(player_.active())player_.move(forward,right);else map_.move(forward,right,seconds); }
    std::vector<std::string> maps() { return files_.maps(); }
    bool startGame(const std::string& map);
    void stopGame();
    MobileResources mobileResources() { return loadMobileResources(player_.state().team); }
    int buy(const std::string& alias) { return player_.buy(alias); }
    bool playerAction(const std::string& action) { return player_.action(action); }
    void clearInput() { player_.move(0,0);player_.button(63,false);accumulator_=0; }
    void playerButton(unsigned button,bool pressed) { player_.button(button,pressed); }
    void thirdPerson(bool enabled) { thirdPerson_=enabled;gameModel_.clear(); }
    const PlayerState& playerState() const { return player_.state(); }
    const std::string& gameError() const { return gameError_.empty()?player_.error():gameError_; }
    bool ready() const { return ready_; }
private:
    bool ready_ = false;
    bool ownsCore_ = false;
    SourceAppSystems systems_;
    SourceFiles files_;
    SourceHost host_;
    SourceMap map_;
    SourcePlayer player_;
    bool thirdPerson_=false,gameModelReady_=false;
    double accumulator_=0;
    std::string gameModel_,gameError_;
    double elapsed_ = 0;
};
}
