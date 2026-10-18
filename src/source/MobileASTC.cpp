#include "MobileASTC.hpp"
#include "astc.h"
#include <vector>
#include <cstring>

bool MobileASTC::decodeBGRA(const void *data, size_t size, int width, int height,
                           int format, void *pixels, size_t pixelBytes) {
    const int required = bytes(width, height, 1, format);
    if (!required || !data || !pixels || size < size_t(required) ||
        uint64_t(width) * uint64_t(height) * 4 > pixelBytes) return false;
    // The scalar decoder reads bit words. Pad the last block so those reads
    // never cross the allocation, even for the smallest mip.
    std::vector<uint8_t> padded(size_t(required) + 16, 0);
    std::memcpy(padded.data(), data, required);
    return decode_astc(padded.data(), width, height, block(format), block(format),
                       static_cast<uint32_t *>(pixels)) != 0;
}
