#include "MobileTextureChannels.hpp"
#include <cassert>
#include <cstring>
int main() {
    const unsigned char bgra[] = {0, 0, 255, 17, 255, 0, 0, 99};
    const unsigned char rgba[] = {255, 0, 0, 17, 0, 0, 255, 99};
    for (int upload = 0; upload < 2; ++upload) {
        MobileTextureChannels converted(bgra, sizeof(bgra), 4);
        assert(converted.data());
        assert(std::memcmp(converted.data(), rgba, sizeof(rgba)) == 0);
        assert(bgra[0] == 0 && bgra[2] == 255 && bgra[3] == 17);
    }
    const unsigned char bgr[] = {10, 20, 30, 40, 50, 60};
    const unsigned char rgb[] = {30, 20, 10, 60, 50, 40};
    MobileTextureChannels converted(bgr, sizeof(bgr), 3);
    assert(std::memcmp(converted.data(), rgb, sizeof(rgb)) == 0);
    MobileTextureChannels empty(nullptr, 64, 4);
    assert(!empty.data());
}
