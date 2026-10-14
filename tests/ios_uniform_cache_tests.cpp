#include "IOSUniformCache.hpp"
#include <cassert>
#include <cstring>
#include <cstdint>
int main() {
    IOSUniformCache<8> first, second;
    float values[32]{};
    int calls = 0, offset = -1, slots = -1;
    float captured[32]{};
    auto upload = [&](int begin, int count, const float *p) {
        ++calls; offset = begin; slots = count;
        std::memcpy(captured, p, count * 4 * sizeof(float));
    };
    assert(first.update(0, 8, values, upload) && calls == 1 && slots == 8);
    assert(!first.update(0, 8, values, upload) && calls == 1);
    values[13] = .25f;
    assert(first.update(0, 8, values, upload) && calls == 2 && offset == 3 && slots == 1);
    assert(captured[1] == .25f);
    assert(second.update(0, 8, values, upload) && calls == 3);
    assert(!first.update(0, 8, values, upload));
    values[4] = .5f; values[20] = .75f;
    assert(first.update(0, 8, values, upload) && offset == 1 && slots == 5);
    first.reset();
    assert(first.update(0, 8, values, upload) && offset == 0 && slots == 8);
    std::uint32_t bits = 0x80000000u;
    std::memcpy(values, &bits, 4);
    assert(first.update(0, 1, values, upload));
    bits = 0x7fc01234u; std::memcpy(values, &bits, 4);
    assert(first.update(0, 1, values, upload));
    assert(!first.update(0, 1, values, upload));
    assert(!first.update(0, 0, values, upload));
    assert(first.update(8, 1, values, upload) && slots == 1);
}
