#include "MobileTextureChannels.hpp"
#include <cassert>
int main() {
    MobileTextureChannels bgra(true, false);
    assert(bgra.red == 0x1905 && bgra.blue == 0x1903);
    MobileTextureChannels rgba(false, false);
    assert(rgba.red == 0x1903 && rgba.blue == 0x1905);
    // Render targets contain shader output, regardless of their D3D format name.
    MobileTextureChannels target(true, true);
    assert(target.red == 0x1903 && target.blue == 0x1905);
}
