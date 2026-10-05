/** @file game.cpp
 * @brief Coordinate desktop input, original screens, level progression, and saves.
 */
#include "game/game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

namespace alien {
Game::Game(const Assets& assets, std::filesystem::path saveDirectory, bool silent, std::uint64_t seed)
    : assets_(assets), renderer_(assets), world_(assets), saveStore_(assets, std::move(saveDirectory)) {
    world_.seedRandom(seed);
    // The asset pipeline already writes desktop control labels into the help page.
    help_ = assets_.helpText;
    about_ = assets_.aboutText;
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
const std::vector<std::string>& Game::completionMessage() const {
    if (world_.level.id == assets_.levelCount()) { return assets_.text.message(message_key::GameComplete); }
    return assets_.text.message(message_key::LevelComplete);
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
    if (screen == Screen::Main && audio_) { audio_->play(sound::MainMenuMusic); }
    if (screen == Screen::Pause && audio_) { audio_->play(sound::LevelMusic); }
    buildMenu();
}
void Game::buildMenu() {
    const auto& text = assets_.text;
    items_.clear(); enabled_.clear(); title_.clear();
    switch (screen_) {
    case Screen::Main:
        items_ = {text.label("new_game"), text.label("continue"), text.label("select_level"), text.label("settings"),
                  text.label("help"), text.label("about"), text.label("exit")};
        enabled_ = {true, progress_.unlocked > 0, progress_.unlocked > 0, true, true, true, true}; selection_ = mainSelection_; break;
    case Screen::Difficulty:
        title_ = text.label("new_game"); items_ = {text.label("easy"), text.label("normal"), text.label("hard")}; selection_ = difficulty_; break;
    case Screen::Levels:
        title_ = text.label("select_level");
        for (int level = 1; level <= progress_.unlocked; ++level) { items_.push_back(text.label("level_prefix") + std::to_string(level)); }
        break;
    case Screen::Settings:
        title_ = text.label("settings");
        if (sound_) { items_.push_back(text.label("sound_on")); }
        else { items_.push_back(text.label("sound_off")); }
        // Vibration has no desktop equivalent and stays disabled.
        items_.push_back(text.label("vibration_off")); enabled_ = {true, false};
        break;
    case Screen::Pause:
        items_ = {text.label("continue"), text.label("sound_off"), text.label("vibration_off"), text.label("restart"),
                  text.label("menu"), text.label("exit")};
        if (sound_) { items_[1] = text.label("sound_on"); }
        enabled_ = {true, true, false, true, true, true}; break;
    default: break;
    }
    if (enabled_.empty()) { enabled_.resize(items_.size(), true); }
}
void Game::startLevel(int level, bool carry) {
    if (!carry) { checkpoint_ = Inventory{}; }
    world_.load(level, difficulty_, &checkpoint_);
    showScreen(Screen::Playing);
    if (audio_) { audio_->play(sound::LevelMusic); }
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
        // Clicks on the soft-key bar act as SELECT (left) and BACK (right).
        if (mouse.y >= layout::SoftKeyClickY) {
            if (mouse.x < layout::SoftKeyLeftClickWidth) { choose(); }
            if (mouse.x > layout::SoftKeyRightClickX) { back(); }
            return;
        }
        const auto box = menuLayout(static_cast<int>(items_.size()), !title_.empty(), screen_ == Screen::Pause);
        const int item = static_cast<int>(mouse.y - box.firstItem) / layout::MenuItemHeight;
        if (mouse.x >= box.left && mouse.x < box.left + box.width && mouse.y >= box.firstItem
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
    // Keys 1-4 select the four guns; 5 selects the pistol.
    for (int key = KEY_ONE; key <= KEY_FOUR; ++key) {
        if (IsKeyPressed(key)) {
            controls.player.weapon = key - KEY_ONE + 1;
        }
    }
    if (IsKeyPressed(KEY_FIVE)) { controls.player.weapon = rules::Pistol; }
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
    if (!items_.empty()) { menuTime_ += std::min(elapsed, MaxFrameSeconds); selectionTime_ += elapsed; }
    if (screen_ == Screen::Intro) {
        introTime_ += elapsed;
        if (introTime_ >= flow::IntroSeconds || confirm) { showScreen(Screen::SoundPrompt); }
        return;
    }
    if (screen_ == Screen::SoundPrompt) {
        soundPromptTime_ += elapsed;
        bool accept = controls.confirm || controls.yes;
        bool decline = controls.no || controls.cancel || controls.rightClick;
        if (controls.leftClick && controls.mouse.y >= layout::PromptClickY && controls.mouse.y < ScreenHeight) {
            accept = controls.mouse.x >= 0 && controls.mouse.x < layout::SoftKeyLeftClickWidth;
            decline = controls.mouse.x >= layout::PromptRightClickX && controls.mouse.x < ScreenWidth;
        }
        if (accept) { setSound(true); save(); showScreen(Screen::Main); }
        else if (decline) { setSound(false); save(); showScreen(Screen::Main); }
        return;
    }
    if (screen_ == Screen::Help || screen_ == Screen::About) {
        if (controls.cancel || controls.rightClick || (controls.leftClick && controls.mouse.y >= layout::PromptClickY)) { back(); return; }
        int movement = -static_cast<int>(controls.wheel * flow::PageWheelStep);
        if (controls.player.down) { scrollRemainder_ += elapsed * flow::PageScrollPerSecond; }
        if (controls.scrollUp) { scrollRemainder_ -= elapsed * flow::PageScrollPerSecond; }
        const int continuous = static_cast<int>(scrollRemainder_);
        movement += continuous; scrollRemainder_ -= continuous;
        if (controls.pageDown) { movement += flow::PageJump; }
        if (controls.pageUp) { movement -= flow::PageJump; }
        scroll_ = std::clamp(scroll_ + movement, 0, maxScroll_); return;
    }
    if (screen_ == Screen::Dying) {
        accumulator_ += std::min(elapsed, MaxFrameSeconds);
        float interval = DeathParticleStepSeconds;
        if (world_.deathWipeActive) { interval = DeathWipeStepSeconds; }
        while (accumulator_ >= interval) {
            accumulator_ -= interval;
            const bool finished = world_.advanceDeathEffect();
            playWorldSounds();
            if (finished) { showScreen(Screen::Failed); break; }
            if (world_.deathWipeActive) { interval = DeathWipeStepSeconds; }
        }
        return;
    }
    if (screen_ == Screen::Failed) {
        messageTime_ += elapsed;
        if (confirm && messageTime_ >= messageDuration(assets_.text.message(message_key::LevelFailed))) { startLevel(world_.level.id, true); }
        else if (controls.cancel) { showScreen(Screen::Main); }
        return;
    }
    if (screen_ == Screen::Completed) {
        messageTime_ += elapsed;
        if (confirm && messageTime_ >= messageDuration(completionMessage())) {
            world_.cheat.disable(world_);
            if (world_.level.id < assets_.levelCount()) {
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
        accumulator_ += std::min(elapsed, MaxFrameSeconds);
        while (accumulator_ >= CameraTransitStepSeconds) {
            accumulator_ -= CameraTransitStepSeconds;
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
    accumulator_ += std::min(elapsed, MaxFrameSeconds);
    while (accumulator_ >= TickSeconds) {
        world_.tick(takePlayerInput(controls.player.fire));
        accumulator_ -= TickSeconds;
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
    const int menuPhase = static_cast<int>(menuTime_ / flow::MenuAnimationStepSeconds) * flow::MenuPhasePerStep;
    int spread = 0;
    if (selectionTime_ < flow::SelectionSpreadSeconds) {
        int halfWidth = layout::MenuSpreadHalfWidth;
        if (screen_ == Screen::Pause) { halfWidth = layout::PauseSpreadHalfWidth; }
        spread = halfWidth - static_cast<int>(selectionTime_ / flow::MenuAnimationStepSeconds) * (halfWidth / flow::SelectionSpreadSteps);
    }
    // Smooth movement between 80ms ticks only while the world clock runs; other screens show the tick as is.
    float blend = 1.0f;
    if (screen_ == Screen::Playing && world_.message.empty() && !world_.inCameraTransit()) {
        blend = std::min(accumulator_ / TickSeconds, 1.0f);
    }
    renderer_.setBlend(blend);
    switch (screen_) {
    case Screen::Intro:
        ClearBackground(WHITE);
        if (introTime_ < flow::PublisherLogoSeconds) { renderer_.image("publisher_logo", 0, 0); }
        else { ClearBackground(BLACK); renderer_.image("intro_logo", layout::IntroLogoX, layout::IntroLogoY); }
        break;
    case Screen::SoundPrompt:
        // The original loop shows a blank screen for 200ms, then the prompt for 1000ms.
        renderer_.soundPrompt(std::fmod(soundPromptTime_, flow::SoundPromptCycleSeconds) >= flow::SoundPromptBlankSeconds); break;
    case Screen::Playing: case Screen::Pause: case Screen::Dying: case Screen::Failed: case Screen::Completed:
        renderer_.world(world_, collisionDebug_, screen_ != Screen::Pause);
        if (screen_ == Screen::Dying || screen_ == Screen::Failed) { renderer_.deathWipe(world_); }
        if (screen_ == Screen::Pause) { renderer_.menu(title_, items_, selection_, enabled_, true, menuPhase, spread); }
        if (screen_ == Screen::Failed) { renderer_.message(assets_.text.message(message_key::LevelFailed), true, messageTime_); }
        if (screen_ == Screen::Completed) { renderer_.message(completionMessage(), false, messageTime_); }
        if (screen_ == Screen::Playing && !world_.message.empty()) { renderer_.message(world_.message, false, messageTime_); }
        break;
    case Screen::Help: maxScroll_ = renderer_.richPage(help_, scroll_); break;
    case Screen::About: maxScroll_ = renderer_.richPage(about_, scroll_); break;
    default: renderer_.menu(title_, items_, selection_, enabled_, false, menuPhase, spread); break;
    }
}
}
