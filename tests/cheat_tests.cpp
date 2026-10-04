/** @file cheat_tests.cpp
 * @brief Verify CHGOD input, damage immunity, revocation, and save isolation.
 */
#include "game/game.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

void type(alien::Game& game, const std::string& text) {
    // Feed one desktop event at a time, including events between physics ticks.
    for (char character : text) {
        alien::Controls controls;
        controls.textInput = character;
        game.update(0, controls, true);
    }
}

void checkCheat(alien::Game& game, const alien::Assets& assets, const std::filesystem::path& saveDirectory) {
    type(game, "chgod");
    require(!game.world().cheat.enabled(), "Cheat activated outside gameplay.");
    game.startLevel(1);
    auto& world = game.world();
    world.inventory.owned[1] = true;
    world.inventory.ammo[1] = 17;
    world.selectWeapon(1);
    const auto previous = world.inventory;
    type(game, "chgoXgod");
    require(!world.cheat.enabled(), "Wrong code was accepted.");
    type(game, "cCHGoD");
    require(world.cheat.enabled(), "Mixed-case overlapping code was not recognized.");
    for (int weapon = 0; weapon < 5; ++weapon) {
        require(world.inventory.owned[weapon], "A weapon was not granted.");
        if (weapon > 0) {
            require(world.inventory.ammo[weapon] == assets.projectiles[weapon].capacity, "Ammunition was not filled.");
        }
    }
    const int health = world.player.health;
    const auto soundCount = world.sounds.size();
    world.damagePlayer(1000);
    require(world.player.health == health && world.player.alive && !world.dead, "Cheat did not prevent lethal damage.");
    require(world.sounds.size() == soundCount, "Ignored damage still produced a hit sound.");

    world.selectWeapon(4);
    world.player.state = 2;
    type(game, "chgod");
    require(!world.cheat.enabled(), "Second code did not disable the cheat.");
    require(world.inventory.owned == previous.owned && world.inventory.ammo == previous.ammo
        && world.inventory.weapon == previous.weapon && world.player.state != 2,
        "Revocation during an attack did not restore normal equipment.");
    world.damagePlayer(1);
    require(world.player.health == health - 1, "Damage immunity survived revocation.");

    type(game, "chg");
    game.update(0, alien::Controls{}, false);
    require(game.screen() == alien::Screen::Pause, "Focus loss did not pause.");
    game.showScreen(alien::Screen::Playing);
    type(game, "od");
    require(!world.cheat.enabled(), "Partial code survived focus loss.");
    type(game, "chgod");
    game.showScreen(alien::Screen::Pause);
    type(game, "chgod");
    require(world.cheat.enabled(), "Typing in a paused menu changed the cheat.");
    game.showScreen(alien::Screen::Playing);
    game.startLevel(1, true);
    require(!world.cheat.enabled() && !world.inventory.owned[4], "Restart retained temporary equipment.");

    world.inventory.owned[1] = true;
    world.inventory.ammo[1] = 23;
    type(game, "chgod");
    world.selectWeapon(4);
    // Completion is injected only to check the real checkpoint and save path.
    world.complete = true;
    game.update(0.08f, alien::Controls{}, true);
    require(game.screen() == alien::Screen::Completed, "Completion screen was not entered.");
    game.update(30, alien::Controls{}, true);
    alien::Controls confirm;
    confirm.confirm = true;
    game.update(0, confirm, true);
    const auto saved = alien::SaveStore(assets, saveDirectory).load();
    require(world.level.id == 2 && !world.cheat.enabled(), "Cheat survived level completion.");
    require(saved.checkpoint.owned[1] && saved.checkpoint.ammo[1] == 23
        && !saved.checkpoint.owned[4] && saved.checkpoint.ammo[4] == 0,
        "Temporary equipment leaked into the checkpoint.");
    type(game, "chgod");
    game.showScreen(alien::Screen::Main);
    require(!world.cheat.enabled(), "Leaving gameplay did not disable the cheat.");
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) { throw std::runtime_error("Expected assets and test save directory."); }
        alien::Assets assets(argv[1]);
        assets.load();
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(240, 320, "Cheat verification");
        if (!IsWindowReady()) { throw std::runtime_error("Hidden graphics context unavailable."); }
        {
            alien::Game game(assets, argv[2], true);
            checkCheat(game, assets, argv[2]);
        }
        CloseWindow();
        std::cout << "CHGOD contracts passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "CHGOD verification failed: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        return 1;
    }
}
