/** @file render_snapshot.cpp
 * @brief Verify render snapshot against runtime and original-game contracts.
 */
#include "render/renderer.h"
#include "reference_scenarios.h"
#include <filesystem>
#include <iostream>

// Render reference scenes on the GPU and export the 240x320 canvas for pixel comparison.
int main(int argc, char** argv) {
    if (argc != 5) { std::cerr << "Usage: alien_render assets level scenario output.png\n"; return 1; }
    try {
        alien::Assets assets(argv[1]); assets.load();
        alien::World world(assets); world.load(std::stoi(argv[2]), 1);
        const std::string scenario = argv[3];
        MechanismScenario mechanism(world, scenario);
        const bool messageScenario = scenario.rfind("message", 0) == 0;
        const bool deathScenario = scenario.rfind("death", 0) == 0;
        if (deathScenario) {
            world.damagePlayer(100); world.beginDeathEffect();
            const int frames = std::stoi(scenario.substr(6));
            for (int frame = 0; frame < frames; ++frame) { world.advanceDeathEffect(); }
        }
        if (scenario != "initial" && !messageScenario && !deathScenario) {
            for (int tick = 0; tick < 160 && !world.dead && !world.complete; ++tick) {
                alien::Input input = mechanism.input(tick);
                if (scenario == "ambient") { world.player.health = 100000; }
                if (scenario == "walk") { input.right = true; }
                if (scenario == "jump" && tick == 0) { input.jump = true; }
                if (scenario == "crouch") { input.down = true; }
                if (scenario == "fire") { input.fire = true; }
                mechanism.beforeTick(tick); world.dismissMessage(); world.tick(input);
            }
        }
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(240, 320, "Alien Shooter render validation");
        auto target = LoadRenderTexture(240, 320);
        {
            alien::Renderer renderer(assets);
            BeginTextureMode(target); renderer.world(world);
            if (deathScenario) { renderer.deathWipe(world); }
            if (messageScenario) {
                // Scenario format: message:<key>[@age], keys from localization interface messages.
                float age = 1000.0f;
                const auto separator = scenario.find('@');
                const std::string key = scenario.substr(8, separator - 8);
                if (separator != std::string::npos) { age = std::stof(scenario.substr(separator + 1)); }
                renderer.message(assets.text.message(key), key == alien::message_key::LevelFailed, age);
            }
            EndTextureMode();
            auto screenshot = LoadImageFromTexture(target.texture); ImageFlipVertical(&screenshot);
            const auto path = std::filesystem::path(argv[4]);
            std::filesystem::create_directories(path.parent_path());
            const bool saved = ExportImage(screenshot, path.string().c_str());
            UnloadImage(screenshot);
            if (!saved) { throw std::runtime_error("Cannot export render snapshot."); }
        }
        UnloadRenderTexture(target); CloseWindow();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Render failure: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        return 1;
    }
}
