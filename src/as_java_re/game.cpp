/** @file game.cpp
 * @brief Coordinate desktop input, original screens, level progression, and saves.
 */
#include "game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace alien {
namespace {
void replaceText(std::string& contents, const std::string& from, const std::string& to) {
    const auto position = contents.find(from);
    if (position != std::string::npos) { contents.replace(position, from.size(), to); }
}
}
Game::Game(const Assets& assets, std::filesystem::path saveDirectory, bool silent, std::uint64_t seed)
    : assets_(assets), renderer_(assets), world_(assets), saveStore_(assets, std::move(saveDirectory)) {
    world_.seedRandom(seed);
    help_ = assets_.readText("localization/en/help.txt");
    about_ = assets_.readText("localization/en/about.txt");
    // Replace loaded control labels while preserving the original body text and icons.
    replaceText(help_, "LEFT (4) - Move left", "A - Move left");
    replaceText(help_, "RIGHT (6) - Move right", "D - Move right");
    replaceText(help_, "UP (2) - Jump", "W - Jump / Stand up");
    replaceText(help_, "DOWN (8) - Look down", "S - Crouch / Look down / Terminal");
    replaceText(help_, "(0) - Switch weapon", "Q / E - Switch weapon\n1-4 - Select gun\n5 - Pistol");
    replaceText(help_, "SOFT1 (5) - Fire", "LEFT MOUSE - Fire");
    replaceText(help_, "SOFT2 (#) - Open menu", "ESC - Open menu");
    progress_ = saveStore_.load();
    difficulty_ = progress_.difficulty; sound_ = progress_.sound;
    if (progress_.unlocked > 0) { mainSelection_ = 1; }
    if (!silent) {
        try {
            audio_ = std::make_unique<Audio>(assets_);
            audio_->setEnabled(sound_);
        } catch (const std::exception& error) {
            // An unavailable audio device does not block gameplay; log the fallback reason.
            std::cerr << "Audio disabled: " << error.what() << '\n';
            sound_ = false;
        }
    }
}
void Game::setSound(bool enabled) {
    sound_ = enabled;
    if (audio_) { audio_->setEnabled(enabled); }
}
void Game::playWorldSounds() {
    if (audio_) {
        for (int sound : world_.sounds) { audio_->play(sound); }
    }
    // Clear consumed events so landing before a message can emit new sounds separately.
    world_.sounds.clear();
}
void Game::save() {
    progress_.sound = sound_;
    try {
        saveStore_.write(progress_);
    } catch (const std::exception& error) {
        // Report persistence errors at the game boundary and keep the current session playable.
        std::cerr << "Save failed: " << error.what() << '\n';
    }
}

