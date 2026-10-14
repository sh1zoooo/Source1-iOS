#pragma once
#include <cstddef>
#include <cstring>

// One cache per linked program and shader stage. Bitwise comparison preserves
// signed zero and NaN payloads; relinking must invalidate every slot.
template <std::size_t Slots = 256> class IOSUniformCache {
    float values_[Slots][4];
    bool valid_[Slots]{};
public:
    void reset() { std::memset(valid_, 0, sizeof(valid_)); }
    template <class Upload> bool update(int first, int count, const float *values, Upload upload) {
        if (count <= 0) return false;
        if (first < 0 || first >= static_cast<int>(Slots) || count > static_cast<int>(Slots) - first) {
            upload(0, count, values);
            return true;
        }
        int begin = count, end = 0;
        for (int i = 0; i < count; ++i) {
            if (!valid_[first + i] || std::memcmp(values_[first + i], values + i * 4, sizeof(values_[0]))) {
                if (begin == count) begin = i;
                end = i + 1;
            }
        }
        if (begin == count) return false;
        upload(begin, end - begin, values + begin * 4);
        for (int i = begin; i < end; ++i) {
            std::memcpy(values_[first + i], values + i * 4, sizeof(values_[0]));
            valid_[first + i] = true;
        }
        return true;
    }
};
