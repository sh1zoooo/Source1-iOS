#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>

// Keep the CPU texture backing in its original D3D channel order.
// GLES receives a temporary RGB(A) copy, including whole rows for subimages.
class MobileTextureChannels {
    void *copy_ = nullptr;
public:
    MobileTextureChannels(const void *source, std::size_t bytes, unsigned channels) {
        if (!source || (channels != 3 && channels != 4)) return;
        copy_ = std::malloc(bytes);
        if (!copy_) return;
        std::memcpy(copy_, source, bytes);
        auto *p = static_cast<unsigned char *>(copy_);
        for (std::size_t i = 0; i + channels <= bytes; i += channels) {
            unsigned char r = p[i]; p[i] = p[i + 2]; p[i + 2] = r;
        }
    }
    ~MobileTextureChannels() { std::free(copy_); }
    void *data() const { return copy_; }
    MobileTextureChannels(const MobileTextureChannels &) = delete;
    MobileTextureChannels &operator=(const MobileTextureChannels &) = delete;
};
