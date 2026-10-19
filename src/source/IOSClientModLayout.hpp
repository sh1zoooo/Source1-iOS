#pragma once
#include <algorithm>
struct IOSClientModLayout {
    int x, y, width, height, margin, row, font;
    static IOSClientModLayout fit(int screenWidth, int screenHeight) {
        const int unit = std::max(1, std::min(screenWidth, screenHeight));
        const int margin = std::max(8, unit / 35);
        const int h = std::max(1, screenHeight - 4 * margin);
        const int w = std::max(1, std::min(screenWidth - 4 * margin, h * 17 / 10));
        return {(screenWidth-w)/2, (screenHeight-h)/2, w, h, margin,
                std::max(20, h/9), std::max(12, unit/32)};
    }
};
