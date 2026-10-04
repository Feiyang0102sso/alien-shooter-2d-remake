/** @file world_actor.cpp
 * @brief Animate actors, apply shared movement, gravity, and fall damage.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>

namespace alien {
namespace {
int poseOf(int weapon) {
    return std::min(weapon, rules::PoseCount - 1);
}
}
int World::actorHeight(const Actor& actor) const {
    return collisionHeights_[actor.type];
}
int World::frameHeight(const Actor& actor) const {
    int height = 0;
    for (int part : assets_.frames.at(actor.frame)) {
        if (part > 0) { height = std::max(height, -assets_.parts[part][0].offsetY); }
    }
    if (actor.type == rules::PlayerType) { height -= crouch; }
    return std::max(1, height);
}
void World::updateActorHeights() {
    if (player.alive) { collisionHeights_[rules::PlayerType] = frameHeight(player); }
    // Original paint traverses visible enemies in reverse slot order and writes shared type heights.
    // Preserve that order so the desktop refresh rate cannot change the next tick's hit results.
    for (auto enemy = enemies.rbegin(); enemy != enemies.rend(); ++enemy) {
        if (!enemy->alive) { continue; }
        if (enemy->direction == 0 && (enemy->behavior & 1) == 0) { continue; }
        const auto& bounds = assets_.entities[enemy->type];
        if (enemy->x + bounds.right < cameraX - rules::VisibilityMargin
            || enemy->x - bounds.left > cameraX + ScreenWidth + rules::VisibilityMargin) { continue; }
        if (enemy->y + collisionHeights_[enemy->type] < cameraBottom || enemy->y - bounds.bottom > cameraBottom + ScreenHeight) { continue; }
        collisionHeights_[enemy->type] = frameHeight(*enemy);
    }
}
// Walk and air states share the walk range; idle has its own frame only for the player.
const Animation& World::animationFor(const Actor& actor) const {
    if (actor.type == rules::PlayerType) {
        if (actor.state == rules::StateAttack) { return attackAnimations_[inventory.weapon]; }
        return walkAnimations_[poseOf(inventory.weapon)];
    }
    if (actor.state == rules::StateAttack) { return enemyAttackAnimations_[actor.type]; }
    return enemyWalkAnimations_[actor.type];
}
void World::startAnimation(Actor& actor, int state) {
    actor.state = state;
    if (actor.type == rules::PlayerType && state == rules::StateIdle) {
        actor.frame = idleAnimations_[poseOf(inventory.weapon)].first;
        return;
    }
    actor.frame = animationFor(actor).first;
}
bool World::advanceAnimation(Actor& actor) {
    const Animation& animation = animationFor(actor);
    // Step numerically and skip empty frames; a weapon change mid-animation keeps the old frame number.
    ++actor.frame;
    while (actor.frame <= animation.last && assets_.frames.at(actor.frame)[0] == 0) { ++actor.frame; }
    if (actor.frame > animation.last) {
        if (actor.state == rules::StateWalk) { actor.frame = animation.first; return true; }
        actor.frame = animation.last;
        return false;
    }
    return true;
}

void World::moveActor(Actor& actor, int direction, bool airborne) {
    if (dead || actor.y < 0) { return; }
    const auto& p = assets_.entities[actor.type];
    if (actor.direction != direction) { actor.direction = direction; return; }
    if (actor.supportY >= 0) {
        if (actor.state != rules::StateWalk) { startAnimation(actor, rules::StateWalk); }
        else { advanceAnimation(actor); }
    }
    int speed = p.speedX;
    // Airborne steering moves at half speed.
    if (airborne) { speed /= 2; }
    const bool isPlayer = actor.type == rules::PlayerType;
    if (direction < 0 && actor.x - speed - p.left < 0) {
        actor.x = p.left + 1;
        if (!isPlayer) { actor.supportY = -1; }
        return;
    }
    if (direction > 0 && actor.x + speed + p.right - 1 >= level.width * TileSize) {
        actor.x = level.width * TileSize - p.right - 1;
        if (!isPlayer) { actor.supportY = -1; }
        return;
    }
    const int candidateX = actor.x + speed * direction;
    int side = (candidateX + p.right) / TileSize;
    if (direction < 0) { side = (candidateX - p.left) / TileSize; }
    int feetCell = actor.y / TileSize;
    if (isPlayer && playerFootRowOverride_ >= 0) { feetCell = playerFootRowOverride_; }
    const int topCell = (actor.y + actorHeight(actor)) / TileSize;
    for (int row = topCell; row > feetCell; --row) {
        if (collisionAt(side, row) > 0) {
            if (!isPlayer) { actor.supportY = -1; }
            return;
        }
    }
    int supportCell = candidateX / TileSize;
    if (!isPlayer) { supportCell = side; }
    const int height = collisionAt(supportCell, feetCell);
    const int surface = feetCell * TileSize + height;
    if (height > 0 && actor.velocityY <= 0) {
        if (surface - actor.y > rules::StepUpTolerance) {
            if (!isPlayer) { actor.supportY = -1; }
            return;
        }
        actor.y = surface; actor.supportY = surface;
        if (isPlayer) {
            playerFootRowOverride_ = -1;
            if (airState != rules::AirGrounded && actor.state != rules::StateWalk) { startAnimation(actor, rules::StateIdle); }
            airState = rules::AirGrounded;
        }
    } else {
        actor.supportY = -1;
        if (!isPlayer) { return; }
        if (airState == rules::AirGrounded && riding_ < 0) { airState = rules::AirFalling; startAnimation(actor, rules::StateAir); }
    }
    actor.x = candidateX;
}

void World::gravity(Actor& actor) {
    if (actor.supportY >= 0) { return; }
    const bool isPlayer = actor.type == rules::PlayerType;
    // Original g() enters gravity before airborne movement; new support does not skip that gravity step.
    if (isPlayer && airState == rules::AirJumping) { moveActor(actor, actor.direction, true); }
    const int previousY = actor.y;
    int previousRow = previousY / TileSize;
    if (isPlayer && playerFootRowOverride_ >= 0) { previousRow = playerFootRowOverride_; }
    actor.y += actor.velocityY;
    if (isPlayer) { playerFootRowOverride_ = -1; }
    actor.velocityY += assets_.entities[actor.type].gravity;
    const int column = actor.x / TileSize;
    if (actor.y > previousY) {
        const int top = (actor.y + actorHeight(actor)) / TileSize;
        if (collisionAt(column, top) > 0) {
            actor.y = top * TileSize - actorHeight(actor) - 1; actor.velocityY = 0;
        }
        return;
    }
    for (int row = previousRow; row >= std::max(0, actor.y / TileSize); --row) {
        const int height = collisionAt(column, row);
        const int surface = row * TileSize + height;
        if (height > 0 && surface > actor.y && surface <= previousY + TileSize) {
            actor.y = surface;
            actor.supportY = surface;
            // Broken crates clear support without necessarily entering an airborne state or interrupting fire.
            if (isPlayer && airState > rules::AirGrounded) {
                const auto& bounds = assets_.entities[rules::PlayerType];
                const int left = (actor.x - bounds.left) / TileSize;
                const int right = (actor.x + bounds.right) / TileSize;
                const int above = actor.y / TileSize + 1;
                if (collisionAt(left, above) > 0) { actor.x += bounds.left; }
                if (collisionAt(right, above) > 0) { actor.x -= bounds.right; }
                airState = rules::AirGrounded;
                startAnimation(actor, rules::StateIdle);
                // Apply gravity, compute landing damage from the updated velocity, then clear velocity.
                applyFallDamage();
            } else if (!isPlayer) {
                startAnimation(actor, rules::StateWalk);
            }
            actor.velocityY = 0;
            return;
        }
    }
    if (actor.y < 0 && isPlayer) { damagePlayer(rules::FatalDamage); }
}
void World::applyFallDamage() {
    if (player.velocityY < rules::FatalFallSpeed) { damagePlayer(rules::FatalDamage); }
    else if (player.velocityY < rules::DamagingFallSpeed) {
        damagePlayer(rules::FallDamageBase - rules::FallDamagePerSpeed * (player.velocityY - rules::DamagingFallSpeed));
    }
}
}
