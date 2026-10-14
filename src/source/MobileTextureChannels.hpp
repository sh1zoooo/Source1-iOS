#pragma once

// GLES 3 texture component selectors. Apply to the texture object, so CPU
// pointers and pixel-unpack buffers use the same channel correction.
struct MobileTextureChannels {
    unsigned red;
    unsigned blue;
    constexpr MobileTextureChannels(bool bgrSource, bool renderTarget)
        : red(bgrSource && !renderTarget ? 0x1905u : 0x1903u),
          blue(bgrSource && !renderTarget ? 0x1903u : 0x1905u) {}
};
