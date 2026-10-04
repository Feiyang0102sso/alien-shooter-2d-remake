/** @file playthrough.cpp
 * @brief Verify playthrough against runtime and original-game contracts.
 */
#include "reference_snapshot.h"
#include <fstream>
#include <iostream>

// Replay ordinary inputs without cheats; omit message reading waits from logical ticks.
int main(int argc, char** argv) {
    try {
        if (argc != 4 && argc != 5) { throw std::runtime_error("Expected assets, route JSON, report JSON and optional original trace."); }
        alien::Assets assets(argv[1]); assets.load();
        std::ifstream routeStream(argv[2]); const auto route = alien::Json::parse(routeStream);
        alien::World world(assets); world.load(route.at("level"), route.value("difficulty", 1));
        alien::Json reference;
        if (argc == 5) {
            std::ifstream referenceStream(argv[4]);
            reference = alien::Json::parse(referenceStream);
        }
        alien::Json report{{"actions", alien::Json::array()}};
        int actionIndex = 0;
        for (const auto& action : route.at("actions")) {
            alien::Input input;
            input.left = action.value("left", false); input.right = action.value("right", false);
            input.jump = action.value("jump", false); input.down = action.value("down", false); input.fire = action.value("fire", false);
            input.weapon = action.value("weapon", -1); input.cycleWeapon = action.value("cycle", 0);
            for (int tick = 0; tick < action.at("ticks").get<int>() && !world.dead && !world.complete; ++tick) {
                if (!world.message.empty()) { world.prepareMessage(); world.dismissMessage(); }
                world.tick(input);
                if (!reference.is_null()) {
                    const auto actual = playthroughSnapshot(world);
                    const auto& expected = reference.at("trace").at(world.ticks - 1);
                    for (auto entry = expected.begin(); entry != expected.end(); ++entry) {
                        if (actual.at(entry.key()) != entry.value()) {
                            throw std::runtime_error("Original playthrough differs: tick=" + std::to_string(world.ticks)
                                + ", category=" + entry.key());
                        }
                    }
                }
            }
            alien::Json state{{"action", actionIndex++}, {"tick", world.ticks}, {"x", world.player.x}, {"y", world.player.y},
                {"health", world.player.health}, {"weapon", world.inventory.weapon}, {"ammo", world.inventory.ammo},
                {"air", world.airState}, {"state", world.player.state}, {"direction", world.player.direction}, {"crouch", world.crouch}, {"support", world.player.supportY},
                {"explosives", world.inventory.explosives}, {"enemies", world.activeEnemies()}, {"sources", world.activeSources()},
                {"score", world.inventory.score}, {"power", world.power}, {"complete", world.complete}};
            if (route.value("trace_world", false)) { state["world"] = fullSnapshot(world); }
            report["actions"].push_back(state); std::cout << state.dump() << '\n';
            if (world.dead || world.complete) { break; }
        }
        report["dead"] = world.dead; report["complete"] = world.complete;
        report["enemies"] = alien::Json::array(); report["objects"] = alien::Json::array();
        for (const auto& enemy : world.enemies) {
            if (enemy.alive) { report["enemies"].push_back({{"type", enemy.type}, {"x", enemy.x}, {"y", enemy.y}, {"health", enemy.health}}); }
        }
        for (const auto& object : world.objects) {
            if (object.active) { report["objects"].push_back({{"id", object.id}, {"type", object.type}, {"x", object.x}, {"y", object.y}, {"health", object.health}, {"variant", object.variant}, {"state", object.state}}); }
        }
        std::ofstream reportStream(argv[3]); reportStream << report.dump(2);
        if (!reportStream) { throw std::runtime_error("Cannot write route report."); }
        if (!reference.is_null() && world.ticks != reference.at("trace").size()) {
            throw std::runtime_error("Original playthrough trace length differs.");
        }
        if (world.dead || (route.value("require_complete", false) && !world.complete)) { return 2; }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Playthrough failed: " << error.what() << '\n';
        return 1;
    }
}
