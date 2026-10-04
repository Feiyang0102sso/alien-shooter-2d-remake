/** @file game.h
 * @brief Coordinate desktop input, original screens, level progression, and saves.
 */
#pragma once
#include "audio/audio.h"
#include "render/renderer.h"
#include "game/save.h"
#include <memory>

namespace alien {
enum class Screen { Intro, SoundPrompt, Main, Difficulty, Levels, Settings, Help, About, Playing, Pause, Dying, Failed, Completed };

// One desktop input snapshot shared by tests and the real game window.
struct Controls {
    // player.fire is the held state; leftClick is this desktop frame's press edge.
    Input player;
    Vector2 mouse{-1, -1};
    bool confirm = false, cancel = false, up = false, down = false;
    bool leftClick = false, rightClick = false, yes = false, no = false;
    bool pageUp = false, pageDown = false, debug = false;
    bool scrollUp = false;
    float wheel = 0;
    std::string textInput;
};

class Game {
public:
    Game(const Assets& assets, std::filesystem::path saveDirectory, bool silent, std::uint64_t seed = DefaultRandomSeed);
    void update(float elapsed, Vector2 mouse, bool focused);
    void update(float elapsed, const Controls& controls, bool focused);
    void draw();
    void startLevel(int level, bool carry = false);
    void showScreen(Screen screen);
    bool exiting() const { return exit_; }
    World& world() { return world_; }
    Screen screen() const { return screen_; }
    const Progress& progress() const { return progress_; }
private:
    void menuInput(const Controls& controls);
    void choose();
    void back();
    void buildMenu();
    void save();
    void setSound(bool enabled);
    void playWorldSounds();
    void queuePlayerInput(const Controls& controls);
    Input takePlayerInput(bool heldFire);
    const std::vector<std::string>& completionMessage() const;
    const Assets& assets_;
    Renderer renderer_;
    std::unique_ptr<Audio> audio_;
    World world_;
    SaveStore saveStore_;
    Progress progress_;
    Inventory checkpoint_;
    Screen screen_ = Screen::Intro;
    std::vector<std::string> items_;
    std::vector<bool> enabled_;
    std::string title_, help_, about_;
    Input pending_;
    float accumulator_ = 0, introTime_ = 0, messageTime_ = 0, scrollRemainder_ = 0;
    float menuTime_ = 0, selectionTime_ = 1, soundPromptTime_ = 0;
    int selection_ = 0, mainSelection_ = 0, difficulty_ = rules::DefaultDifficulty, scroll_ = 0, maxScroll_ = 0;
    bool exit_ = false, sound_ = true, collisionDebug_ = false;
};
}
