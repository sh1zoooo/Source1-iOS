#include "MobileASTC.hpp"
#include "astcenc.h"

namespace {
struct DecodeContexts {
    astcenc_context *contexts[4]{};
    ~DecodeContexts() { for (auto *context : contexts) astcenc_context_free(context); }
    astcenc_context *get(int format, bool srgb) {
        astcenc_context *&context = contexts[(format == 43 ? 0 : 1) + (srgb ? 2 : 0)];
        if (!context) {
            astcenc_config config;
            unsigned b = unsigned(MobileASTC::block(format));
            if (astcenc_config_init(srgb ? ASTCENC_PRF_LDR_SRGB : ASTCENC_PRF_LDR,
                                   b, b, 1, ASTCENC_PRE_FASTEST,
                                   ASTCENC_FLG_DECOMPRESS_ONLY, &config) != ASTCENC_SUCCESS ||
                astcenc_context_alloc(&config, 1, &context) != ASTCENC_SUCCESS) return nullptr;
        }
        return context;
    }
};
}

bool MobileASTC::decodeBGRA(const void *data, size_t size, int width, int height,
                           int format, void *pixels, size_t pixelBytes, bool srgb) {
    const int required = bytes(width, height, 1, format);
    if (!required || !data || !pixels || size < size_t(required) ||
        uint64_t(width) * uint64_t(height) * 4 > pixelBytes) return false;
    // Contexts are small, decompression-only, and private to the calling thread.
    // Reuse their block tables across mip uploads without sharing codec state.
    static thread_local DecodeContexts contexts;
    astcenc_context *context = contexts.get(format, srgb);
    if (!context) return false;
    void *slice = pixels;
    astcenc_image output{unsigned(width), unsigned(height), 1, ASTCENC_TYPE_U8, &slice};
    const astcenc_swizzle swizzle{ASTCENC_SWZ_B, ASTCENC_SWZ_G, ASTCENC_SWZ_R, ASTCENC_SWZ_A};
    return astcenc_decompress_image(context, static_cast<const uint8_t *>(data), size,
                                   &output, &swizzle, 0) == ASTCENC_SUCCESS;
}
