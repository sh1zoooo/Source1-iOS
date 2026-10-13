#pragma once
// Select existing mip levels before reading/decompressing a world texture.
// Preserve block-compressed minimum dimensions and never invent missing mips.
inline int MobileTextureMipSkip(int width, int height, int mipCount) {
    int skip = 0;
    while ((width > 512 || height > 512) && width > 4 && height > 4 &&
           skip + 1 < mipCount) {
        width >>= 1;
        height >>= 1;
        ++skip;
    }
    return skip;
}
