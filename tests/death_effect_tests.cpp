/** @file death_effect_tests.cpp
 * @brief Verify death effect tests against runtime and original-game contracts.
 */
#include "reference_snapshot.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 3) { throw std::runtime_error("Expected assets and death fixture."); }
        alien::Assets assets(argv[1]); assets.load();
        alien::World world(assets); world.load(1, 1);
        world.damagePlayer(100); world.beginDeathEffect();
        std::ifstream stream(argv[2]);
        const auto reference = alien::Json::parse(stream);
        for (const auto& sample : reference.at("samples")) {
            if (world.deathWipeActive != sample.at("wipe") || alien::Json(world.deathColumns) != sample.at("columns")
                || projectileSnapshot(world) != sample.at("projectiles")) {
                throw std::runtime_error("Death effect differs at frame " + std::to_string(sample.at("frame").get<int>()));
            }
            const bool finished = world.advanceDeathEffect();
            if (finished != (sample.at("frame") == reference.at("samples").size() - 1)) {
                throw std::runtime_error("Death effect completion frame differs.");
            }
        }
        std::cout << "Original death particle and wipe sequence passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Death effect test failed: " << error.what() << '\n';
        return 1;
    }
}
