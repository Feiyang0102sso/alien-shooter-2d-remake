/** @file geometry.h
 * @brief Share the original endpoint rectangle test and clamped approach step.
 */
#pragma once
#include <algorithm>

namespace alien {
inline bool intersects(int left, int right, int bottom, int top, int otherLeft, int otherRight, int otherBottom, int otherTop) {
    // Original collision tests endpoints; crossing thin rectangles can miss when all endpoints lie outside.
    const bool ownXInside = (left >= otherLeft && left <= otherRight) || (right >= otherLeft && right <= otherRight);
    if (ownXInside && ((top >= otherBottom && top <= otherTop) || (bottom >= otherBottom && bottom <= otherTop))) { return true; }
    const bool otherXInside = (otherLeft >= left && otherLeft <= right) || (otherRight >= left && otherRight <= right);
    return otherXInside && ((otherTop >= bottom && otherTop <= top) || (otherBottom >= bottom && otherBottom <= top));
}
inline int toward(int current, int target, int amount) {
    return current + std::clamp(target - current, -amount, amount);
}
}
