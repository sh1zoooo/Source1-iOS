#pragma once
#include "RenderTypes.hpp"
#include "PlayerState.hpp"
#include <filesystem>
#include <memory>
#include <vector>
namespace source1ios {
// Original engine brush loading/collision for the built-in world; polygon
// preview for user maps. Drawing uses our Metal adapter, not Source shaderapi.
bool sourceDecodeUITexture(const std::string& material,SourceTexture& texture);
class SourceMap final {
public:
    SourceMap();
    ~SourceMap();
    bool start(const std::filesystem::path& root);
    void stop();
    bool load(const char* filename, const char* pathID = "GAME", bool gameLevel=false);
    bool resetMap();
    bool demoTerrain();
    bool demoMaterials();
    bool demoProps();
    bool demoPhy();
    bool demoHdr();
    bool hdrSelfTest();
    bool materialsSelfTest();
    bool demoSkins();
    bool demoEntities();
    bool entitiesSelfTest();
    bool setSkin(unsigned family);
    bool skinSelfTest();
    bool propsSkinSelfTest();
    bool propsSelfTest();
    bool phySelfTest();
    bool loadModel(const char* filename, const char* pathID = "GAME");
    bool resetModel();
    bool playAnimation(unsigned index);
    bool setAnimationPlaying(bool playing);
    bool selfTest();
    void look(float yaw, float pitch);
    void frame(float seconds);
    void move(float forward, float right, float seconds);
    void resetCamera();
    void setGameView(const PlayerState& player,bool thirdPerson,bool modelReady=true);
    void clearGameView();
    bool resetPhysics();
    bool impulsePhysics();
    std::vector<SourceVertex> vertices(float aspect) const;
    const SourceTexture& texture() const;
    std::uint64_t textureRevision() const;
    const SourceTexture& lightmapTexture() const;
    const SourceTexture& modelTexture() const;
    std::uint64_t modelTextureRevision() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
