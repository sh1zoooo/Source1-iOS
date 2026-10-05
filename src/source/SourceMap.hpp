#pragma once
#include "RenderTypes.hpp"
#include <filesystem>
#include <memory>
#include <vector>
namespace source1ios {
// BSP polygon preview uses Source's original lump loader and collision library.
// It does not replace the engine world loader or graphical materialsystem.
class SourceMap final {
public:
    SourceMap();
    ~SourceMap();
    bool start(const std::filesystem::path& root);
    void stop();
    bool load(const char* filename, const char* pathID = "GAME");
    bool selfTest();
    void look(float yaw, float pitch);
    void move(float forward, float right, float seconds);
    void resetCamera();
    std::vector<SourceVertex> vertices(float aspect) const;
    const SourceTexture& texture() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
