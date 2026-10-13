#pragma once
#include <cmath>
#include <cstring>

// Source's touch ABI stores the float bit patterns in data2/data3.
// data1 is the contact slot, not the old packed 16-bit XY coordinate.
inline bool TouchMenuCoordinates(int slot, int xBits, int yBits,
                                 int width, int height, int &x, int &y) {
    if (slot != 0 || width <= 0 || height <= 0) return false;
    float fx, fy;
    static_assert(sizeof(float) == sizeof(int), "touch ABI requires 32-bit floats");
    std::memcpy(&fx, &xBits, sizeof(fx));
    std::memcpy(&fy, &yBits, sizeof(fy));
    if (!std::isfinite(fx) || !std::isfinite(fy)) return false;
    fx = fx < 0.f ? 0.f : (fx > 1.f ? 1.f : fx);
    fy = fy < 0.f ? 0.f : (fy > 1.f ? 1.f : fy);
    x = fx >= 1.f ? width - 1 : int(fx * width);
    y = fy >= 1.f ? height - 1 : int(fy * height);
    return true;
}
