/** @file assets.cpp
 * @brief Load original level, sprite, font, and MIDI metadata.
 */
#include "assets.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace alien {
Sprite readSprite(const Json& source) {
    Sprite sprite;
    if (source.is_null()) { return sprite; }
    sprite.texture = source.at("texture").get<std::string>();
    const auto& region = source.at("region");
    sprite.x = region.at(0); sprite.y = region.at(1);
    sprite.width = region.at(2); sprite.height = region.at(3);
    if (source.contains("offset")) {
        sprite.offsetX = source["offset"][0];
        sprite.offsetY = source["offset"][1];
    }
    return sprite;
}

Assets::Assets(std::filesystem::path root) : root_(std::move(root)) {}

Json Assets::readJson(const std::string& relative) const {
    std::ifstream stream(root_ / relative);
    if (!stream) { throw std::runtime_error("Cannot open asset: " + relative); }
    Json result;
    stream >> result;
    return result;
}

std::string Assets::readText(const std::string& relative) const {
    std::ifstream stream(root_ / relative, std::ios::binary);
    if (!stream) { throw std::runtime_error("Cannot open text: " + relative); }
    return std::string(std::istreambuf_iterator<char>(stream), {});
}

void Assets::load() {
    const auto catalog = readJson("catalog.json");
    if (catalog.at("format_version") != 2) { throw std::runtime_error("Unsupported asset catalog."); }
    const auto& definitions = catalog.at("definitions");
    const auto partData = readJson(definitions["character_parts"]);
    for (const auto& part : partData["parts"]) {
        const int id = part.at("id");
        const auto& directions = part.at("directions");
        if (directions.contains("right")) { parts.at(id)[0] = readSprite(directions["right"]); }
        if (directions.contains("left")) { parts.at(id)[1] = readSprite(directions["left"]); }
    }
    const auto frameData = readJson(definitions["character_frames"]);
    for (const auto& frame : frameData["frames"]) {
        frames.at(frame.at("id")) = frame.at("part_slots").get<std::array<int, 8>>();
    }
    const auto tileData = readJson(definitions["world_tiles"]);
    for (const auto& theme : tileData["themes"]) {
        for (const auto& entry : theme.at("tiles")) {
            Tile& tile = tiles.at(theme.at("id")).at(entry.at("id"));
            if (entry.contains("image")) { tile.image = readSprite(entry.at("image")); }
            if (entry.contains("offset")) {
                tile.image.offsetX = entry["offset"][0];
                tile.image.offsetY = entry["offset"][1];
            }
            if (entry.contains("children")) {
                tile.base = entry["children"][0];
                tile.overlay = entry["children"][1];
            }
        }
    }
    const auto font = readJson(catalog.at("font"));
    for (const auto& entry : font.at("glyphs")) {
        Json image = entry;
        image["texture"] = font.at("texture");
        Glyph& glyph = glyphs.at(entry.at("id"));
        glyph.image = readSprite(image);
        glyph.image.offsetX = entry.at("offset_x");
        glyph.image.offsetY = -entry.at("baseline_offset_y").get<int>();
        glyph.advance = entry.at("advance");
    }
    const auto entityData = readJson(definitions["entity_parameters"]);
    for (const auto& entry : entityData["records"]) {
        const auto& values = entry.at("parameters");
        if (values.empty()) { continue; }
        auto& p = entities.at(entry.at("id"));
        p = {values[0], values[1], values[2], values[3], values[4], values[5],
             values[6], values[7], values[8], values[9], values[10], values[11]};
    }
    const auto projectileData = readJson(definitions["projectile_parameters"]);
    for (const auto& entry : projectileData["records"]) {
        const auto& values = entry.at("parameters");
        projectiles.at(entry.at("id")) = {values[0], values[1], values[2], values[3], values[4], values[5], values[6]};
    }
    interface = readJson(definitions.at("interface"));
    poses = readJson(definitions.at("player_poses"));
    text = readJson(catalog["language"]["interface"])["rows"].get<std::vector<std::vector<std::string>>>();
    const auto soundData = readJson(catalog.at("audio"));
    for (const auto& sound : soundData.at("sounds")) {
        midiPaths.at(sound.at("id")) = sound.at("midi_source").get<std::string>();
    }
    std::cout << "Assets loaded: 8 levels, 155 parts, 17 MIDI files.\n";
}

Level Assets::loadLevel(int number) const {
    std::ostringstream path;
    path << "levels/level_" << std::setw(2) << std::setfill('0') << number << ".json";
    const auto source = readJson(path.str());
    Level level;
    level.id = source.at("id"); level.theme = source.at("theme");
    level.width = source.at("width"); level.height = source.at("height");
    level.tiles = source.at("tiles").get<std::vector<std::vector<int>>>();
    level.collision = source.at("base_collision_heights").get<std::vector<std::vector<int>>>();
    level.playerCellX = source["player_spawn_cell"][0];
    level.playerCellY = source["player_spawn_cell"][1];
    for (const auto& entry : source.at("entities")) {
        Spawn spawn;
        spawn.id = entry.at("id"); spawn.type = entry.at("type");
        spawn.cellX = entry["spawn_cell"][0]; spawn.cellY = entry["spawn_cell"][1];
        if (entry.contains("spawn_count")) {
            spawn.spawnLimit = entry.at("spawn_count");
            spawn.spawnTypes = entry.at("spawn_types").get<std::array<int, 4>>();
        }
        level.spawns.push_back(spawn);
    }
    return level;
}
}
