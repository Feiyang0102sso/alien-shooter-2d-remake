/** @file input_reference_tests.cpp
 * @brief Verify input reference tests against runtime and original-game contracts.
 */
#include "reference_snapshot.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

// Replay original input boundaries using keys only; some scenarios intentionally end in death.
int main(int argc, char** argv) {
    try {
        if (argc != 4) { throw std::runtime_error("Expected assets, input routes and original traces."); }
        alien::Assets assets(argv[1]);
        assets.load();
        std::ifstream routeStream(argv[2]), referenceStream(argv[3]);
        const auto routes = alien::Json::parse(routeStream);
        const auto references = alien::Json::parse(referenceStream);
        if (routes.at("cases").size() != references.at("cases").size()) {
            throw std::runtime_error("Input reference case count differs.");
        }
        for (std::size_t index = 0; index < routes.at("cases").size(); ++index) {
            const auto& route = routes.at("cases").at(index);
            const auto& reference = references.at("cases").at(index);
            const std::string name = route.at("name");
            if (name != reference.at("name")) { throw std::runtime_error("Input reference case order differs."); }
            alien::World world(assets);
            world.load(route.at("level"), 1);
            std::size_t tick = 0;
            for (const auto& action : route.at("actions")) {
                alien::Input input;
                input.left = action.value("left", false);
                input.right = action.value("right", false);
                input.jump = action.value("jump", false);
                input.down = action.value("down", false);
                input.fire = action.value("fire", false);
                for (int step = 0; step < action.at("ticks").get<int>(); ++step) {
                    if (world.dead || world.complete) { throw std::runtime_error("Input case ended early: " + name); }
                    if (!world.message.empty()) { world.prepareMessage(); world.dismissMessage(); }
                    world.tick(input);
                    const auto actual = playthroughSnapshot(world);
                    const auto& expected = reference.at("trace").at(tick++);
                    for (auto field = expected.begin(); field != expected.end(); ++field) {
                        if (actual.at(field.key()) != field.value()) {
                            throw std::runtime_error("Original input differs: " + name + ", tick="
                                + std::to_string(tick) + ", field=" + field.key());
                        }
                    }
                }
            }
            if (tick != reference.at("trace").size()) { throw std::runtime_error("Input reference length differs: " + name); }
            std::cout << "Original input matched: " << name << " (" << tick << " ticks)\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Input reference failed: " << error.what() << '\n';
        return 1;
    }
}
