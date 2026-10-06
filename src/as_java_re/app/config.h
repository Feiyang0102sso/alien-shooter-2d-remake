/** @file config.h
 * @brief Define the settings loaded from the executable's config.cfg.
 */
#pragma once
#include "core/constants.h"
#include <filesystem>
#include <raylib.h>
#include <string>

namespace alien {
// File name, CFG keys, and defaults live in core/constants.h (namespace config).

struct Config {
    std::string title = config::DefaultTitle;
    int defaultScreenX = config::DefaultScreenX;
    int defaultScreenY = config::DefaultScreenY;
    int scalar = config::DefaultScalar;
    int maxFps = config::DefaultMaxFps;
    bool modernControls = config::DefaultModernControls;
    bool fallProtect = config::DefaultFallProtect;
    // raylib key codes of config::DefaultFireKey and config::DefaultJumpKey.
    int fireKey = KEY_J;
    int jumpKey = KEY_W;
};

// Create missing files from defaults; missing keys use defaults and invalid values report an error.
Config loadConfig(const std::filesystem::path& path);
}
