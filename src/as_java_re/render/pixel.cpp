/** @file pixel.cpp
 * @brief Rasterize MIDP-style integer lines, borders, triangles, and shadows.
 */
#include "render/pixel.h"
#include <cstdlib>

namespace alien {
Color rgb(unsigned int value) {
    return Color{static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value), 255};
}
void pixelBorder(int x, int y, int width, int height, Color color) {
    // Use integer filled rectangles to avoid OpenGL half-pixel endpoint expansion.
    DrawRectangle(x, y, width, 1, color);
    DrawRectangle(x, y + height - 1, width, 1, color);
    DrawRectangle(x, y + 1, 1, height - 2, color);
    DrawRectangle(x + width - 1, y + 1, 1, height - 2, color);
}
void pixelTriangle(int x, int y, int width, int direction, Color color) {
    while (width > 0) {
        DrawRectangle(x, y, width, 1, color);
        ++x; y += direction; width -= 2;
    }
}
void pixelShadow(int x, int y, int width, Color color) {
    // Match the four-pixel MIDP ellipse: empty first row and full middle rows, without smoothing.
    int inset = 1;
    int edgeWidth = width - 1;
    if (width == 15) { inset = 2; edgeWidth = 12; }
    DrawRectangle(x + inset, y + 1, edgeWidth, 1, color);
    DrawRectangle(x, y + 2, width, 1, color);
    DrawRectangle(x + inset, y + 3, edgeWidth, 1, color);
}
void pixelLine(int x, int y, int endX, int endY, Color color) {
    // MIDP lines include both endpoints, unlike OpenGL line rasterization.
    const int width = std::abs(endX - x), height = -std::abs(endY - y);
    int stepX = 1, stepY = 1;
    if (x > endX) { stepX = -1; }
    if (y > endY) { stepY = -1; }
    int error = width + height;
    while (true) {
        DrawPixel(x, y, color);
        if (x == endX && y == endY) { break; }
        const int twiceError = error * 2;
        if (twiceError >= height) { error += height; x += stepX; }
        if (twiceError <= width) { error += width; y += stepY; }
    }
}
}
