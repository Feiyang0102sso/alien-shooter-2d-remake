/** @file assets.h
 * @brief Load original level, sprite, font, and MIDI metadata.
 */
#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace alien {
using Json = nlohmann::json;
inline constexpr int ScreenWidth = 240;
inline constexpr int ScreenHeight = 320;
inline constexpr int TileSize = 16;
inline constexpr int TickMilliseconds = 80;

// Preserve resource IDs and convert JSON into named fields at the loading boundary.
struct Sprite {
    std::string texture;
    int x = 0, y = 0, width = 0, height = 0;
    int offsetX = 0, offsetY = 0;
};
struct Glyph {
    Sprite image;
    int advance = 0;
};
struct Tile {
    Sprite image;
    int base = 0, overlay = 0;
};
struct EntityParameters {
    int speedX = 0, speedY = 0, weapon = 0, health = 0;
    int left = 0, right = 0, top = 0, bottom = 0;
    int damage = 0, motion = -1, jump = 0, gravity = -1;
};
struct ProjectileParameters {
    int speedX = 0, speedY = 0, damage = 0;
    int rangeX = 0, rangeY = 0, capacity = 0, cooldownMs = 0;
};
struct Spawn {
    int id = 0, type = 0, cellX = 0, cellY = 0;
    int spawnLimit = 0;
    std::array<int, 4> spawnTypes{};
};
struct Level {
    int id = 0, theme = 0, width = 0, height = 0;
    int playerCellX = 0, playerCellY = 0;
    std::vector<std::vector<int>> tiles;
    std::vector<std::vector<int>> collision;
    std::vector<Spawn> spawns;
};

class Assets {
public:
    explicit Assets(std::filesystem::path root);
    void load();
    Level loadLevel(int number) const;
    Json readJson(const std::string& relative) const;
    std::string readText(const std::string& relative) const;
    const std::filesystem::path& root() const { return root_; }

    std::array<std::array<Sprite, 2>, 155> parts{};
    std::array<std::array<int, 8>, 85> frames{};
    std::array<std::array<Tile, 220>, 3> tiles{};
    std::array<Glyph, 100> glyphs{};
    std::array<EntityParameters, 44> entities{};
    std::array<ProjectileParameters, 11> projectiles{};
    std::vector<std::vector<std::string>> text;
    std::array<std::string, 17> midiPaths{};
    Json interface;
    Json poses;
private:
    std::filesystem::path root_;
};
Sprite readSprite(const Json& source);
}
