#pragma once
#include <cstdint>
#include <vector>
namespace source1ios {
struct alignas(16) SourceVertex { float position[4]; float color[4]; float uv[2]; };
struct SourceTexture { unsigned width=0, height=0; std::vector<std::uint8_t> pixels; };
}
