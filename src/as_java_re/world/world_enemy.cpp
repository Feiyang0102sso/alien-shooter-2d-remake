/** @file world_enemy.cpp
 * @brief Spawn, damage, and drive enemy behavior.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>
#include <cmath>

namespace alien {
void World::spawnEnemy(int type, int x, int y, int generatorObjectIndex, int generatorSlot) {
    if (type <= 0 || type == rules::EmptySpawnType || activeEnemies() >= rules::MaxEnemies) { return; }
    Actor enemy;
    enemy.type = std::min(type, rules::MaxEnemyType); enemy.x = x; enemy.y = y; enemy.source = generatorSlot;
    const auto& p = assets_.entities[enemy.type];
    enemy.health = enemyHealth_[enemy.type];
    enemy.direction = -1; enemy.supportY = y; enemy.shotType = p.damage;
    // Records above the last enemy type spawn that type with a different projectile.
    if (type > rules::MaxEnemyType) { enemy.shotType = type - rules::SpawnShotTypeOffset; }
    if (generatorSlot >= 0) {
        enemy.spawnTime = timeMs;
        enemy.attackStarted = timeMs;
        const auto& generator = objects[generatorObjectIndex];
        if (generator.type == object_type::TeleporterSource) { enemy.direction = 0; enemy.behavior = rules::EmergeBehavior; }
        else if (generator.x < cameraX - rules::VisibilityMargin || generator.x > cameraX + ScreenWidth + rules::SpawnVisibleMarginRight
                 || generator.y < cameraBottom - rules::SpawnVisibleMarginY || generator.y > cameraBottom + ScreenHeight + rules::SpawnVisibleMarginY) {
            enemy.x -= randomBits(31) % rules::OffscreenSpawnScatter;
        }
    }
    startAnimation(enemy, rules::StateIdle);
    for (auto& slot : enemies) { if (!slot.alive) { slot = enemy; return; } }
    enemies.push_back(enemy);
}
int World::activeEnemies() const {
    int count = 0;
    for (const auto& enemy : enemies) { if (enemy.alive) { ++count; } }
    return count;
}

void World::spawnBlood(int x, int y, bool fatal) {
    int count = 1;
    if (fatal) { count = 2; }
    for (int particle = 0; particle < count; ++particle) {
        // Preserve RNG order; blood particles affect subsequent random enemy turns.
        const int horizontal = randomBits(rules::BloodHorizontalBits) - rules::BloodHorizontalCenter;
        const int vertical = randomBits(3);
        addProjectile(rules::BloodProjectile, x, y, horizontal, vertical, false);
        if (fatal) { projectiles.back().variant = randomBits(1); }
        else {
            projectiles.back().variant = 1;
            if (randomBits(2) < 3) { projectiles.back().variant = 2; }
        }
        --projectiles.back().life;
    }
}
void World::damageEnemy(Actor& actor, int damage, int impactY, int incomingX) {
    actor.health -= damage;
    if (actor.health > 0) {
        if (incomingX > 0) { actor.direction = -1; }
        else if (incomingX < 0) { actor.direction = 1; }
        spawnBlood(actor.x, impactY, false);
        return;
    }
    actor.alive = false;
    inventory.score += rules::KillScorePerType * actor.type;
    scoreFlash_ = rules::ScoreFlashTicks;
    int deathSound = sound::Hurt;
    if (actor.type == 2 || actor.type == 4) { deathSound = sound::EnemyDeathTypes2And4; }
    if (actor.type == 3 || actor.type == 5) { deathSound = sound::EnemyDeathTypes3And5; }
    sounds.push_back(deathSound);
    spawnBlood(actor.x, impactY, true);
    decorateCorpse(actor.type, actor.x / TileSize, actor.y / TileSize, actor.direction);
    if (actor.source >= 0) { generators_[actor.source].lastSpawn = timeMs; }
}
void World::updateEnemies() {
    for (auto& enemy : enemies) {
        if (!enemy.alive) { continue; }
        const auto& p = assets_.entities[enemy.type];
        const int left = enemy.x - p.left, right = enemy.x + p.right;
        const int top = enemy.y + actorHeight(enemy), bottom = enemy.y - p.bottom;
        const int playerLeft = playerBounds_[0], playerRight = playerBounds_[1];
        const int playerTop = playerBounds_[3], playerBottom = playerBounds_[2];
        if (!intersects(left, right, bottom, top, playerLeft - rules::EnemyWakeRangeX, playerRight + rules::EnemyWakeRangeX,
                        playerBottom - rules::EnemyWakeRangeY, playerTop + rules::EnemyWakeRangeY)) { continue; }
        if (enemy.direction == 0) {
            --enemy.behavior;
            if (timeMs - enemy.spawnTime > rules::EmergeDurationMs) { enemy.direction = -1; }
            continue;
        }
        const int distance = std::abs(player.x - enemy.x);
        int facing = -1;
        if (player.x > enemy.x) { facing = 1; }
        const int initialDirection = enemy.direction;
        const int initialState = enemy.state;
        const int elapsed = timeMs - enemy.attackStarted;
        bool threat = intersects(left, enemy.x + rules::ThreatRange, bottom, top, playerLeft, playerRight, playerBottom, playerTop);
        if (initialDirection < 0) { threat = intersects(enemy.x - rules::ThreatRange, right, bottom, top, playerLeft, playerRight, playerBottom, playerTop); }
        bool canMove = enemy.health > 0 && enemy.supportY >= 0;
        bool wantsAttack = initialState == rules::StateAttack;
        const bool ranged = enemy.type >= rules::RangedEnemyType;
        int attackLeft = enemy.x, attackRight = right;
        if (ranged) { attackRight = enemy.x + rules::RangedAttackReach; }
        if (initialDirection < 0) {
            attackRight = enemy.x; attackLeft = left;
            if (ranged) { attackLeft = enemy.x - rules::RangedAttackReach; }
        }
        wantsAttack = wantsAttack || intersects(attackLeft, attackRight, bottom, top, playerLeft, playerRight, playerBottom, playerTop);
        if (!ranged) {
            canMove = canMove && !wantsAttack;
            if (initialState != rules::StateAttack) { wantsAttack = wantsAttack && elapsed > rules::MeleeCooldownMs; }
        } else {
            const int cooldown = assets_.projectiles[enemy.shotType].cooldownMs;
            if (enemy.behavior != 0) {
                wantsAttack = false; canMove = true;
                if (distance < ScreenWidth || elapsed > cooldown) { enemy.behavior = 0; }
            } else {
                canMove = canMove && !wantsAttack;
                if (initialState != rules::StateAttack) {
                    wantsAttack = wantsAttack && elapsed > cooldown * 2;
                    // Ranged enemies sometimes reposition instead of waiting for the next shot.
                    if (threat && !wantsAttack && distance > rules::RetreatMinDistance && randomBits(31) % 100 < rules::RetreatChancePercent) {
                        canMove = true; enemy.behavior = 1; enemy.attackStarted = timeMs - cooldown + rules::RetreatResumeMs;
                    }
                }
            }
        }
        if (randomBits(31) % 100 < rules::TurnChancePercent) { enemy.direction = -initialDirection; }
        if (threat) { enemy.direction = facing; }
        if (canMove) { moveActor(enemy, enemy.direction); }
        if (wantsAttack) {
            if (initialState != rules::StateAttack) {
                enemy.attackStarted = timeMs; startAnimation(enemy, rules::StateAttack);
                if (enemy.type == rules::ChargingEnemyType) { enemy.x += initialDirection * rules::ChargeOffset; }
            } else if (!advanceAnimation(enemy)) {
                startAnimation(enemy, rules::StateWalk);
                if (enemy.type == rules::ChargingEnemyType) { enemy.x -= initialDirection * rules::ChargeOffset; }
                if (!ranged) {
                    damagePlayer(enemy.shotType);
                    spawnBlood(player.x, enemy.y + rules::BloodSpawnHeight, false);
                }
                else {
                    int muzzle = right, height = rules::RangedMuzzleHeight;
                    if (enemy.direction < 0) { muzzle = left; }
                    if (enemy.type == rules::HeavyRangedEnemyType) { height = rules::HeavyRangedMuzzleHeight; }
                    addProjectile(enemy.shotType, muzzle, enemy.y + height - bob_, initialDirection, 0, true);
                    --bob_;
                    if (bob_ < 0) { bob_ = rules::RecoilBobFrames; }
                }
                if (dead) { break; }
            }
        }
        if (enemy.supportY == -1) {
            enemy.supportY = enemy.y;
            if (canMove) {
                enemy.direction = -initialDirection;
                moveActor(enemy, -initialDirection);
            }
        }
    }
}
}
