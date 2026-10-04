/** @file world_player.cpp
 * @brief Handle player weapons, firing, input, and incoming damage.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>

namespace alien {
namespace {
bool isLitFloor(int tile) {
    return tile >= tile_id::LitFloorLeft && tile <= tile_id::LitFloorRight;
}
}
void World::selectWeapon(int weapon) {
    if (weapon < 0 || weapon >= rules::WeaponCount || !inventory.owned[weapon] || player.state == rules::StateAttack) { return; }
    inventory.weapon = weapon;
    displayedWeapon_ = -1;
    startAnimation(player, rules::StateIdle);
}
void World::cycleWeapon(int delta) {
    int weapon = inventory.weapon;
    for (int attempt = 0; attempt < rules::WeaponCount; ++attempt) {
        weapon = (weapon + delta + rules::WeaponCount) % rules::WeaponCount;
        if (inventory.owned[weapon]) { selectWeapon(weapon); return; }
    }
}
void World::firePlayer() {
    --bob_;
    int height = rules::MuzzleHeight + bob_;
    int vertical = 0;
    const int weapon = inventory.weapon;
    if (weapon == rules::Pistol) { height = rules::PistolMuzzleHeight + bob_ - crouch; vertical = rules::PistolAim; }
    if (weapon == rules::Flamethrower) { height = rules::FlamethrowerMuzzleHeight + bob_ * 2; vertical = rules::FlamethrowerAim; }
    sounds.push_back(sound::WeaponFire[weapon]);
    addProjectile(weapon, player.x + player.direction * rules::MuzzleOffsetX, player.y + height, player.direction, vertical, false);
    if (bob_ < 0) { bob_ = rules::RecoilBobFrames; }
    if (weapon > rules::Pistol) {
        --inventory.ammo[weapon];
        if (inventory.ammo[weapon] <= 0) {
            inventory.ammo[weapon] = 0;
            int replacement = weapon - 1;
            while (replacement > rules::Pistol && (!inventory.owned[replacement] || inventory.ammo[replacement] <= 0)) { --replacement; }
            inventory.weapon = replacement;
        }
    }
}
void World::updatePlayer(const Input& input) {
    int feetRow = player.y / TileSize;
    if (playerFootRowOverride_ >= 0) { feetRow = playerFootRowOverride_; }
    if (inventory.armor != rules::HeavyArmor) {
        const int tile = tileAt(player.x / TileSize, feetRow);
        const int below = tileAt(player.x / TileSize, feetRow - 1);
        playerLight = 0;
        if (isLitFloor(tile)) { playerLight = tile - tile_id::LightPartOffset; }
        else if (isLitFloor(below)) { playerLight = below - tile_id::LightPartOffset; }
        if (playerLight != 0 && player.direction < 0) { playerLight = tile_id::MirroredLightPartSum - playerLight; }
    }
    if (player.state == rules::StateWalk && !input.left && !input.right && !input.fire && !input.down && !input.jump
        && airState == rules::AirGrounded) {
        // Releasing all keys first shows the last walk frame, then the idle frame.
        const int end = walkAnimations_[std::min(inventory.weapon, rules::PoseCount - 1)].last;
        if (player.frame < end) { player.frame = end; return; }
        startAnimation(player, rules::StateIdle);
    }
    if (player.state == rules::StateAttack) {
        const int previousFrame = player.frame;
        const auto& fireFrames = attackAnimations_[inventory.weapon].fireFrames;
        const bool continuing = advanceAnimation(player);
        // Finish the old animation before the last shot; depleted-ammo idle frames retain the old weapon.
        if (!continuing) { startAnimation(player, rules::StateIdle); }
        bool shoot = !continuing;
        if (!fireFrames.empty()) { shoot = std::find(fireFrames.begin(), fireFrames.end(), previousFrame) != fireFrames.end(); }
        if (shoot) { firePlayer(); }
    }
    if (riding_ < 0) {
        gravity(player);
    }
}
void World::applyPlayerInput(const Input& input) {
    if (input.weapon >= 0) { selectWeapon(input.weapon); }
    if (input.cycleWeapon != 0) { cycleWeapon(input.cycleWeapon); }
    if (input.jump) {
        if (dead) { return; }
        if (airState != rules::AirGrounded && airState != rules::AirLanding) { return; }
        if (crouch != 0) {
            if (player.state == rules::StateIdle) { crouch = 0; }
        }
        else if (player.supportY >= 0) {
            // Jump can cancel standing fire; horizontal movement and another shot remain attack-locked.
            riding_ = -1; player.velocityY = assets_.entities[rules::PlayerType].jump;
            player.supportY = -1; airState = rules::AirJumping; startAnimation(player, rules::StateAir);
        }
    } else if (input.fire && inventory.ammo[inventory.weapon] > 0) {
        if (dead) { return; }
        if (player.state == rules::StateAttack || airState != rules::AirGrounded) { return; }
        if (timeMs - player.attackStarted > assets_.projectiles[inventory.weapon].cooldownMs) {
            if (inventory.weapon > rules::Pistol) { crouch = 0; }
            startAnimation(player, rules::StateAttack); player.attackStarted = timeMs;
        }
    } else if (input.left != input.right) {
        if (player.state == rules::StateAttack || airState != rules::AirGrounded) { return; }
        crouch = 0;
        int direction = 1;
        if (input.left) { direction = -1; }
        moveActor(player, direction);
    } else if (input.down) {
        if (dead) { return; }
        if (airState != rules::AirGrounded && airState != rules::AirLanding) { return; }
        if (selectedTerminal_ >= 0) {
            objects[selectedTerminal_].direction = -1;
            sounds.push_back(sound::TerminalTravel);
        } else if (crouch == 0 && player.state == rules::StateIdle) { crouch = assets_.crouch.height; }
        else if (player.state == rules::StateIdle) { lookingDown_ = true; }
    }
}

void World::damagePlayer(int amount) {
    if (cheat.enabled()) { return; }
    if (dead || complete) { return; }
    // Light armor rounds 5/16 of the damage up; heavy armor rounds half up.
    if (inventory.armor == rules::LightArmor) {
        amount = (amount * rules::LightArmorNumerator + rules::LightArmorDenominator - 1) / rules::LightArmorDenominator;
    }
    if (inventory.armor == rules::HeavyArmor) { amount = (amount + 1) / 2; }
    player.health = std::max(0, player.health - amount);
    beginHealthFlash(false); sounds.push_back(sound::Hurt);
    if (player.health == 0) { dead = true; player.alive = false; }
}
}
