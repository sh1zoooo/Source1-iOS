#include "TouchFingerSlots.hpp"
#include "TouchMenuCoordinates.hpp"
#include <limits>
#include <cstdio>
#include <cstdlib>
static void require(bool v) { if (!v) std::abort(); }
int main() {
    auto bits = [](float f) { int v; std::memcpy(&v, &f, sizeof(v)); return v; };
    int x = -1, y = -1;
    require(TouchMenuCoordinates(0, bits(.1f), bits(.5f), 390, 480, x, y));
    require(x == 39 && y == 240);
    require(TouchMenuCoordinates(0, bits(1.f), bits(1.f), 844, 390, x, y));
    require(x == 843 && y == 389);
    require(TouchMenuCoordinates(0, bits(-.2f), bits(2.f), 844, 390, x, y));
    require(x == 0 && y == 389);
    require(!TouchMenuCoordinates(1, bits(.1f), bits(.5f), 390, 480, x, y));
    require(!TouchMenuCoordinates(0, bits(std::numeric_limits<float>::quiet_NaN()), bits(.5f), 390, 480, x, y));
    require(!TouchMenuCoordinates(0, bits(.1f), bits(.5f), 0, 480, x, y));
    TouchFingerSlots slots;
    require(slots.press(INT64_MAX) == 0);
    require(slots.press(INT64_MIN) == 1);
    require(slots.press(INT64_MAX) == 0);
    for (int i = 2; i < 10; ++i) require(slots.press(i) == i);
    require(slots.press(999) == -1);
    slots.release(INT64_MAX);
    require(slots.find(INT64_MAX) == -1);
    require(slots.press(999) == 0);
    slots.release(555);
    require(slots.find(INT64_MIN) == 1);
    slots.reset();
    require(slots.find(999) == -1);
    require(slots.press(-37) == 0);
    std::puts("64-bit contact IDs, duplicates, capacity, release, reuse, reset: PASS");
}
