/** @file world_reference_tests.cpp
 * @brief Verify world reference tests against runtime and original-game contracts.
 */
#include "reference_snapshot.h"
#include "reference_scenarios.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 3) { throw std::runtime_error("Expected assets and original world fixture."); }
        alien::Assets assets(argv[1]); assets.load();
        std::ifstream stream(argv[2]);
        const auto fixture = alien::Json::parse(stream);
        alien::World world(assets);
        for (const auto& level : fixture["levels"]) {
            const int id = level["id"];
            world.load(id, 1);
            const std::string scenario = level.value("scenario", "ambient");
            MechanismScenario mechanism(world, scenario);
            int tick = 0;
            for (const auto& expected : level["trace"]) {
                world.player.health = 100000;
                mechanism.beforeTick(tick);
                world.dismissMessage(); world.tick(mechanism.input(tick));
                const auto actual = fullSnapshot(world);
                for (auto entry = expected.begin(); entry != expected.end(); ++entry) {
                    if (actual[entry.key()] != entry.value()) {
                        throw std::runtime_error("Original world mismatch: level=" + std::to_string(id)
                            + " scenario=" + scenario + " tick=" + std::to_string(tick) + " category=" + entry.key());
                    }
                }
                ++tick;
            }
            if (alien::Json(world.level.tiles) != level["terrain"]["tiles"]
                || alien::Json(world.level.collision) != level["terrain"]["collision"]) {
                throw std::runtime_error("Original final terrain mismatch: level=" + std::to_string(id));
            }
            std::cout << "Original world matched: level=" << id << ", scenario=" << scenario << ", ticks=" << tick << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "World reference failure: " << error.what() << '\n';
        return 1;
    }
}
