/** @file pixel.h
 * @brief Rasterize MIDP-style integer lines, borders, triangles, and shadows.
 */
#pragma once
#include <raylib.h>

namespace alien {
// Convert a 0xRRGGBB value into an opaque raylib color.
Color rgb(unsigned int value);
void pixelBorder(int x, int y, int width, int height, Color color);
void pixelTriangle(int x, int y, int width, int direction, Color color);
void pixelShadow(int x, int y, int width, Color color);
void pixelLine(int x, int y, int endX, int endY, Color color);
}
