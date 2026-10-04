/** @file reference_snapshot.h
 * @brief Serialize original-compatible world states for regression comparisons.
 */
#pragma once
#include "world/world.h"

inline alien::Json projectileSnapshot(const alien::World& world) {
    alien::Json shots = alien::Json::array();
    for (auto shot = world.projectiles.rbegin(); shot != world.projectiles.rend(); ++shot) {
        int visual = shot->originalDirection;
        if (shot->type == 7 || shot->type == 8) { visual = shot->variant; }
        shots.push_back({shot->type, shot->x, shot->y, shot->dx, shot->dy, shot->remainingX,
                         shot->remainingY, shot->damage, shot->radius, visual, shot->life, shot->owner});
    }
    return shots;
}

// Reference tests retain fields directly mapped to original arrays, without inferring expected values.
inline alien::Json worldSnapshot(const alien::World& world) {
    alien::Json actors = alien::Json::array();
    for (const auto& actor : world.enemies) {
        if (!actor.alive) { actors.push_back(nullptr); continue; }
        actors.push_back({actor.type, actor.x, actor.y, actor.health, actor.direction, actor.state, actor.frame});
    }
    while (actors.size() < 29) { actors.push_back(nullptr); }
    alien::Json objects = alien::Json::array(), breakables = alien::Json::array();
    for (auto item = world.objects.rbegin(); item != world.objects.rend(); ++item) {
        if (!item->active || item->type >= 100) { continue; }
        int state = item->state;
        if (item->type == 41) { state = item->terminalGroup; }
        alien::Json record = {item->type, item->x, item->y, item->health, item->direction, state};
        if (item->type >= 26 && item->type <= 32) { breakables.push_back(record); }
        else { objects.push_back(record); }
    }
    return {{"actors", actors}, {"objects", objects}, {"breakables", breakables}};
}

inline alien::Json fullSnapshot(const alien::World& world) {
    auto result = worldSnapshot(world);
    const auto& player = world.player;
    result.update({{"x", player.x}, {"y", player.y}, {"health", player.health}, {"vy", player.velocityY},
                   {"ground", player.supportY}, {"state", player.state}, {"frame", player.frame},
                   {"camera", {world.cameraX, world.cameraBottom}}, {"air", world.airState},
                   {"complete", world.complete}, {"projectiles", projectileSnapshot(world)}});
    return result;
}

// Share equipment and final-state fields across full routes and focused input regressions.
inline alien::Json playthroughSnapshot(const alien::World& world) {
    auto result = fullSnapshot(world);
    result.update({{"crouch", world.crouch}, {"direction", world.player.direction},
        {"dead", world.dead}, {"cleared", world.cleared}, {"weapon", world.inventory.weapon}, {"ammo", world.inventory.ammo},
        {"explosives", world.inventory.explosives}, {"score", world.inventory.score},
        {"enemies", world.activeEnemies()}, {"sources", world.activeSources()}});
    return result;
}
