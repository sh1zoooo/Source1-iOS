#pragma once
#include "RenderTypes.hpp"
#include <filesystem>
#include <memory>
#include <vector>
namespace source1ios {
// Original engine brush loading/collision for the built-in world; polygon
// preview for user maps. Drawing uses our Metal adapter, not Source shaderapi.
class SourceMap final {
public:
    SourceMap();
    ~SourceMap();
    bool start(const std::filesystem::path& root);
    void stop();
    bool load(const char* filename, const char* pathID = "GAME");
    bool resetMap();
    bool demoTerrain();
    bool demoMaterials();
    bool demoProps();
    bool demoPhy();
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
