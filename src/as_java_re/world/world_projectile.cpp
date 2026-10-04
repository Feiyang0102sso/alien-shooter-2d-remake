/** @file world_projectile.cpp
 * @brief Move projectiles and resolve hits against objects, actors, and terrain.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>
#include <cmath>

namespace alien {
void World::addProjectile(int type, int x, int y, int direction, int vertical, bool hostile) {
    const auto& p = assets_.projectiles.at(type);
    Projectile projectile;
    projectile.type = type; projectile.x = x; projectile.y = y;
    projectile.dx = p.speedX * direction; projectile.dy = p.speedY * vertical;
    projectile.remainingX = p.rangeX; projectile.remainingY = p.rangeY;
    projectile.damage = p.damage;
    if (hostile) { projectile.owner = 1; }
    if (type == rules::RocketProjectile) { projectile.life = rules::RocketLife; }
    if (type == rules::SmokeProjectile) { projectile.life = rules::SmokeLife; }
    projectiles.push_back(projectile);
}
bool World::projectileHits(const Projectile& projectile, int nextX, int nextY, int x, int y, const EntityParameters& p) const {
    int left = std::min(projectile.x, nextX), right = std::max(projectile.x, nextX);
    int bottom = std::min(projectile.y, nextY), top = std::max(projectile.y, nextY);
    if (projectile.type == rules::ExplosionProjectile) {
        left = projectile.x - projectile.radius; right = projectile.x + projectile.radius;
        bottom = projectile.y - projectile.radius; top = projectile.y + projectile.radius;
    }
    return intersects(left, right, bottom, top, x - p.left, x + p.right, y - p.bottom, y + p.top);
}

void World::impactProjectile(Projectile& projectile, int left, int right) {
    const int originalType = projectile.type;
    if (originalType < rules::FirstHeavyProjectile) { projectile.type = rules::ImpactProjectile; sounds.push_back(sound::BulletImpact); }
    else {
        projectile.type = rules::ExplosionProjectile;
        if (originalType != rules::ExplosionProjectile) { sounds.push_back(sound::Explosion); }
    }
    if (projectile.dx > 0) { projectile.x = left + rules::ImpactInset; }
    else if (projectile.dx < 0) { projectile.x = right - rules::ImpactInset; }
    projectile.originalDirection = projectile.dx;
    projectile.dx = 0; projectile.dy = 0;
    if (originalType != projectile.type) { projectile.life = rules::DefaultProjectileLife; }
}
void World::updateProjectiles() {
    // Traverse newest projectiles first and defer new impact fragments until the next tick.
    for (int index = static_cast<int>(projectiles.size()) - 1; index >= 0; --index) {
        auto& projectile = projectiles[index];
        const Projectile sweep = projectile;
        const int originalType = projectile.type;
        int nextX = projectile.x + projectile.dx, nextY = projectile.y + projectile.dy;
        const int remainingX = projectile.remainingX - std::abs(projectile.dx);
        const int remainingY = projectile.remainingY - std::abs(projectile.dy);
        if (remainingX <= 0 || remainingY <= 0 || nextX < 0 || nextY < 0 || nextX >= level.width * TileSize || nextY >= level.height * TileSize) {
            projectile.alive = false; continue;
        }
        bool hitTerrain = originalType != rules::ImpactProjectile;
        bool reachedActors = true;
        bool removeBullet = false;
        if (originalType < rules::FirstEffectProjectile) {
            for (int objectIndex = static_cast<int>(objects.size()) - 1; objectIndex >= 0; --objectIndex) {
                auto& object = objects[objectIndex];
                if (!object.active || object.type < object_type::BreakableFirst || object.type > object_type::BreakableLast) { continue; }
                const auto& p = assets_.entities[object.type];
                if (!projectileHits(sweep, sweep.x + sweep.dx, sweep.y + sweep.dy, object.x, object.y, p)) { continue; }
                impactProjectile(projectile, object.x - p.left, object.x + p.right);
                object.health -= projectile.damage;
                nextX = projectile.x; nextY = projectile.y;
                if (object.health <= 0) { destroyObject(objectIndex, projectile); }
                if (originalType != rules::ExplosionProjectile) { reachedActors = false; break; }
            }
            hitTerrain = reachedActors;
            if (reachedActors) {
                int lastActor = static_cast<int>(enemies.size());
                if (projectile.owner == 1) { lastActor = 0; }
                int firstActor = 0;
                if (projectile.owner == 2) { firstActor = 1; }
                for (int actorIndex = lastActor; actorIndex >= firstActor; --actorIndex) {
                    Actor* target = &player;
                    if (actorIndex > 0) { target = &enemies[actorIndex - 1]; }
                    if (!target->alive) { continue; }
                    auto p = assets_.entities[target->type]; p.top = actorHeight(*target);
                    if (!projectileHits(sweep, sweep.x + sweep.dx, sweep.y + sweep.dy, target->x, target->y, p)) { continue; }
                    impactProjectile(projectile, target->x - p.left, target->x + p.right);
                    if (projectile.owner == 0) { projectile.owner = 2; }
                    hitTerrain = false; removeBullet = originalType < rules::FirstHeavyProjectile;
                    if (actorIndex == 0) {
                        damagePlayer(projectile.damage);
                        spawnBlood(player.x, projectile.y, dead);
                        if (dead) { decorateCorpse(rules::PlayerType, player.x / TileSize, player.y / TileSize, player.direction); }
                    } else { damageEnemy(*target, projectile.damage, projectile.y, sweep.dx); }
                    if (originalType != rules::ExplosionProjectile) { break; }
                }
            }
            if (removeBullet) { projectile.alive = false; continue; }
            hitTerrain = hitTerrain && projectile.type != rules::ExplosionProjectile;
        } else if (originalType == rules::BloodProjectile || originalType == rules::DebrisProjectile) { projectile.dy -= rules::ProjectileGravity; }

        const int cellX = nextX / TileSize;
        int cellY = nextY / TileSize;
        if (hitTerrain && nextY < cellY * TileSize + collisionAt(cellX, cellY)) {
            if (projectile.type >= rules::FirstEffectProjectile) { projectile.alive = false; continue; }
            const int hitType = projectile.type;
            projectile.type = rules::ExplosionProjectile;
            if (hitType < rules::FirstHeavyProjectile) { projectile.type = rules::ImpactProjectile; }
            projectile.x = (cellX + 1) * TileSize;
            if (sweep.dx > 0) { projectile.x = cellX * TileSize + rules::WallImpactInset; }
            projectile.originalDirection = projectile.dx;
            projectile.dx = 0; projectile.dy = 0; projectile.life = rules::DefaultProjectileLife;
            int wallDirection = 1;
            if (sweep.dx > 0) { wallDirection = -1; }
            // Pillars store accumulated damage in their collision height and collapse past the threshold.
            if (hitType < rules::FirstPillarProofProjectile && tileAt(cellX, cellY) == tile_id::Pillar
                && tileAt(cellX - wallDirection, cellY) == tile_id::Wall) {
                sounds.push_back(sound::PillarHit);
                if (tileAt(cellX, cellY - 1) != tile_id::PillarTop) { --cellY; }
                const int damage = (collisionAt(cellX, cellY) + projectile.damage) & rules::PillarDamageMask;
                setCollision(cellX, cellY, damage);
                for (int speed = rules::PillarDebrisFastSpeed; speed >= rules::PillarDebrisSlowSpeed; --speed) {
                    addProjectile(rules::DebrisProjectile, projectile.x, nextY, wallDirection * speed, randomBits(31) % rules::PillarDebrisSpreadY, false);
                    projectiles.back().variant = randomBits(31) % 2; --projectiles.back().life;
                }
                if (damage > rules::PillarBreakDamage) {
                    shake = rules::PillarBreakShake;
                    setCollision(cellX, cellY - 1, rules::PillarRubbleHeight); setTile(cellX, cellY - 1, tile_id::BrokenPillarTop);
                    for (int row = cellY; row <= cellY + 1; ++row) {
                        setCollision(cellX, row, 0); setTile(cellX, row, tile_id::WallHole); patchWall(cellX, row);
                        addProjectile(rules::ExplosionProjectile, projectile.x, row * TileSize + TileSize / 2, 0, 0, false);
                    }
                    setCollision(cellX, cellY + 2, 0); setTile(cellX, cellY + 2, tile_id::BrokenWallBase);
                }
            }
        } else {
            projectile.x = nextX; projectile.y = nextY;
            projectile.remainingX = remainingX; projectile.remainingY = remainingY;
        }
        switch (projectile.type) {
        case rules::FlameProjectile: case rules::ExplosionProjectile: case rules::ImpactProjectile: case rules::SmokeProjectile:
            if (--projectile.life < 0) { projectile.alive = false; }
            break;
        case rules::RocketProjectile: projectile.life = 1 - projectile.life; break;
        case rules::BloodProjectile: case rules::DebrisProjectile:
            if (--projectile.life < 0) { projectile.life = rules::ParticleLastFrame; }
            break;
        }
    }
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.alive; }), projectiles.end());
}
}
