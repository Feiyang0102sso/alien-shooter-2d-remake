/** @file world.cpp
 * @brief Own the world clock, random stream, messages, camera, and tick order.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>
#include <cmath>

namespace alien {
namespace {
// Animation names in character_animations.json, indexed by pose or weapon.
const std::array<const char*, rules::PoseCount> IdleAnimationNames{"player_unarmed_idle", "player_machine_gun_idle", "player_weapon_idle"};
const std::array<const char*, rules::PoseCount> WalkAnimationNames{"player_unarmed_walk", "player_machine_gun_walk", "player_weapon_walk"};
const std::array<const char*, rules::WeaponCount> AttackAnimationNames{
    "player_unarmed_attack", "player_machine_gun_attack", "player_flamethrower_attack", "player_rocket_attack", "player_blaster_attack"};
}

World::World(const Assets& assets) : assets_(assets) {
    for (int type = 0; type <= rules::MaxEnemyType; ++type) { collisionHeights_[type] = assets_.entities[type].top; }
    for (int type = 1; type <= rules::MaxEnemyType; ++type) { enemyHealth_[type] = assets_.entities[type].health; }
    for (int pose = 0; pose < rules::PoseCount; ++pose) {
        idleAnimations_[pose] = assets_.animation(IdleAnimationNames[pose]);
        walkAnimations_[pose] = assets_.animation(WalkAnimationNames[pose]);
    }
    for (int weapon = 0; weapon < rules::WeaponCount; ++weapon) { attackAnimations_[weapon] = assets_.animation(AttackAnimationNames[weapon]); }
    for (int type = 1; type <= rules::MaxEnemyType; ++type) {
        const std::string prefix = "monster_" + std::to_string(type);
        enemyWalkAnimations_[type] = assets_.animation(prefix + "_walk");
        enemyAttackAnimations_[type] = assets_.animation(prefix + "_attack");
    }
}
void World::seedRandom(std::uint64_t seed) {
    randomState_ = seed; continuousRandom_ = true;
}

int World::randomBits(int bits) {
    randomState_ = (randomState_ * java_random::Multiplier + java_random::Increment) & java_random::Mask;
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(randomState_ >> (java_random::StateBits - bits)));
}

bool World::overlapsPlayer(const Object& object) const {
    if (!assets_.hasParameters(object.type)) { return false; }
    const auto& p = assets_.entities[object.type];
    return intersects(object.x - p.left, object.x + p.right, object.y - p.bottom, object.y + p.top,
                      playerBounds_[0], playerBounds_[1], playerBounds_[2], playerBounds_[3]);
}
void World::showMessage(const std::string& key) {
    message = assets_.text.message(key);
}
void World::showTutorial(int bit, const std::string& key) {
    // Each tutorial message appears once per save; the bit set is persisted with the checkpoint.
    const unsigned int mask = 1U << bit;
    if ((inventory.tutorialFlags & mask) != 0) { return; }
    inventory.tutorialFlags |= mask;
    showMessage(key);
}
void World::dismissMessage() { message.clear(); }
void World::prepareMessage() {
    // Land the player before a message without updating enemies or the level clock.
    while (airState != rules::AirGrounded && player.alive && player.y >= 0) {
        gravity(player);
        updateCamera(false);
        updateVisuals();
        updateActorHeights();
    }
    startAnimation(player, rules::StateIdle);
    updateVisuals();
    updateActorHeights();
}

void World::updateCamera(bool lookingDown) {
    int anchorX = rules::CameraLead;
    if (player.direction < 0) { anchorX = ScreenWidth - rules::CameraLead; }
    int anchorY = rules::CameraLead;
    if (lookingDown) { anchorY = ScreenHeight - rules::CameraLookDownMargin; }
    cameraX = toward(cameraX, player.x - anchorX, rules::CameraFollowSpeed);
    cameraBottom = toward(cameraBottom, player.y - anchorY, std::max(rules::CameraFollowSpeed, std::abs(player.velocityY)));
    cameraX = std::clamp(cameraX, 0, std::max(0, level.width * TileSize - ScreenWidth));
    cameraBottom = std::clamp(cameraBottom, 0, std::max(0, level.height * TileSize - ScreenHeight));
}
bool World::advanceCameraTransition(bool downHeld) {
    if (!downHeld) { lookingDown_ = false; }
    const int previousX = cameraX, previousY = cameraBottom;
    int anchorX = rules::CameraLead, anchorY = rules::CameraLead;
    if (player.direction < 0) { anchorX = ScreenWidth - rules::CameraLead; }
    if (lookingDown_) { anchorY = ScreenHeight - rules::CameraLookDownMargin; }
    const int targetX = std::clamp(player.x - anchorX, 0, std::max(0, level.width * TileSize - ScreenWidth));
    const int targetY = std::clamp(player.y - anchorY, 0, std::max(0, level.height * TileSize - ScreenHeight));
    cameraX = toward(cameraX, targetX, rules::CameraTransitSpeed);
    cameraBottom = toward(cameraBottom, targetY, rules::CameraTransitSpeed);
    const bool finished = previousX == cameraX && previousY == cameraBottom;
    if (!finished) {
        // Teleport repaints advance animations, shared collision heights, and powered-object RNG state.
        updateVisuals();
        updateActorHeights();
    }
    return finished;
}
void World::tick(const Input& input) {
    sounds.clear();
    if (dead || complete || !message.empty()) { return; }
    if (!input.down) { lookingDown_ = false; }
    timeMs += TickMilliseconds; ++ticks;
    if (cameraTransit_) {
        // Teleport camera movement is a physics-paused phase that the desktop layer can display incrementally.
        while (!advanceCameraTransition(input.down)) {}
        cameraTransit_ = false;
        applyPlayerInput(input);
        updateVisuals(); updateActorHeights();
        return;
    }
    if (power) {
        if (!lightsOn) { setLighting(true, false); }
        else if (randomBits(8) < rules::LightFlickerChance) { setLighting(false, true); }
    }
    updatePlayer(input);
    updateCamera(lookingDown_ && input.down);
    const int activeSourceCount = updateObjects();
    updateEnemies();
    if (!cleared && activeEnemies() == 0 && activeSourceCount == 0) { cleared = true; showMessage(message_key::AreaCleared); }
    updateProjectiles();
    applyPlayerInput(input);
    updateVisuals();
    updateActorHeights();
}
}