void Game::showScreen(Screen screen) {
    world_.cheat.resetInput();
    if (screen == Screen::Main) { world_.cheat.disable(world_); }
    const bool keepMessage = (screen == Screen::Pause || screen == Screen::Playing)
        && (screen_ == Screen::Pause || screen_ == Screen::Playing);
    if (screen == Screen::Main && (screen_ == Screen::Pause || screen_ == Screen::Failed || screen_ == Screen::Completed)) {
        mainSelection_ = 0;
        if (progress_.unlocked > 0) { mainSelection_ = 1; }
    }
    screen_ = screen; selection_ = 0; scroll_ = 0; pending_ = Input{}; accumulator_ = 0;
    if (!keepMessage) { messageTime_ = 0; }
    scrollRemainder_ = 0;
    menuTime_ = 0; selectionTime_ = 0;
    soundPromptTime_ = 0;
    if (audio_) { audio_->pause(false); }
    if (screen == Screen::Main && audio_) { audio_->play(16); }
    if (screen == Screen::Pause && audio_) { audio_->play(11); }
    buildMenu();
}
void Game::buildMenu() {
    items_.clear(); enabled_.clear(); title_.clear();
    switch (screen_) {
    case Screen::Main:
        items_ = {"NEW GAME", "CONTINUE", "SELECT LEVEL", "SETTINGS", "HELP", "ABOUT", "EXIT"};
        enabled_ = {true, progress_.unlocked > 0, progress_.unlocked > 0, true, true, true, true}; selection_ = mainSelection_; break;
    case Screen::Difficulty: title_ = "NEW GAME"; items_ = {"EASY", "NORMAL", "HARD"}; selection_ = difficulty_; break;
    case Screen::Levels:
        title_ = "SELECT LEVEL";
        for (int level = 1; level <= progress_.unlocked; ++level) { items_.push_back("LEVEL " + std::to_string(level)); }
        break;
    case Screen::Settings:
        title_ = "SETTINGS";
        if (sound_) { items_.push_back("SOUND ON"); }
        else { items_.push_back("SOUND OFF"); }
        items_.push_back("VIBRATION OFF"); enabled_ = {true, false};
        break;
    case Screen::Pause:
        items_ = {"CONTINUE", "SOUND OFF", "VIBRATION OFF", "RESTART", "MENU", "EXIT"};
        if (sound_) { items_[1] = "SOUND ON"; }
        enabled_ = {true, true, false, true, true, true}; break;
    default: break;
    }
    if (enabled_.empty()) { enabled_.resize(items_.size(), true); }
}
void Game::startLevel(int level, bool carry) {
    if (!carry) { checkpoint_ = Inventory{}; }
    world_.load(level, difficulty_, &checkpoint_);
    showScreen(Screen::Playing);
    if (audio_) { audio_->play(11); }
}
void Game::choose() {
    if (items_.empty() || !enabled_[selection_]) { return; }
    switch (screen_) {
    case Screen::Main:
        mainSelection_ = selection_;
        switch (selection_) {
        case 0: showScreen(Screen::Difficulty); break;
        case 1:
            checkpoint_ = progress_.checkpoint; difficulty_ = progress_.difficulty;
            startLevel(progress_.unlocked, true); break;
        case 2: showScreen(Screen::Levels); break;
        case 3: showScreen(Screen::Settings); break;
        case 4: showScreen(Screen::Help); break;
        case 5: showScreen(Screen::About); break;
        case 6: exit_ = true; break;
        }
        break;
    case Screen::Difficulty:
        difficulty_ = selection_; progress_ = Progress{}; progress_.difficulty = difficulty_;
        startLevel(1); save(); break;
    case Screen::Levels:
        checkpoint_ = progress_.checkpoint; difficulty_ = progress_.difficulty;
        startLevel(selection_ + 1, selection_ + 1 == progress_.unlocked); break;
    case Screen::Settings: setSound(!sound_); save(); buildMenu(); break;
    case Screen::Pause:
        switch (selection_) {
        case 0: showScreen(Screen::Playing); break;
        case 1: setSound(!sound_); save(); buildMenu(); break;
        case 3: startLevel(world_.level.id, true); break;
        case 4: showScreen(Screen::Main); break;
        case 5: exit_ = true; break;
        }
        break;
    default: break;
    }
}
void Game::back() {
    if (screen_ == Screen::Main) { exit_ = true; }
    else if (screen_ == Screen::Pause) { showScreen(Screen::Playing); }
    else { showScreen(Screen::Main); }
}
void Game::menuInput(const Controls& controls) {
    if (controls.cancel || controls.rightClick) { back(); return; }
    if (items_.empty()) { return; }
    if (controls.up) { selection_ = (selection_ + static_cast<int>(items_.size()) - 1) % static_cast<int>(items_.size()); }
    if (controls.down) { selection_ = (selection_ + 1) % static_cast<int>(items_.size()); }
    if (controls.up || controls.down) { selectionTime_ = 0; }
    if (controls.confirm) { choose(); return; }
    if (controls.leftClick) {
        const auto mouse = controls.mouse;
        if (mouse.x < 0 || mouse.x >= ScreenWidth || mouse.y < 0 || mouse.y >= ScreenHeight) { return; }
        if (mouse.y >= 309) {
            if (mouse.x < 60) { choose(); }
            if (mouse.x > 185) { back(); }
            return;
        }
        const auto layout = menuLayout(static_cast<int>(items_.size()), !title_.empty(), screen_ == Screen::Pause);
        const int item = static_cast<int>(mouse.y - layout.firstItem) / 12;
        if (mouse.x >= layout.left && mouse.x < layout.left + layout.width && mouse.y >= layout.firstItem
            && item >= 0 && item < static_cast<int>(items_.size())) {
            selection_ = item; choose();
        }
    }
}

