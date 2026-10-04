/** @file world_trace.cpp
 * @brief Verify world trace against runtime and original-game contracts.
 */
#include "core/assets.h"
#include "world/world.h"
#include "reference_snapshot.h"
#include "reference_scenarios.h"
#include <fstream>
#include <iostream>

// Use the matching tools/reference scenario inputs for step-by-step state comparison.
int main(int argc, char** argv) {
    if (argc != 5) { std::cerr << "Usage: alien_trace assets level scenario output\n"; return 1; }
    alien::Assets assets(argv[1]); assets.load();
    alien::World world(assets); world.load(std::stoi(argv[2]), 1);
    alien::Json trace = alien::Json::array();
    const std::string scenario = argv[3];
    if (scenario.rfind("enemy", 0) == 0 || scenario.rfind("weapon", 0) == 0) {
        int type = 5;
        if (scenario.rfind("enemy", 0) == 0) { type = std::stoi(scenario.substr(5)); }
        alien::Actor enemy;
        enemy.type = type; enemy.x = world.player.x + 120; enemy.y = world.player.y;
        enemy.supportY = enemy.y; enemy.direction = -1; enemy.frame = 26 + type * 10;
        enemy.health = assets.entities[type].health; enemy.shotType = assets.entities[type].damage;
        world.enemies.assign(1, enemy); world.objects.clear();
        if (scenario.rfind("weapon", 0) == 0) {
            const int weapon = std::stoi(scenario.substr(6));
            world.inventory.owned[weapon] = true;
            world.inventory.ammo[weapon] = assets.projectiles[weapon].capacity;
            world.selectWeapon(weapon);
        }
    }
    if (scenario == "initial") {
        alien::Json objects = alien::Json::array();
        for (const auto& object : world.objects) {
            objects.push_back({{"id", object.id}, {"type", object.type}, {"x", object.x}, {"y", object.y},
                               {"active", object.active}, {"group", object.terminalGroup}, {"variant", object.variant}});
        }
        alien::Json state{{"tiles", world.level.tiles}, {"collision", world.level.collision}, {"objects", objects},
                          {"player", {world.player.x, world.player.y}}, {"camera", {world.cameraX, world.cameraBottom}}};
        std::ofstream stream(argv[4]); stream << state.dump(2);
        return 0;
    }
    MechanismScenario mechanism(world, scenario);
    for (int tick = 0; tick < 160; ++tick) {
        if (scenario == "ambient") { world.player.health = 100000; }
        mechanism.beforeTick(tick);
        alien::Input input = mechanism.input(tick);
        if (scenario == "walk") { input.right = true; }
        if (scenario == "jump" && tick == 0) { input.jump = true; }
        if (scenario == "crouch") { input.down = true; }
        if (scenario == "fire") { input.fire = true; }
        if (scenario.rfind("weapon", 0) == 0) { input.fire = true; }
        if (scenario == "turn") { input.right = tick < 5; input.left = tick >= 5; }
        world.dismissMessage();
        if (world.dead || world.complete) { break; }
        world.tick(input);
        const auto& player = world.player;
        trace.push_back({{"tick", tick}, {"x", player.x}, {"y", player.y}, {"vy", player.velocityY},
                         {"ground", player.supportY}, {"frame", player.frame}, {"state", player.state},
                         {"air", world.airState}, {"camera", {world.cameraX, world.cameraBottom}},
                         {"weapon", world.inventory.weapon}, {"ammo", world.inventory.ammo}, {"crouch", world.crouch}});
        trace.back()["health"] = player.health;
        trace.back()["complete"] = world.complete;
        if (scenario == "ambient" || mechanism.active()) {
            trace.back().update(worldSnapshot(world));
            trace.back()["projectiles"] = projectileSnapshot(world);
        }
        if (scenario.rfind("enemy", 0) == 0 || scenario.rfind("weapon", 0) == 0) {
            const auto& enemy = world.enemies[0];
            trace.back()["enemy"] = {{"x", enemy.x}, {"y", enemy.y}, {"frame", enemy.frame}, {"state", enemy.state}, {"direction", enemy.direction}, {"health", enemy.health}, {"alive", enemy.alive}};
        }
    }
    std::ofstream stream(argv[4]); stream << trace.dump(2);
    auto terrainPath = std::filesystem::path(argv[4]);
    terrainPath.replace_filename(terrainPath.stem().string() + "-terrain.json");
    std::ofstream terrainStream(terrainPath);
    terrainStream << alien::Json{{"tiles", world.level.tiles}, {"collision", world.level.collision}}.dump();
    return 0;
}
