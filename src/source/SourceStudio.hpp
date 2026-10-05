#pragma once
#include "mathlib/vector.h"
#include "mathlib/vector2d.h"
#include "mathlib/mathlib.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace source1ios {
struct StudioVertex {
    Vector position;
    Vector normal;
    Vector2D uv;
    std::array<unsigned char,3> bones{};
    std::array<float,3> weights{};
    unsigned influences=0;
};
struct StudioBone { int parent=-1; Vector position; Quaternion rotation; matrix3x4_t poseToBone; };
struct StudioMesh {
    std::vector<StudioVertex> triangles;
    std::vector<StudioBone> bones;
    unsigned sourceVertices=0;
    unsigned meshes=0;
};
struct StudioFixture {
    std::vector<std::uint8_t> mdl, vvd, vtx;
};
StudioFixture makeStudioFixture();
// Local rotations are explicit pose overrides, not decoded MDL sequences.
bool skinStudioModel(const StudioMesh& model, const std::vector<Quaternion>& rotations,
    std::vector<StudioVertex>& output);
bool parseStudioModel(const std::vector<std::uint8_t>& mdl,
    const std::vector<std::uint8_t>& vvd,
    const std::vector<std::uint8_t>& vtx,
    StudioMesh& output, std::string& error);
}
