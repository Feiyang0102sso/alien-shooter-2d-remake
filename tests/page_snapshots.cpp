/** @file page_snapshots.cpp
 * @brief Verify page snapshots against runtime and original-game contracts.
 */
#include "game.h"
#include <filesystem>
#include <iostream>

// Scroll text pages through game input and capture the GPU canvas for original-layout comparison.
int main(int argc, char** argv) {
    try {
        if (argc != 3) { throw std::runtime_error("Expected assets and output directory."); }
        alien::Assets assets(argv[1]); assets.load();
        const std::filesystem::path output(argv[2]); std::filesystem::create_directories(output);
        SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN); InitWindow(240, 320, "Text page verification");
        auto target = LoadRenderTexture(240, 320);
        {
            alien::Game game(assets, output / "unused-save", true);
            for (const std::string name : {"help", "about"}) {
                std::vector<int> offsets{0, 234, 332, 597, 649};
                alien::Screen screen = alien::Screen::Help;
                if (name == "about") { offsets = {0, 13}; screen = alien::Screen::About; }
                for (int offset : offsets) {
                    game.showScreen(screen);
                    BeginTextureMode(target); game.draw(); EndTextureMode();
                    alien::Controls controls; controls.wheel = -(offset + 0.01f) / 28.0f;
                    game.update(0, controls, true);
                    BeginTextureMode(target); game.draw(); EndTextureMode();
                    auto screenshot = LoadImageFromTexture(target.texture); ImageFlipVertical(&screenshot);
                    const auto path = output / (name + "-" + std::to_string(offset) + ".png");
                    if (!ExportImage(screenshot, path.string().c_str())) { throw std::runtime_error("Cannot write page snapshot."); }
                    UnloadImage(screenshot);
                }
            }
        }
        UnloadRenderTexture(target); CloseWindow();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Page snapshot failed: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        return 1;
    }
}
