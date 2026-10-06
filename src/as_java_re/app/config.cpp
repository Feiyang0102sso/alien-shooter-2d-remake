/** @file config.cpp
 * @brief Read key=value settings with whitespace and semicolon comments.
 */
#include "app/config.h"
#include <cctype>
#include <charconv>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace alien {
namespace {
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

int positiveInteger(const std::string& text, const std::string& location) {
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value <= 0) {
        throw std::runtime_error(location + "Expected a positive integer.");
    }
    return value;
}

// Switches are written as 0 or 1.
bool switchValue(const std::string& text, const std::string& location) {
    if (text == "0") {
        return false;
    }
    if (text == "1") {
        return true;
    }
    throw std::runtime_error(location + "Expected 0 or 1.");
}

// Key bindings are one letter a-z or "spacebar", case-insensitive; returns the raylib key code.
int keyBinding(const std::string& text, const std::string& location) {
    std::string name;
    for (const char character : text) {
        name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
    }
    if (name == config::SpacebarName) {
        return KEY_SPACE;
    }
    if (name.size() != 1 || name[0] < 'a' || name[0] > 'z') {
        throw std::runtime_error(location + "Expected a letter a-z or " + config::SpacebarName + ".");
    }
    if (std::string(config::ReservedKeys).find(name[0]) != std::string::npos) {
        throw std::runtime_error(location + "Key is already used for movement or weapon switching.");
    }
    // raylib letter key codes are consecutive from KEY_A.
    return KEY_A + (name[0] - 'a');
}

// One editable setting as written to the file: comment lines, key and default value.
struct ConfigEntry {
    std::vector<std::string> comment;
    std::string key;
    std::string defaultValue;
};

// Every setting in file order; new files and missing keys use the same text.
std::vector<ConfigEntry> configEntries() {
    return {
        {{"Game window title"}, config::TitleKey, config::DefaultTitle},
        {{"Base screen width before scaling"}, config::ScreenXKey, std::to_string(config::DefaultScreenX)},
        {{"Base screen height before scaling"}, config::ScreenYKey, std::to_string(config::DefaultScreenY)},
        {{"Initial integer scale"}, config::ScalarKey, std::to_string(config::DefaultScalar)},
        {{"Frame rate limit; game logic still runs at the original 80ms tick"}, config::MaxFpsKey,
         std::to_string(config::DefaultMaxFps)},
        {{"Desktop control assists. 1: on, 0: original controls",
          "  - Jump buffer: jump pressed up to " + std::to_string(rules::JumpBufferTicks * TickMilliseconds)
              + "ms before landing still jumps",
          "  - Lift exit: walking off a lift lands on floor within " + std::to_string(rules::LiftExitTolerance)
              + "px of the feet",
          "  - Lift jump: a rising jump keeps air steering instead of snapping back onto the lift",
          "  - Crouched fire: every weapon fires crouched with a lower muzzle (original: pistol only)"},
         config::ModernControlsKey, std::to_string(static_cast<int>(config::DefaultModernControls))},
        {{"Fall protection. 1: one fall costs at most " + std::to_string(rules::FallProtectPercent)
              + "% of current health, 0: original fall damage",
          "  Falling out of the map still kills"},
         config::FallProtectKey, std::to_string(static_cast<int>(config::DefaultFallProtect))},
        {{"Extra fire key; the left mouse button always fires",
          "  Accepts one letter a-z or " + std::string(config::SpacebarName) + ", except "
              + config::ReservedKeys + " (movement and weapon switching)"},
         config::FireBindingKey, config::DefaultFireKey},
        {{"Jump key; same values as " + std::string(config::FireBindingKey) + ", e.g. "
              + config::JumpBindingKey + "=" + config::SpacebarName},
         config::JumpBindingKey, config::DefaultJumpKey},
    };
}

void writeEntry(std::ofstream& stream, const ConfigEntry& entry) {
    for (const auto& line : entry.comment) {
        stream << "; " << line << '\n';
    }
    stream << entry.key << '=' << entry.defaultValue << '\n';
}

