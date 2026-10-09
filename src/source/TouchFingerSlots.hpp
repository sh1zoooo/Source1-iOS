#pragma once
#include <array>
#include <cstdint>
// SDL contact IDs are opaque 64-bit values, never array indices.
class TouchFingerSlots {
public:
    static constexpr int count = 10;
    int find(std::int64_t id) const {
        for (int i = 0; i < count; ++i) if (used_[i] && ids_[i] == id) return i;
        return -1;
    }
    int press(std::int64_t id) {
        int existing = find(id);
        if (existing >= 0) return existing;
        for (int i = 0; i < count; ++i) if (!used_[i]) {
            used_[i] = true; ids_[i] = id; return i;
        }
        return -1;
    }
    void release(std::int64_t id) { int i = find(id); if (i >= 0) used_[i] = false; }
    void reset() { used_.fill(false); }
private:
    std::array<std::int64_t, count> ids_{};
    std::array<bool, count> used_{};
};
