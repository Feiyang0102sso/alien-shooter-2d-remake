/** @file playthrough_render.cpp
 * @brief Verify playthrough render against runtime and original-game contracts.
 */
#include "renderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Capture normal-input routes on the GPU without advancing visual state during drawing.
int main(int argc, char** argv) {
    try {
        if (argc != 5) { throw std::runtime_error("Expected assets, route, output directory and snapshot interval."); }
        alien::Assets assets(argv[1]);
        assets.load();
        std::ifstream routeStream(argv[2]);
        const auto route = alien::Json::parse(routeStream);
        const std::filesystem::path output(argv[3]);
        const int interval = std::stoi(argv[4]);
        if (interval <= 0) { throw std::runtime_error("Snapshot interval must be positive."); }
        std::filesystem::create_directories(output);
        alien::World world(assets);
        world.load(route.at("level"), 1);
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(240, 320, "Playthrough render verification");
        if (!IsWindowReady()) { throw std::runtime_error("Hidden graphics context unavailable."); }
        const auto target = LoadRenderTexture(240, 320);
        {
            alien::Renderer renderer(assets);
            for (const auto& action : route.at("actions")) {
                alien::Input input;
                input.left = action.value("left", false);
                input.right = action.value("right", false);
                input.jump = action.value("jump", false);
                input.down = action.value("down", false);
                input.fire = action.value("fire", false);
                input.weapon = action.value("weapon", -1);
                for (int step = 0; step < action.at("ticks").get<int>() && !world.dead && !world.complete; ++step) {
                    if (!world.message.empty()) {
                        world.prepareMessage();
                        // Draw the message background first; shaken canvas edges contribute to the following frame.
                        BeginTextureMode(target);
                        renderer.world(world);
                        EndTextureMode();
                        world.dismissMessage();
                    }
                    world.tick(input);
                    BeginTextureMode(target);
                    renderer.world(world);
                    EndTextureMode();
                    if (world.ticks % interval != 0) { continue; }
                    auto screenshot = LoadImageFromTexture(target.texture);
                    ImageFlipVertical(&screenshot);
                    const auto path = output / ("tick-" + std::to_string(world.ticks) + ".png");
                    const bool saved = ExportImage(screenshot, path.u8string().c_str());
                    UnloadImage(screenshot);
                    if (!saved) { throw std::runtime_error("Cannot export playthrough screenshot."); }
                }
                if (world.dead || world.complete) { break; }
            }
        }
        UnloadRenderTexture(target);
        CloseWindow();
        std::cout << "Playthrough snapshots ready: " << world.ticks << " ticks.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Playthrough rendering failed: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        return 1;
    }
}
