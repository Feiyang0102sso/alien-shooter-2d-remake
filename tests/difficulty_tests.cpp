/** @file difficulty_tests.cpp
 * @brief Verify difficulty tests against runtime and original-game contracts.
 */
#include "world/world.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        if (argc != 3) { throw std::runtime_error("Expected asset root and difficulty fixture."); }
        alien::Assets assets(argv[1]); assets.load();
        alien::World world(assets);
        std::ifstream stream(argv[2]);
        const auto fixture = alien::Json::parse(stream);
        for (const auto& scenario : fixture.at("cases")) {
            const int difficulty = scenario.at("difficulty");
            world.load(6, difficulty);
            alien::Json initial = alien::Json::array();
            for (const auto& enemy : world.enemies) { initial.push_back({enemy.type, enemy.health}); }
            if (initial != scenario.at("initial")) { throw std::runtime_error("Initial difficulty health differs from original."); }
            world.tick({});
            for (const auto& enemy : world.enemies) {
                if (enemy.source >= 0 && enemy.health != scenario.at("generated").at(enemy.type - 1)) {
                    throw std::runtime_error("Generator difficulty health differs from original.");
                }
            }
        }
        std::cout << "Original difficulty load-order contracts passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Difficulty test failed: " << error.what() << '\n';
        return 1;
    }
}
