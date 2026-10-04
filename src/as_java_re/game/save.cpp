/** @file save.cpp
 * @brief Persist level-start checkpoints with atomic replacement and backup recovery.
 */
#include "game/save.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <system_error>

namespace alien {
SaveStore::SaveStore(const Assets& assets, std::filesystem::path directory)
    : assets_(assets), directory_(std::move(directory)) {}

Progress SaveStore::read(const std::filesystem::path& path) const {
    std::ifstream stream(path);
    const auto data = Json::parse(stream);
    if (data.at("version") != save_file::Version) { throw std::runtime_error("Unknown save version."); }
    Progress result;
    result.unlocked = std::clamp(data.at("unlocked").get<int>(), 0, assets_.levelCount());
    result.difficulty = std::clamp(data.at("difficulty").get<int>(), 0, rules::DifficultyCount - 1);
    result.sound = data.at("sound").get<bool>();
    const auto& inventory = data.at("checkpoint");
    result.checkpoint.owned = inventory.at("owned").get<std::array<bool, rules::WeaponCount>>();
    result.checkpoint.ammo = inventory.at("ammo").get<std::array<int, rules::WeaponCount>>();
    result.checkpoint.score = std::max(0, inventory.at("score").get<int>());
    result.checkpoint.armor = std::clamp(inventory.at("armor").get<int>(), 0, rules::HeavyArmor);
    result.checkpoint.tutorialFlags = inventory.value("tutorial_flags", 0U);
    result.checkpoint.owned[rules::Pistol] = true;
    result.checkpoint.ammo[rules::Pistol] = rules::PistolAmmo;
    for (int weapon = rules::Pistol + 1; weapon < rules::WeaponCount; ++weapon) {
        result.checkpoint.ammo[weapon] = std::clamp(result.checkpoint.ammo[weapon], 0, assets_.projectiles[weapon].capacity);
    }
    return result;
}

Progress SaveStore::load() {
    primaryValid_ = false;
    // Return only fully parsed candidates so corrupt fields cannot partially overwrite defaults.
    for (const std::string name : {save_file::Primary, save_file::Backup}) {
        const auto path = directory_ / name;
        if (!std::filesystem::exists(path)) { continue; }
        try {
            auto progress = read(path);
            primaryValid_ = name == save_file::Primary;
            if (!primaryValid_) { std::cerr << "Save recovered from backup.\n"; }
            return progress;
        } catch (const std::exception& error) {
            std::cerr << "Save ignored (" << name << "): " << error.what() << '\n';
        }
    }
    return Progress{};
}

void SaveStore::write(const Progress& progress) {
    std::filesystem::create_directories(directory_);
    const auto& inventory = progress.checkpoint;
    const Json data{
        {"version", save_file::Version}, {"unlocked", progress.unlocked}, {"difficulty", progress.difficulty}, {"sound", progress.sound},
        {"checkpoint", {{"owned", inventory.owned}, {"ammo", inventory.ammo}, {"score", inventory.score},
                        {"armor", inventory.armor}, {"tutorial_flags", inventory.tutorialFlags}}}
    };
    const auto path = directory_ / save_file::Primary;
    const auto temporary = directory_ / save_file::Temporary;
    std::ofstream stream(temporary, std::ios::trunc);
    stream << data.dump(2);
    stream.flush();
    if (!stream) { throw std::runtime_error("Cannot write game save."); }
    stream.close();
    if (std::filesystem::exists(path)) {
        if (primaryValid_) {
            std::filesystem::copy_file(path, directory_ / save_file::Backup, std::filesystem::copy_options::overwrite_existing);
        } else {
            // Retain a corrupt primary file for diagnosis without overwriting the valid backup.
            std::filesystem::copy_file(path, directory_ / save_file::Corrupt, std::filesystem::copy_options::skip_existing);
        }
    }
    // Commit with a same-volume Windows replacement; failures preserve the old primary and backup.
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot commit game save");
    }
    primaryValid_ = true;
}
}
