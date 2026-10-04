/** @file main.cpp
 * @brief Run the desktop window and fixed-resolution game canvas.
 */
#include "game/game.h"
#include "app/config.h"
#include "platform/platform.h"
#include "version.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
struct Options {
    std::filesystem::path assets, save, screenshot;
    int level = 0, frameLimit = 0;
    bool silent = false, hidden = false;
    std::string screen;
    std::uint64_t seed = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
};
Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--silent") { options.silent = true; continue; }
        if (option == "--hidden") { options.hidden = true; continue; }
        if (index + 1 >= argc) { throw std::runtime_error("Missing argument for " + option); }
        const std::string value = argv[++index];
        if (option == "--assets") { options.assets = std::filesystem::u8path(value); }
        else if (option == "--save-dir") { options.save = std::filesystem::u8path(value); }
        else if (option == "--screenshot") { options.screenshot = std::filesystem::u8path(value); }
        else if (option == "--frames") { options.frameLimit = std::stoi(value); }
        else if (option == "--level") { options.level = std::stoi(value); }
        else if (option == "--screen") { options.screen = value; }
        else if (option == "--seed") { options.seed = std::stoull(value); }
        else { throw std::runtime_error("Unknown argument: " + option); }
    }
    if (options.level < 0) { throw std::runtime_error("Level must be positive."); }
    return options;
}
}

int main(int argc, char** argv) {
    Options options;
    try {
        options = parseOptions(argc, argv);
        const auto executable = alien::executableDirectory();
        if (options.assets.empty()) { options.assets = executable / "assets"; }
        if (options.save.empty()) { options.save = alien::defaultSaveDirectory(); }
        if (!alien::initializeLog(options.save / "logs")) { std::cerr << "Session log unavailable.\n"; }
        std::cout << ALIEN_PRODUCT_NAME " " ALIEN_VERSION_STRING " started. Seed=" << options.seed << '\n';
        const auto config = alien::loadConfig(executable / alien::config::FileName);
        alien::Assets assets(options.assets);
        assets.load();
        if (options.level > assets.levelCount()) {
            throw std::runtime_error("Level must be 1-" + std::to_string(assets.levelCount()) + ".");
        }
        unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT;
        if (options.hidden) { flags |= FLAG_WINDOW_HIDDEN; }
        SetConfigFlags(flags);
        InitWindow(config.defaultScreenX * config.scalar, config.defaultScreenY * config.scalar, config.title.c_str());
        if (!IsWindowReady()) { throw std::runtime_error("Cannot initialize game window."); }
        SetExitKey(KEY_NULL);
        SetWindowMinSize(alien::ScreenWidth, alien::ScreenHeight);
        const int monitor = GetCurrentMonitor();
        // Keep the initial window within the monitor while preserving integer scaling.
        const int fittingScaleX = (GetMonitorWidth(monitor) - alien::config::WindowMargin) / config.defaultScreenX;
        const int fittingScaleY = (GetMonitorHeight(monitor) - alien::config::WindowMargin) / config.defaultScreenY;
        const int initialScale = std::max(1, std::min({config.scalar, fittingScaleX, fittingScaleY}));
        const int windowWidth = std::max(alien::ScreenWidth, config.defaultScreenX * initialScale);
        const int windowHeight = std::max(alien::ScreenHeight, config.defaultScreenY * initialScale);
        SetWindowSize(windowWidth, windowHeight);
        std::cout << "Window configured: " << config.title << ", " << windowWidth << 'x' << windowHeight
                  << ", scalar=" << initialScale << '\n';
        SetTargetFPS(alien::config::TargetFps);
        auto target = LoadRenderTexture(alien::ScreenWidth, alien::ScreenHeight);
        SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);
        {
            alien::Game game(assets, options.save, options.silent, options.seed);
            if (options.level > 0) { game.startLevel(options.level); }
            if (options.screen == "menu") { game.showScreen(alien::Screen::Main); }
            if (options.screen == "help") { game.showScreen(alien::Screen::Help); }
            if (options.screen == "about") { game.showScreen(alien::Screen::About); }
            if (options.screen == "difficulty") { game.showScreen(alien::Screen::Difficulty); }
            if (options.screen == "settings") { game.showScreen(alien::Screen::Settings); }
            if (options.screen == "pause" && options.level > 0) { game.showScreen(alien::Screen::Pause); }
            int frame = 0;
            while (!WindowShouldClose() && !game.exiting()) {
                const int scale = std::max(1, std::min(GetScreenWidth() / alien::ScreenWidth, GetScreenHeight() / alien::ScreenHeight));
                const int offsetX = (GetScreenWidth() - alien::ScreenWidth * scale) / 2;
                const int offsetY = (GetScreenHeight() - alien::ScreenHeight * scale) / 2;
                const auto rawMouse = GetMousePosition();
                Vector2 mouse{(rawMouse.x - offsetX) / scale, (rawMouse.y - offsetY) / scale};
                game.update(GetFrameTime(), mouse, options.hidden || IsWindowFocused());
                BeginTextureMode(target);
                game.draw();
                EndTextureMode();
                BeginDrawing();
                ClearBackground(BLACK);
                Rectangle source{0, 0, static_cast<float>(alien::ScreenWidth), -static_cast<float>(alien::ScreenHeight)};
                Rectangle destination{static_cast<float>(offsetX), static_cast<float>(offsetY), static_cast<float>(alien::ScreenWidth * scale), static_cast<float>(alien::ScreenHeight * scale)};
                DrawTexturePro(target.texture, source, destination, Vector2{}, 0, WHITE);
                EndDrawing();
                ++frame;
                if (options.frameLimit > 0 && frame >= options.frameLimit) {
                    if (!options.screenshot.empty()) {
                        if (!options.screenshot.parent_path().empty()) { std::filesystem::create_directories(options.screenshot.parent_path()); }
                        auto screenshot = LoadImageFromTexture(target.texture);
                        ImageFlipVertical(&screenshot);
                        if (!ExportImage(screenshot, options.screenshot.u8string().c_str())) { throw std::runtime_error("Cannot export screenshot."); }
                        UnloadImage(screenshot);
                    }
                    break;
                }
            }
        }
        UnloadRenderTexture(target);
        CloseWindow();
        std::cout << "Game closed normally.\n";
        return 0;
    } catch (const std::exception& error) {
        // The startup log uses stdout even when the desktop process has no stderr handle.
        std::cout << "Fatal: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        if (!options.hidden) {
            alien::showErrorDialog(std::string("The game could not start or continue.\n\n") + error.what()
                + "\n\nSee logs/latest.log in the save directory:\n" + options.save.u8string());
        }
        return 1;
    }
}
