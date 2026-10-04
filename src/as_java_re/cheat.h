/** @file cheat.h
 * @brief Toggle CHGOD without persisting temporary equipment.
 */
#pragma once
#include <array>
#include <cstddef>

namespace alien {
class Assets;
class World;

class Cheat {
public:
    void input(int character, World& world, const Assets& assets);
    void disable(World& world);
    void resetInput();
    bool enabled() const { return enabled_; }
private:
    void enable(World& world, const Assets& assets);
    bool enabled_ = false;
    std::size_t matched_ = 0;
    std::array<bool, 5> previousOwned_{};
    std::array<int, 5> previousAmmo_{};
    int previousWeapon_ = 0;
};
}
