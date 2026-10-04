/** @file config.cpp
 * @brief Read key=value settings with whitespace and semicolon comments.
 */
#include "config.h"
#include <charconv>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

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

// Generate the editable file from the same constants used for missing keys.
void writeDefaultConfig(const std::filesystem::path& path) {
    std::ofstream stream(path);
    stream << "; Game window title\n"
           << config::TitleKey << '=' << config::DefaultTitle << "\n\n"
           << "; Base screen width before scaling\n"
           << config::ScreenXKey << '=' << config::DefaultScreenX << "\n\n"
           << "; Base screen height before scaling\n"
           << config::ScreenYKey << '=' << config::DefaultScreenY << "\n\n"
           << "; Initial integer scale\n"
           << config::ScalarKey << '=' << config::DefaultScalar << '\n';
    stream.close();
    if (!stream) {
        throw std::runtime_error("Cannot create config: " + path.u8string());
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
        } else {
            throw std::runtime_error(location + "Unknown setting: " + key);
        }
    }
    if (!stream.eof()) {
        throw std::runtime_error("Cannot read config: " + path.u8string());
    }
    // Check before multiplying the base dimensions by the requested scale.
    const int maximumBase = std::numeric_limits<int>::max() / config.scalar;
    if (config.defaultScreenX > maximumBase || config.defaultScreenY > maximumBase) {
        throw std::runtime_error("Config window dimensions overflow: " + path.u8string());
    }
    std::cout << "Config loaded: " << path.u8string() << '\n';
    return config;
}
}
