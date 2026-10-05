#pragma once
#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include <cstdint>
#include <string>
#include <vector>

namespace source1ios {
struct StudioVertex {
    Vector position;
    Vector normal;
    Vector2D uv;
};
struct StudioMesh {
    std::vector<StudioVertex> triangles;
    unsigned sourceVertices=0;
    unsigned meshes=0;
};
struct StudioFixture {
    std::vector<std::uint8_t> mdl, vvd, vtx;
};
StudioFixture makeStudioFixture();
bool parseStudioModel(const std::vector<std::uint8_t>& mdl,
    const std::vector<std::uint8_t>& vvd,
    const std::vector<std::uint8_t>& vtx,
    StudioMesh& output, std::string& error);
}
