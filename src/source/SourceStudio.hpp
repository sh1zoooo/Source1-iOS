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
    unsigned material=0;
    unsigned materialReference=0;
};
struct StudioBone { int parent=-1; Vector position; Quaternion rotation; matrix3x4_t poseToBone; };
struct StudioPose { std::vector<Quaternion> rotations; std::vector<Vector> positions; };
struct StudioAnimation { std::string name; float fps=0; bool looping=false; std::vector<StudioPose> frames; };
struct StudioMesh {
    std::int32_t checksum=0;
    Vector hullMins{0,0,0},hullMaxs{0,0,0},renderMins{0,0,0},renderMaxs{0,0,0};
    std::vector<StudioVertex> triangles;
    std::vector<StudioBone> bones;
    std::vector<StudioAnimation> animations;
    std::vector<std::string> materialPaths;
    std::vector<std::vector<std::string>> materials;
    std::vector<std::vector<unsigned>> skinFamilies;
    unsigned activeSkin=0;
    std::string animationPath;
    unsigned sourceVertices=0;
    unsigned meshes=0;
};
struct StudioFixture {
    std::vector<std::uint8_t> mdl, vvd, vtx, ani;
};
StudioFixture makeStudioFixture(bool external=false,bool multipleMaterials=false);
bool selectStudioSkin(StudioMesh& model,unsigned family);
// Local rotations are explicit pose overrides, not decoded MDL sequences.
bool skinStudioModel(const StudioMesh& model, const std::vector<Quaternion>& rotations,
    std::vector<StudioVertex>& output, const std::vector<Vector>& positions={});
bool sampleStudioAnimation(const StudioMesh& model,unsigned animation,double seconds,StudioPose& pose);
bool parseStudioModel(const std::vector<std::uint8_t>& mdl,
    const std::vector<std::uint8_t>& vvd,
    const std::vector<std::uint8_t>& vtx,
    StudioMesh& output, std::string& error,const std::vector<std::uint8_t>& ani={});
}
