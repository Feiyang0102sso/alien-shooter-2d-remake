/** @file ui_snapshots.cpp
 * @brief Verify ui snapshots against runtime and original-game contracts.
 */
#include "render/renderer.h"
#include <filesystem>
#include <iostream>

namespace {
void saveSnapshot(RenderTexture2D target, const std::filesystem::path& path) {
    auto screenshot = LoadImageFromTexture(target.texture); ImageFlipVertical(&screenshot);
    const bool saved = ExportImage(screenshot, path.string().c_str());
    UnloadImage(screenshot);
    if (!saved) { throw std::runtime_error("Cannot write UI snapshot."); }
}
}

// Render fixed menu animation phases on the GPU independently of monitor refresh rate.
int main(int argc, char** argv) {
    try {
        if (argc != 3) { throw std::runtime_error("Expected assets and output directory."); }
        alien::Assets assets(argv[1]); assets.load();
        const std::filesystem::path output(argv[2]); std::filesystem::create_directories(output);
        SetTraceLogLevel(LOG_WARNING); SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(240, 320, "UI snapshot verification");
        auto target = LoadRenderTexture(240, 320);
        {
            alien::Renderer renderer(assets);
            for (int frame = -1; frame < 12; ++frame) {
                int spread = 0;
                if (frame >= 0 && frame < 10) { spread = 81 - frame * 8; }
                BeginTextureMode(target);
                renderer.menu("", {"NEW GAME", "CONTINUE", "SELECT LEVEL", "SETTINGS", "HELP", "ABOUT", "EXIT"},
                    0, {true, false, false, true, true, true, true}, false, std::max(0, frame * 4), spread);
                EndTextureMode();
                std::string name = "menu-animation-" + std::to_string(frame) + ".png";
                if (frame == -1) { name = "menu-menu.png"; }
                saveSnapshot(target, output / name);
            }
            BeginTextureMode(target);
            renderer.menu("NEW GAME", {"EASY", "NORMAL", "HARD"}, 1, {true, true, true});
            EndTextureMode(); saveSnapshot(target, output / "menu-difficulty.png");
            BeginTextureMode(target);
            renderer.menu("SETTINGS", {"SOUND ON", "VIBRATION OFF"}, 0, {true, false});
            EndTextureMode(); saveSnapshot(target, output / "menu-settings.png");
            for (int visible = 0; visible <= 1; ++visible) {
                BeginTextureMode(target); renderer.soundPrompt(visible != 0); EndTextureMode();
                saveSnapshot(target, output / ("sound-prompt-" + std::to_string(visible) + ".png"));
            }
        }
        UnloadRenderTexture(target); CloseWindow();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "UI snapshot failed: " << error.what() << '\n';
        if (IsWindowReady()) { CloseWindow(); }
        return 1;
    }
}