void Game::update(float elapsed, Vector2 mouse, bool focused) {
    Controls controls;
    for (int character = GetCharPressed(); character != 0; character = GetCharPressed()) {
        if (character < 128) { controls.textInput.push_back(static_cast<char>(character)); }
        else { controls.textInput.push_back('\0'); }
    }
    controls.mouse = mouse;
    controls.confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
    controls.cancel = IsKeyPressed(KEY_ESCAPE);
    controls.up = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    controls.down = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);
    controls.leftClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    controls.rightClick = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    controls.yes = IsKeyPressed(KEY_Y); controls.no = IsKeyPressed(KEY_N);
    controls.pageUp = IsKeyPressed(KEY_PAGE_UP); controls.pageDown = IsKeyPressed(KEY_PAGE_DOWN);
    controls.scrollUp = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
    controls.wheel = GetMouseWheelMove(); controls.debug = IsKeyPressed(KEY_F3);
    controls.player.left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
    controls.player.right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);
    controls.player.down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
    controls.player.fire = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    controls.player.jump = controls.up;
    if (IsKeyPressed(KEY_Q)) { controls.player.cycleWeapon = -1; }
    if (IsKeyPressed(KEY_E)) { controls.player.cycleWeapon = 1; }
    for (int key = KEY_ONE; key <= KEY_FOUR; ++key) {
        if (IsKeyPressed(key)) {
            controls.player.weapon = key - KEY_ONE + 1;
        }
    }
    if (IsKeyPressed(KEY_FIVE)) { controls.player.weapon = 0; }
    update(elapsed, controls, focused);
}