// Generate the editable file from the same constants used for missing keys.
void writeDefaultConfig(const std::filesystem::path& path) {
    std::ofstream stream(path);
    bool first = true;
    for (const auto& entry : configEntries()) {
        if (!first) {
            stream << '\n';
        }
        writeEntry(stream, entry);
        first = false;
    }
    stream.close();
    if (!stream) {
        throw std::runtime_error("Cannot create config: " + path.u8string());
    }
}

// Append settings added by newer versions; existing lines and user values stay untouched.
void appendMissingEntries(const std::filesystem::path& path, const std::set<std::string>& presentKeys) {
    std::vector<ConfigEntry> missing;
    for (const auto& entry : configEntries()) {
        if (presentKeys.count(entry.key) == 0) {
            missing.push_back(entry);
        }
    }
    if (missing.empty()) {
        return;
    }
    std::ofstream stream(path, std::ios::app);
    for (const auto& entry : missing) {
        // The leading newline also ends a last line written without one.
        stream << '\n';
        writeEntry(stream, entry);
        std::cout << "Config key added: " << entry.key << '=' << entry.defaultValue << '\n';
    }
    stream.close();
    if (!stream) {
        throw std::runtime_error("Cannot update config: " + path.u8string());
    }
}
}

Config loadConfig(const std::filesystem::path& path) {
    Config config;
    if (!std::filesystem::exists(path)) {
        writeDefaultConfig(path);
        std::cout << "Default config created: " << path.u8string() << '\n';
        return config;
    }
    std::ifstream stream(path);
    if (!stream) {
        throw std::runtime_error("Cannot open config: " + path.u8string());
    }

    std::string line;
    int lineNumber = 0;
    std::set<std::string> presentKeys;
    while (std::getline(stream, line)) {
        ++lineNumber;
        // Windows text editors may prefix a UTF-8 file with a byte-order mark.
        if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            line.erase(0, 3);
        }
        const auto comment = line.find(';');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const auto location = path.u8string() + ":" + std::to_string(lineNumber) + ": ";
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            throw std::runtime_error(location + "Expected key=value.");
        }
        const auto key = trim(line.substr(0, separator));
        const auto value = trim(line.substr(separator + 1));
        if (key == config::TitleKey) {
            if (value.empty()) {
                throw std::runtime_error(location + "Title must not be empty.");
            }
            config.title = value;
        } else if (key == config::ScreenXKey) {
            config.defaultScreenX = positiveInteger(value, location);
        } else if (key == config::ScreenYKey) {
            config.defaultScreenY = positiveInteger(value, location);
        } else if (key == config::ScalarKey) {
            config.scalar = positiveInteger(value, location);
        } else if (key == config::MaxFpsKey) {
            config.maxFps = positiveInteger(value, location);
        } else if (key == config::ModernControlsKey) {
            config.modernControls = switchValue(value, location);
        } else if (key == config::FallProtectKey) {
            config.fallProtect = switchValue(value, location);
        } else if (key == config::FireBindingKey) {
            config.fireKey = keyBinding(value, location);
        } else if (key == config::JumpBindingKey) {
            config.jumpKey = keyBinding(value, location);
        } else {
            throw std::runtime_error(location + "Unknown setting: " + key);
        }
        presentKeys.insert(key);
    }
    if (!stream.eof()) {
        throw std::runtime_error("Cannot read config: " + path.u8string());
    }
    // Check before multiplying the base dimensions by the requested scale.
    const int maximumBase = std::numeric_limits<int>::max() / config.scalar;
    if (config.defaultScreenX > maximumBase || config.defaultScreenY > maximumBase) {
        throw std::runtime_error("Config window dimensions overflow: " + path.u8string());
    }
    if (config.fireKey == config.jumpKey) {
        throw std::runtime_error("Config FireKey and JumpKey must differ: " + path.u8string());
    }
    // Only a valid file is extended, after the read stream is closed.
    stream.close();
    appendMissingEntries(path, presentKeys);
    std::cout << "Config loaded: " << path.u8string() << '\n';
    return config;
}
}
