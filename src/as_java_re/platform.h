/** @file platform.h
 * @brief Resolve Unicode Windows paths and capture session diagnostics.
 */
#pragma once
#include <filesystem>
#include <string>

namespace alien {
// Keep Windows startup and Unicode path handling separate from gameplay rules.
std::filesystem::path executableDirectory();
std::filesystem::path defaultSaveDirectory();
bool initializeLog(const std::filesystem::path& directory);
void showErrorDialog(const std::string& message);
}
