/** @file platform.cpp
 * @brief Resolve Unicode Windows paths and capture session diagnostics.
 */
#include "platform.h"
#include <Windows.h>
#include <cstdio>
#include <cstdlib>
#include <io.h>
#include <stdexcept>

namespace alien {
std::filesystem::path executableDirectory() {
    std::wstring path(32768, L'\0');
    const auto count = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (count == 0 || count >= path.size()) { throw std::runtime_error("Cannot locate the game executable."); }
    path.resize(count);
    return std::filesystem::path(path).parent_path();
}

std::filesystem::path defaultSaveDirectory() {
    wchar_t* value = nullptr;
    std::size_t length = 0;
    _wdupenv_s(&value, &length, L"LOCALAPPDATA");
    if (value) {
        const auto directory = std::filesystem::path(value) / "AlienShooterRemake";
        std::free(value);
        return directory;
    }
    return executableDirectory() / "save";
}

bool initializeLog(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) { return false; }
    const auto path = directory / "latest.log";
    FILE* output = nullptr;
    if (_wfreopen_s(&output, path.c_str(), L"w", stdout) != 0) { return false; }
    // Share one file descriptor for C++, raylib, and FluidSynth diagnostics.
    if (_dup2(_fileno(stdout), _fileno(stderr)) != 0) { return false; }
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    return true;
}

void showErrorDialog(const std::string& message) {
    const int length = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
    std::wstring text(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, text.data(), length);
    MessageBoxW(nullptr, text.c_str(), L"Alien Shooter", MB_OK | MB_ICONERROR);
}
}
