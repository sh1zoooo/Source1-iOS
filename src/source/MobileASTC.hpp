#pragma once
#include <cstddef>
#include <cstdint>
#include <climits>

// ClientMod 2.0's VTF IDs. Existing Source image/D3D IDs must stay unchanged.
namespace MobileASTC {
inline int block(int format) { return format == 43 ? 4 : format == 47 ? 6 : 0; }
inline bool isFormat(int format) { return block(format) != 0; }
inline int bytes(int width, int height, int depth, int format) {
    const int b = block(format);
    if (!b || width <= 0 || height <= 0 || depth <= 0) return 0;
    const uint64_t n = ((uint64_t(width) + b - 1) / b) *
                       ((uint64_t(height) + b - 1) / b) * 16;
    return n <= uint64_t(INT_MAX) / uint64_t(depth) ? int(n * depth) : 0;
}
inline unsigned glFormat(int format, bool srgb) {
    return format == 43 ? (srgb ? 0x93D0 : 0x93B0) :
           format == 47 ? (srgb ? 0x93D4 : 0x93B4) : 0;
}
inline int fromGL(unsigned format) {
    return format == 0x93B0 || format == 0x93D0 ? 43 :
           format == 0x93B4 || format == 0x93D4 ? 47 : -1;
}
inline bool srgbGL(unsigned format) { return format == 0x93D0 || format == 0x93D4; }
bool decodeBGRA(const void *data, size_t size, int width, int height, int format,
                void *pixels, size_t pixelBytes, bool srgb = false);
}
