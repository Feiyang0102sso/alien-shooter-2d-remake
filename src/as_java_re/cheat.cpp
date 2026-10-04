/** @file cheat.cpp
 * @brief Match CHGOD and manage reversible invulnerability and weapon grants.
 */
#include "cheat.h"
#include "world.h"
#include <iostream>
#include <string_view>

namespace alien {
namespace {
constexpr std::string_view CheatCode = "chgod";
}

void Cheat::input(int character, World& world, const Assets& assets) {
    if (character >= 'A' && character <= 'Z') { character += 'a' - 'A'; }
    if (character == CheatCode[matched_]) { ++matched_; }
    else {
        matched_ = 0;
        if (character == CheatCode[0]) { matched_ = 1; }
    }
    if (matched_ != CheatCode.size()) { return; }
    matched_ = 0;
    if (enabled_) { disable(world); }
    else if (!world.dead && !world.complete) { enable(world, assets); }
}

void Cheat::enable(World& world, const Assets& assets) {
    previousOwned_ = world.inventory.owned;
    previousAmmo_ = world.inventory.ammo;
    previousWeapon_ = world.inventory.weapon;
    for (int weapon = 0; weapon < 5; ++weapon) {
        world.inventory.owned[weapon] = true;
        if (weapon > 0) { world.inventory.ammo[weapon] = assets.projectiles[weapon].capacity; }
    }
    enabled_ = true;
    std::cout << "CHGOD enabled.\n";
}

void Cheat::disable(World& world) {
    resetInput();
    if (!enabled_) { return; }
    enabled_ = false;
    world.inventory.owned = previousOwned_;
    world.inventory.ammo = previousAmmo_;
    // Revocation interrupts an attack so a temporary weapon cannot remain selected.
    world.player.state = 0;
    world.selectWeapon(previousWeapon_);
    std::cout << "CHGOD disabled.\n";
}

void Cheat::resetInput() { matched_ = 0; }
}
