#pragma once
#include <cstdint>
#include <vector>
namespace source1ios {
struct alignas(16) SourceVertex { float position[4]; float color[4]; float uv[2]; float material[2]; float lightmap[4]; };
struct SourceTexture { unsigned width=0, height=0; std::vector<std::uint8_t> pixels; };
}
