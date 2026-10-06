/** @file config_tests.cpp
 * @brief Verify editable CFG files and executable-relative save storage.
 */
#include "app/config.h"
#include "platform/platform.h"
#include "game/save.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void writeConfig(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream stream(path, std::ios::binary);
    stream << contents;
    stream.close();
    require(static_cast<bool>(stream), "Cannot write test config.");
}

void expectInvalid(const std::filesystem::path& path, const std::string& contents) {
    writeConfig(path, contents);
    bool rejected = false;
    try {
        alien::loadConfig(path);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "Invalid config accepted: " + contents);
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected the asset directory.");
        const auto executable = alien::executableDirectory();
        const auto runId = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto directory = executable / ("config-fixtures-" + std::to_string(runId));
        std::filesystem::create_directories(directory);
        const auto path = directory / "config.cfg";

        // The first launch creates an editable file that can be read back unchanged.
        const auto defaults = alien::loadConfig(path);
        require(std::filesystem::exists(path), "Default config was not created.");
        require(defaults.title == "Alien Shooter 2D", "Wrong default title.");
        require(defaults.defaultScreenX == 240 && defaults.defaultScreenY == 320 && defaults.scalar == 4,
                "Wrong default window settings.");
        require(defaults.fireKey == KEY_J && defaults.jumpKey == KEY_W, "Wrong default key bindings.");
        const auto generated = alien::loadConfig(path);
        require(generated.fireKey == defaults.fireKey && generated.jumpKey == defaults.jumpKey,
                "Generated key bindings do not match defaults.");
        require(generated.title == defaults.title && generated.scalar == defaults.scalar,
                "Generated config does not match defaults.");

        // Exercise the actual user format, including CRLF, tabs, comments and UTF-8.
        const std::string custom = "\xEF\xBB\xBF; comment\r\n\r\n"
            " Title = 测试 Alien = Shooter \t ; title\r\n"
            "DefaultScreenX = 300 ; width\r\nDefaultScreenY=400\r\nScalar=2 ; scale\r\n";
        writeConfig(path, custom);
        const auto settings = alien::loadConfig(path);
        require(settings.title == "测试 Alien = Shooter", "Title or UTF-8 parsing failed.");
        require(settings.defaultScreenX == 300 && settings.defaultScreenY == 400 && settings.scalar == 2,
                "Custom dimensions or scale were ignored.");
        // Newer keys are appended once; the user's own lines stay byte-for-byte at the start.
        std::ifstream preserved(path, std::ios::binary);
        const std::string contents{std::istreambuf_iterator<char>(preserved), std::istreambuf_iterator<char>()};
        preserved.close();
        require(contents.compare(0, custom.size(), custom) == 0, "Loading changed the user's config lines.");
        require(contents.find("MaxFps=120") != std::string::npos && contents.find("ModernControls=1") != std::string::npos
                && contents.find("FallProtect=1") != std::string::npos
                && contents.find("FireKey=j") != std::string::npos && contents.find("JumpKey=w") != std::string::npos,
                "Missing config keys were not appended.");
        alien::loadConfig(path);
        std::ifstream reloaded(path, std::ios::binary);
        const std::string again{std::istreambuf_iterator<char>(reloaded), std::istreambuf_iterator<char>()};
        reloaded.close();
        require(again == contents, "A complete config was changed on reload.");

        writeConfig(path, "Scalar=1\n");
        const auto partial = alien::loadConfig(path);
        require(partial.title == defaults.title && partial.defaultScreenX == 240
                && partial.defaultScreenY == 320 && partial.scalar == 1, "Missing-key defaults failed.");

        // Key names are case-insensitive; spacebar is the only non-letter key.
        writeConfig(path, "FireKey = K\nJumpKey = SpaceBar\n");
        const auto rebound = alien::loadConfig(path);
        require(rebound.fireKey == KEY_K && rebound.jumpKey == KEY_SPACE, "Custom key bindings were ignored.");
        for (const std::string invalid : {"Scalar=0", "Scalar=-1", "Scalar=1.5", "Scalar=3oops",
                 "Scalar=9999999999999", "DefaultScreenX=0", "DefaultScreenY=-3", "Title= ; empty",
                 "Scalr=2", "Scalar", "DefaultScreenX=2147483647\nScalar=2", "MaxFps=0", "ModernControls=2", "FallProtect=2",
                 "FireKey=", "FireKey=1", "FireKey=jk", "FireKey=space", "JumpKey=a", "FireKey=E",
                 "FireKey=w", "FireKey=spacebar\nJumpKey=spacebar"}) {
            expectInvalid(path, invalid);
        }

        // The test runs from a different working directory; saves must still sit beside its EXE.
        const auto saveDirectory = alien::defaultSaveDirectory();
        require(saveDirectory == executable / "save", "Save path depends on the working directory.");
        alien::Assets assets(std::filesystem::u8path(argv[1]));
        assets.load();
        alien::SaveStore store(assets, saveDirectory);
        alien::Progress progress;
        progress.unlocked = 2;
        store.write(progress);
        require(std::filesystem::exists(executable / "save" / "progress.json"), "Save was written elsewhere.");
        require(store.load().unlocked == 2, "Executable-relative save could not be loaded.");
        std::cout << "Config and save path tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << '\n';
        return 1;
    }
}
