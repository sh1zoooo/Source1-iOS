#include "MobileTextureMips.hpp"
#include <cstdlib>
#include <cstdio>
static void require(bool value) { if (!value) std::abort(); }
int main() {
    require(MobileTextureMipSkip(2048, 2048, 12) == 2);
    require(MobileTextureMipSkip(4096, 2048, 13) == 3);
    require(MobileTextureMipSkip(256, 512, 10) == 0);
    require(MobileTextureMipSkip(2048, 2048, 1) == 0);
    require(MobileTextureMipSkip(2048, 2048, 2) == 1);
    require(MobileTextureMipSkip(8192, 4, 14) == 0);
    require(MobileTextureMipSkip(8192, 8, 14) == 1);
    require(MobileTextureMipSkip(0, 0, 0) == 0);
    std::puts("Mobile world texture mips: sizes, aspect, missing mips, compressed edges PASS");
}
