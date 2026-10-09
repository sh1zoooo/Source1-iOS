#include "TouchFingerSlots.hpp"
#include <cstdio>
#include <cstdlib>
static void require(bool v) { if (!v) std::abort(); }
int main() {
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