void Game::queuePlayerInput(const Controls& controls) {
    pending_.left = controls.player.left;
    pending_.right = controls.player.right;
    pending_.down = controls.player.down;
    // The original delays released keys; retain desktop clicks until the next logical tick.
    pending_.fire = pending_.fire || controls.leftClick;
    pending_.jump = pending_.jump || controls.player.jump;
    if (controls.player.cycleWeapon != 0) { pending_.cycleWeapon = controls.player.cycleWeapon; }
    if (controls.player.weapon >= 0) { pending_.weapon = controls.player.weapon; }
}
Input Game::takePlayerInput(bool heldFire) {
    Input input = pending_;
    input.fire = input.fire || heldFire;
    // Consume a click edge once; subsequent catch-up ticks use the held state.
    pending_.fire = false;
    pending_.jump = false;
    pending_.weapon = -1;
    pending_.cycleWeapon = 0;
    return input;
}
void Game::update(float elapsed, const Controls& controls, bool focused) {
    const bool confirm = controls.confirm || controls.leftClick;
    if (!items_.empty()) { menuTime_ += std::min(elapsed, 0.25f); selectionTime_ += elapsed; }
    if (screen_ == Screen::Intro) {
        introTime_ += elapsed;
        if (introTime_ >= 5.0f || confirm) { showScreen(Screen::SoundPrompt); }
        return;
    }
    if (screen_ == Screen::SoundPrompt) {
        soundPromptTime_ += elapsed;
        bool accept = controls.confirm || controls.yes;
        bool decline = controls.no || controls.cancel || controls.rightClick;
        if (controls.leftClick && controls.mouse.y >= 302 && controls.mouse.y < ScreenHeight) {
            accept = controls.mouse.x >= 0 && controls.mouse.x < 60;
            decline = controls.mouse.x >= 180 && controls.mouse.x < ScreenWidth;
        }
        if (accept) { setSound(true); save(); showScreen(Screen::Main); }
        else if (decline) { setSound(false); save(); showScreen(Screen::Main); }
        return;
    }
    if (screen_ == Screen::Help || screen_ == Screen::About) {
        if (controls.cancel || controls.rightClick || (controls.leftClick && controls.mouse.y >= 302)) { back(); return; }
        int movement = -static_cast<int>(controls.wheel * 28);
        if (controls.player.down) { scrollRemainder_ += elapsed * 100; }
        if (controls.scrollUp) { scrollRemainder_ -= elapsed * 100; }
        const int continuous = static_cast<int>(scrollRemainder_);
        movement += continuous; scrollRemainder_ -= continuous;
        if (controls.pageDown) { movement += 240; }
        if (controls.pageUp) { movement -= 240; }
        scroll_ = std::clamp(scroll_ + movement, 0, maxScroll_); return;
    }
    if (screen_ == Screen::Dying) {
        accumulator_ += std::min(elapsed, 0.25f);
        float interval = 0.08f;
        if (world_.deathWipeActive) { interval = 0.1f; }
        while (accumulator_ >= interval) {
            accumulator_ -= interval;
            const bool finished = world_.advanceDeathEffect();
            playWorldSounds();
            if (finished) { showScreen(Screen::Failed); break; }
            if (world_.deathWipeActive) { interval = 0.1f; }
        }
        return;
    }
    if (screen_ == Screen::Failed) {
        messageTime_ += elapsed;
        if (confirm && messageTime_ >= messageDuration(assets_.text[12])) { startLevel(world_.level.id, true); }
        else if (controls.cancel) { showScreen(Screen::Main); }
        return;
    }
    if (screen_ == Screen::Completed) {
        messageTime_ += elapsed;
        int row = 13;
        if (world_.level.id == 8) { row = 15; }
        if (confirm && messageTime_ >= messageDuration(assets_.text[row])) {
            world_.cheat.disable(world_);
            if (world_.level.id < 8) {
                checkpoint_ = world_.inventory; progress_.checkpoint = checkpoint_;
                progress_.difficulty = difficulty_; progress_.unlocked = std::max(progress_.unlocked, world_.level.id + 1);
                save(); startLevel(world_.level.id + 1, true);
            }
            else { showScreen(Screen::About); }
        }
        return;
    }
    if (screen_ != Screen::Playing) { menuInput(controls); return; }
    if (!focused || controls.cancel) { showScreen(Screen::Pause); return; }
    for (unsigned char character : controls.textInput) { world_.cheat.input(character, world_, assets_); }
    if (world_.inCameraTransit()) {
        queuePlayerInput(controls);
        accumulator_ += std::min(elapsed, 0.25f);
        while (accumulator_ >= 0.05f) {
            accumulator_ -= 0.05f;
            if (world_.advanceCameraTransition(controls.player.down)) {
                world_.tick(takePlayerInput(controls.player.fire));
                playWorldSounds();
                accumulator_ = 0;
                break;
            }
        }
        return;
    }
    if (!world_.message.empty()) {
        messageTime_ += elapsed;
        if (messageTime_ >= messageDuration(world_.message)
            && (confirm || controls.up || controls.down || controls.player.left || controls.player.right)) {
            world_.dismissMessage(); messageTime_ = 0;
        }
        accumulator_ = 0; return;
    }
    queuePlayerInput(controls);
    if (controls.debug) { collisionDebug_ = !collisionDebug_; }
    accumulator_ += std::min(elapsed, 0.25f);
    while (accumulator_ >= 0.08f) {
        world_.tick(takePlayerInput(controls.player.fire));
        accumulator_ -= 0.08f;
        playWorldSounds();
        if (world_.dead) { world_.beginDeathEffect(); showScreen(Screen::Dying); break; }
        if (world_.complete) {
            world_.prepareMessage();
            playWorldSounds();
            showScreen(Screen::Completed);
            break;
        }
        if (!world_.message.empty()) {
            world_.prepareMessage();
            playWorldSounds();
            messageTime_ = 0; accumulator_ = 0; pending_ = Input{};
            if (world_.dead) { world_.beginDeathEffect(); showScreen(Screen::Dying); }
            break;
        }
        if (world_.inCameraTransit()) { accumulator_ = 0; break; }
    }
}
void Game::draw() {
    const int menuPhase = static_cast<int>(menuTime_ / 0.03f) * 4;
    int spread = 0;
    if (selectionTime_ < 0.3f) {
        int halfWidth = 81;
        if (screen_ == Screen::Pause) { halfWidth = 45; }
        spread = halfWidth - static_cast<int>(selectionTime_ / 0.03f) * (halfWidth / 10);
    }
    switch (screen_) {
    case Screen::Intro:
        ClearBackground(WHITE);
        if (introTime_ < 3.0f) { renderer_.image("publisher_logo", 0, 0); }
        else { ClearBackground(BLACK); renderer_.image("intro_logo", 67, 131); }
        break;
    case Screen::SoundPrompt:
        // The original loop shows a blank screen for 200ms, then the prompt for 1000ms.
        renderer_.soundPrompt(std::fmod(soundPromptTime_, 1.2f) >= 0.2f); break;
    case Screen::Playing: case Screen::Pause: case Screen::Dying: case Screen::Failed: case Screen::Completed:
        renderer_.world(world_, collisionDebug_, screen_ != Screen::Pause);
        if (screen_ == Screen::Dying || screen_ == Screen::Failed) { renderer_.deathWipe(world_); }
        if (screen_ == Screen::Pause) { renderer_.menu(title_, items_, selection_, enabled_, true, menuPhase, spread); }
        if (screen_ == Screen::Failed) { renderer_.message(assets_.text[12], true, messageTime_); }
        if (screen_ == Screen::Completed) {
            int row = 13; if (world_.level.id == 8) { row = 15; }
            renderer_.message(assets_.text[row], false, messageTime_);
        }
        if (screen_ == Screen::Playing && !world_.message.empty()) { renderer_.message(world_.message, false, messageTime_); }
        break;
    case Screen::Help: maxScroll_ = renderer_.richPage(help_, scroll_); break;
    case Screen::About: maxScroll_ = renderer_.richPage(about_, scroll_); break;
    default: renderer_.menu(title_, items_, selection_, enabled_, false, menuPhase, spread); break;
    }
}
}
