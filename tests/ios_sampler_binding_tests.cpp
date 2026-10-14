#include "IOSSamplerBindingCache.hpp"
#include <cassert>
int main() {
    IOSSamplerBindingCache<16> first, second;
    unsigned calls = 0, lastUnit = 0, lastName = 0;
    auto bind = [&](unsigned unit, unsigned name) { ++calls; lastUnit = unit; lastName = name; };
    assert(first.bind(0, 17, bind) && calls == 1);
    assert(!first.bind(0, 17, bind));
    assert(first.bind(1, 17, bind) && calls == 2 && lastUnit == 1);
    assert(first.bind(0, 0, bind) && calls == 3 && lastName == 0);
    assert(first.bind(0, 17, bind) && calls == 4);
    assert(second.bind(0, 17, bind) && calls == 5);
    first.reset();
    assert(first.bind(0, 17, bind) && calls == 6);
    assert(first.bind(15, 0, bind) && calls == 7);
    assert(first.bind(15, 25, bind) && calls == 8);
}
