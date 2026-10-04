/** @file config.h
 * @brief Define the settings loaded from the executable's config.cfg.
 */
#pragma once
#include <filesystem>
#include <string>

namespace alien {
namespace config {
// Keep file names, CFG keys, and defaults together for future changes.
inline constexpr char FileName[] = "config.cfg";
inline constexpr char TitleKey[] = "Title";
inline constexpr char ScreenXKey[] = "DefaultScreenX";
inline constexpr char ScreenYKey[] = "DefaultScreenY";
inline constexpr char ScalarKey[] = "Scalar";

inline constexpr char DefaultTitle[] = "Alien Shooter 2D";
inline constexpr int DefaultScreenX = 240;
inline constexpr int DefaultScreenY = 320;
inline constexpr int DefaultScalar = 3;
}

struct Config {
    std::string title = config::DefaultTitle;
    int defaultScreenX = config::DefaultScreenX;
    int defaultScreenY = config::DefaultScreenY;
    int scalar = config::DefaultScalar;
};

// Create missing files from defaults; missing keys use defaults and invalid values report an error.
Config loadConfig(const std::filesystem::path& path);
}
