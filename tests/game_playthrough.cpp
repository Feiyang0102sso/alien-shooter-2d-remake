/** @file game_playthrough.cpp
 * @brief Verify game playthrough against runtime and original-game contracts.
 */
#include "game/game.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

// Replay normal desktop input through messages, teleports, combat, exits, and checkpoint saves.
int main(int argc, char** argv) {
    try {
        if (argc != 4) { throw std::runtime_error("Expected assets, route and save directory."); }
        alien::Assets assets(argv[1]); assets.load();
        std::ifstream stream(argv[2]);
        const auto route = alien::Json::parse(stream);
        SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(240, 320, "Campaign playthrough verification");
        if (!IsWindowReady()) { throw std::runtime_error("Hidden graphics context unavailable."); }
        {
            alien::Game game(assets, argv[3], true, 12345);
            game.startLevel(route.at("level"));
            for (const auto& action : route.at("actions")) {
                alien::Controls controls;
                controls.player.left = action.value("left", false);
                controls.player.right = action.value("right", false);
                controls.player.jump = action.value("jump", false);
                controls.player.down = action.value("down", false);
                controls.player.fire = action.value("fire", false);
                controls.player.weapon = action.value("weapon", -1);
                controls.player.cycleWeapon = action.value("cycle", 0);
                for (int tick = 0; tick < action.at("ticks").get<int>(); ++tick) {
                    if (game.screen() != alien::Screen::Playing) { break; }
                    if (!game.world().message.empty()) {
                        game.update(30, alien::Controls{}, true);
                        alien::Controls confirm; confirm.confirm = true;
                        game.update(0, confirm, true);
                    }
                    const int previousTicks = game.world().ticks;
                    // The camera transition occupies one original phase; Game consumes queued input at its end.
                    for (int frame = 0; frame < 100 && game.world().ticks == previousTicks; ++frame) {
                        game.update(0.08f, controls, true);
                    }
                    if (game.world().ticks != previousTicks + 1) { throw std::runtime_error("Playthrough stalled or skipped a logical step."); }
                }
                if (game.screen() != alien::Screen::Playing) { break; }
            }
            const auto& world = game.world();
            const auto& expected = route.at("expected");
            if (game.screen() != alien::Screen::Completed || !world.complete || world.dead
                || world.ticks != expected.at("ticks") || world.player.health != expected.at("health")
                || world.inventory.score != expected.at("score") || world.activeEnemies() != 0 || world.activeSources() != 0) {
                std::cerr << "Final state: ticks=" << world.ticks << ", health=" << world.player.health
                    << ", score=" << world.inventory.score << ", enemies=" << world.activeEnemies() << '\n';
                throw std::runtime_error("Normal-input campaign route did not reach the expected exit.");
            }
            game.update(30, alien::Controls{}, true);
            alien::Controls confirm; confirm.confirm = true;
            game.update(0, confirm, true);
            const int nextLevel = route.at("level").get<int>() + 1;
            if (game.world().level.id != nextLevel || game.progress().unlocked < nextLevel) { throw std::runtime_error("Real completion did not advance the campaign."); }
            const auto saved = alien::SaveStore(assets, argv[3]).load();
            if (saved.unlocked < nextLevel || saved.checkpoint.score != expected.at("score")) {
                throw std::runtime_error("Real completion was not saved.");
            }
        }
        CloseWindow();
        std::cout << "Normal-input playthrough and progression passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Game playthrough failed: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        return 1;
    }
}
