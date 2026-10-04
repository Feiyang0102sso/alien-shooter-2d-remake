/** @file game_flow_tests.cpp
 * @brief Verify game flow tests against runtime and original-game contracts.
 */
#include "game/game.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}
void clearSaveFiles(const std::filesystem::path& directory) {
    // Remove only this test's fixed files; never recursively modify directories.
    for (const std::string name : {"progress.json", "progress.bak", "progress.tmp", "progress.corrupt.json"}) {
        std::filesystem::remove(directory / name);
    }
}
void writeText(const std::filesystem::path& path, const std::string& value) {
    std::ofstream stream(path); stream << value;
    require(static_cast<bool>(stream), "Cannot create test input.");
}
void confirm(alien::Game& game) {
    alien::Controls controls; controls.confirm = true;
    game.update(0, controls, true);
}
void cancel(alien::Game& game) {
    alien::Controls controls; controls.cancel = true;
    game.update(0, controls, true);
}
void selectItem(alien::Game& game, int count, int item, bool title, bool overlay = false) {
    const auto layout = alien::menuLayout(count, title, overlay);
    alien::Controls controls; controls.leftClick = true;
    controls.mouse = {static_cast<float>(layout.left + 8), static_cast<float>(layout.firstItem + item * 12 + 5)};
    game.update(0, controls, true);
}
void testSaveRecovery(const alien::Assets& assets, const std::filesystem::path& root) {
    clearSaveFiles(root);
    alien::SaveStore store(assets, root);
    auto progress = store.load();
    require(progress.unlocked == 0 && progress.sound, "Fresh save defaults differ.");
    progress.unlocked = 3; progress.difficulty = 2; progress.sound = false;
    progress.checkpoint.owned[2] = true; progress.checkpoint.ammo[2] = 87;
    progress.checkpoint.score = 170; progress.checkpoint.tutorialFlags = 1023;
    store.write(progress);
    progress.unlocked = 4; progress.checkpoint.score = 280; store.write(progress);
    alien::SaveStore reloaded(assets, root);
    auto found = reloaded.load();
    require(found.unlocked == 4 && found.checkpoint.score == 280, "Committed save did not round-trip.");
    require(found.difficulty == 2 && !found.sound && found.checkpoint.ammo[2] == 87
        && found.checkpoint.tutorialFlags == 1023, "Inventory or preferences lost during save.");
    writeText(root / "progress.json", "{truncated");
    alien::SaveStore recovered(assets, root);
    found = recovered.load();
    require(found.unlocked == 3 && found.checkpoint.score == 170, "Backup recovery failed.");
    found.checkpoint.score = 180; recovered.write(found);
    require(std::filesystem::exists(root / "progress.corrupt.json"), "Corrupt original was not retained.");
    writeText(root / "progress.json", "{truncated again");
    alien::SaveStore secondRecovery(assets, root);
    require(secondRecovery.load().checkpoint.score == 170, "Recovery overwrote the valid backup with corrupt data.");
    writeText(root / "progress.bak", "{\"version\":1,\"unlocked\":8,\"difficulty\":2,\"sound\":false,\"checkpoint\":null}");
    writeText(root / "progress.tmp", "incomplete pending write");
    found = secondRecovery.load();
    require(found.unlocked == 0 && found.sound && found.difficulty == 1, "Partial parse polluted default progress.");
    const auto blocked = root / "not-a-directory";
    writeText(blocked, "file");
    bool failed = false;
    try { alien::SaveStore(assets, blocked).write(found); }
    catch (const std::filesystem::filesystem_error&) { failed = true; }
    require(failed, "Save write failure was silently accepted.");
    std::cout << "Save recovery contracts passed.\n";
}
void testGameFlow(const alien::Assets& assets, const std::filesystem::path& root) {
    clearSaveFiles(root);
    alien::Game game(assets, root, true);
    confirm(game);
    require(game.screen() == alien::Screen::SoundPrompt, "Intro did not enter sound prompt.");
    alien::Controls decline; decline.leftClick = true; decline.mouse = {220, 313};
    game.update(0, decline, true);
    require(game.screen() == alien::Screen::Main && !game.progress().sound, "Mouse NO incorrectly enabled audio.");
    selectItem(game, 7, 1, false);
    require(game.screen() == alien::Screen::Main, "Unavailable Continue started a level.");
    selectItem(game, 7, 0, false); confirm(game);
    require(game.screen() == alien::Screen::Playing && game.world().level.id == 1, "New game did not start level one.");
    require(game.progress().unlocked == 0, "Starting a new game prematurely unlocked Continue.");

    const int originalTicks = game.world().ticks;
    alien::Controls jump; jump.player.jump = true;
    game.update(0.01f, jump, true);
    game.update(0.071f, alien::Controls{}, true);
    require(game.world().ticks == originalTicks + 1 && game.world().player.velocityY > 0, "Short W press was lost between fixed ticks.");
    game.update(0, alien::Controls{}, false);
    require(game.screen() == alien::Screen::Pause, "Focus loss did not pause.");
    const int pausedTicks = game.world().ticks;
    game.update(1, alien::Controls{}, false);
    require(game.world().ticks == pausedTicks, "World advanced while paused.");
    selectItem(game, 6, 3, false, true);
    require(game.screen() == alien::Screen::Playing && game.world().ticks == 0, "Pause restart did not reset the level.");

    // Injected completion checks progression only; bytecode tests separately cover combat and real exits.
    for (int level = 1; level <= 8; ++level) {
        auto& world = game.world();
        require(world.level.id == level, "Campaign level order changed.");
        world.inventory.owned[1] = true; world.inventory.ammo[1] = 79;
        world.inventory.score = level * 10; world.inventory.armor = 2; world.inventory.explosives = 4;
        world.inventory.tutorialFlags = 1023;
        world.dismissMessage(); world.complete = true;
        game.update(0.081f, alien::Controls{}, true);
        require(game.screen() == alien::Screen::Completed, "Completion did not open the original message flow.");
        confirm(game);
        require(game.screen() == alien::Screen::Completed, "Completion typing was skipped by an early input.");
        game.update(10, alien::Controls{}, true);
        confirm(game);
        if (level < 8) {
            require(game.world().inventory.owned[1] && game.world().inventory.ammo[1] == 79
                && game.world().inventory.score == level * 10, "Cross-level inventory was not carried.");
            require(game.world().inventory.armor == 0 && game.world().inventory.explosives == 0
                && game.world().inventory.weapon == 0 && game.world().player.health == 100, "Original level resets differ.");
            require(game.world().inventory.tutorialFlags == 1023, "Tutorials were not remembered across levels.");
        }
    }
    require(game.screen() == alien::Screen::About && game.progress().unlocked == 8, "Final level did not reach credits.");
    alien::SaveStore persisted(assets, root);
    require(persisted.load().unlocked == 8, "Campaign unlocks were not persisted.");
    game.showScreen(alien::Screen::Main);
    selectItem(game, 7, 2, false);
    selectItem(game, 8, 0, true);
    require(game.world().level.id == 1 && game.world().inventory.score == 0, "Old level replay should start with fresh inventory.");
    cancel(game); selectItem(game, 6, 4, false, true);
    selectItem(game, 7, 1, false);
    require(game.world().level.id == 8 && game.world().inventory.score == 70, "Replay overwrote the highest Continue checkpoint.");
    game.world().inventory.ammo[1] = 2;
    game.world().damagePlayer(10000);
    game.update(0.081f, alien::Controls{}, true);
    require(game.screen() == alien::Screen::Dying, "Death skipped the original transition.");
    for (int step = 0; step < 200 && game.screen() == alien::Screen::Dying; ++step) { game.update(0.1f, alien::Controls{}, true); }
    require(game.screen() == alien::Screen::Failed, "Death did not enter failure flow.");
    game.update(10, alien::Controls{}, true);
    confirm(game);
    require(game.world().level.id == 8 && game.world().inventory.ammo[1] == 79
        && game.world().player.health == 100, "Retry did not restore the level checkpoint.");
    std::cout << "Desktop input and campaign flow contracts passed.\n";
}
void testFireInput(const alien::Assets& assets, const std::filesystem::path& root) {
    alien::Game game(assets, root, true);
    game.startLevel(1);

    // A click released between logical ticks must still be consumed by the next tick.
    alien::Controls held;
    held.player.fire = true;
    alien::Controls pressed = held;
    pressed.leftClick = true;
    game.update(0.01f, pressed, true);
    game.update(0.071f, alien::Controls{}, true);
    require(game.world().player.state == 2, "Short mouse press was lost between fixed ticks.");
    const int firstAttack = game.world().player.attackStarted;
    for (int frame = 0; frame < 20; ++frame) {
        game.update(0.08f, alien::Controls{}, true);
    }
    require(game.world().player.attackStarted == firstAttack, "Released mouse press kept firing.");

    // A synthetic click-only snapshot fires once, without repeating during catch-up ticks.
    game.startLevel(1);
    alien::Controls click;
    click.leftClick = true;
    game.update(0.241f, click, true);
    require(game.world().player.attackStarted == 80, "Mouse click pulse did not start an attack.");
    for (int frame = 0; frame < 20; ++frame) {
        game.update(0.08f, alien::Controls{}, true);
    }
    require(game.world().player.attackStarted == 80, "Mouse click pulse was replayed after consumption.");

    // Held fire continues at the original weapon cooldown instead of acting as a single click.
    for (int frame = 0; frame < 20; ++frame) {
        game.update(0.08f, held, true);
    }
    require(game.world().player.attackStarted > 80, "Held mouse stopped repeating fire.");
    const int lastHeldAttack = game.world().player.attackStarted;
    for (int frame = 0; frame < 20; ++frame) {
        game.update(0.08f, alien::Controls{}, true);
    }
    require(game.world().player.attackStarted == lastHeldAttack, "Held fire continued after release.");
    std::cout << "Mouse pulse and held-fire contracts passed.\n";
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "Expected assets and test save directory.");
        alien::Assets assets(argv[1]); assets.load();
        const std::filesystem::path root(argv[2]);
        std::filesystem::create_directories(root / "recovery");
        std::filesystem::create_directories(root / "flow");
        testSaveRecovery(assets, root / "recovery");
        SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(240, 320, "Game flow verification");
        require(IsWindowReady(), "Hidden graphics context unavailable.");
        testGameFlow(assets, root / "flow");
        testFireInput(assets, root / "flow");
        CloseWindow();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Game flow test failed: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        return 1;
    }
}
