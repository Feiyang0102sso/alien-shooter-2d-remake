/** @file assets.cpp
 * @brief Load original level, sprite, font, animation, composition, text, and MIDI metadata.
 */
#include "core/assets.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace alien {
namespace {
inline constexpr int CatalogFormatVersion = 3;

// Grow an id-indexed vector so that the id is addressable.
template <typename Value>
Value& slot(std::vector<Value>& values, int id) {
    if (id < 0) { throw std::runtime_error("Negative asset id: " + std::to_string(id)); }
    if (id >= static_cast<int>(values.size())) { values.resize(static_cast<std::size_t>(id) + 1); }
    return values[static_cast<std::size_t>(id)];
}

unsigned int packRgb(int red, int green, int blue) {
    return (static_cast<unsigned int>(red) << 16) | (static_cast<unsigned int>(green) << 8) | static_cast<unsigned int>(blue);
}

std::vector<unsigned int> readHexColors(const Json& values) {
    std::vector<unsigned int> colors;
    for (const auto& value : values) {
        colors.push_back(static_cast<unsigned int>(std::stoul(value.get<std::string>(), nullptr, 16)));
    }
    return colors;
}

ObjectLayer readLayer(const Json& source) {
    ObjectLayer layer;
    if (source.contains("part")) {
        layer.part = true;
        layer.id = source.at("part");
    } else {
        layer.id = source.at("tile");
    }
    layer.x = source.value("x", 0);
    layer.y = source.value("y", 0);
    layer.step = source.value("step", 1);
    layer.baseline = source.value("anchor", "top") == "baseline";
    const std::string frame = source.value("frame", "");
    if (frame == "variant") { layer.frame = ObjectLayer::Frame::Variant; }
    else if (frame == "visual_frame") { layer.frame = ObjectLayer::Frame::VisualFrame; }
    else if (!frame.empty()) { throw std::runtime_error("Unknown object layer frame: " + frame); }
    if (source.contains("states")) { layer.states = source.at("states").get<std::vector<int>>(); }
    return layer;
}

ProjectileVisual readProjectileVisual(const Json& source) {
    ProjectileVisual visual;
    visual.part = source.at("part");
    visual.lifeStep = source.value("life_step", 0);
    visual.variantStep = source.value("variant_step", 0);
    const std::string facing = source.at("facing");
    if (facing == "velocity") { visual.facing = ProjectileVisual::Facing::Velocity; }
    else if (facing == "right") { visual.facing = ProjectileVisual::Facing::Right; }
    else if (facing == "impact") { visual.facing = ProjectileVisual::Facing::Impact; }
    else { throw std::runtime_error("Unknown projectile facing: " + facing); }
    return visual;
}
}

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

const std::string& Localization::label(const std::string& key) const {
    const auto found = labels.find(key);
    if (found == labels.end()) { throw std::runtime_error("Missing interface label: " + key); }
    return found->second;
}

const std::vector<std::string>& Localization::message(const std::string& key) const {
    const auto found = messages.find(key);
    if (found == messages.end()) { throw std::runtime_error("Missing interface message: " + key); }
    return found->second;
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

const Animation& Assets::animation(const std::string& name) const {
    const auto found = animations.find(name);
    if (found == animations.end()) { throw std::runtime_error("Missing animation: " + name); }
    return found->second;
}

void Assets::load() {
    const auto catalog = readJson("catalog.json");
    if (catalog.at("format_version") != CatalogFormatVersion) { throw std::runtime_error("Unsupported asset catalog."); }
    // The logical canvas is fixed in code; the display record must describe the same canvas.
    const auto display = readJson(catalog.at("display"));
    if (display.at("logical_width") != ScreenWidth || display.at("logical_height") != ScreenHeight) {
        throw std::runtime_error("Display record does not match the 240x320 canvas.");
    }
    loadGraphics(catalog);
    loadDefinitions(catalog);
    loadVisuals(catalog);
    loadText(catalog);
    levelPaths = catalog.at("levels").get<std::vector<std::string>>();
    textures = catalog.at("textures").get<std::vector<std::string>>();
    const auto soundData = readJson(catalog.at("audio"));
    for (const auto& sound : soundData.at("sounds")) {
        slot(midiPaths, sound.at("id")) = sound.at("midi_source").get<std::string>();
    }
    soundfontPath = soundData.at("soundfont").get<std::string>();
    std::cout << "Assets loaded: " << levelPaths.size() << " levels, " << parts.size() << " parts, "
              << midiPaths.size() << " MIDI files.\n";
}

void Assets::loadGraphics(const Json& catalog) {
    const auto& definitions = catalog.at("definitions");
    const auto partData = readJson(definitions.at("character_parts"));
    for (const auto& part : partData["parts"]) {
        auto& directions = slot(parts, part.at("id"));
        const auto& source = part.at("directions");
        if (source.contains("right")) { directions[0] = readSprite(source["right"]); }
        if (source.contains("left")) { directions[1] = readSprite(source["left"]); }
    }
    const auto frameData = readJson(definitions.at("character_frames"));
    for (const auto& frame : frameData["frames"]) {
        slot(frames, frame.at("id")) = frame.at("part_slots").get<std::array<int, 8>>();
    }
    const auto tileData = readJson(definitions.at("world_tiles"));
    for (const auto& theme : tileData["themes"]) {
        auto& themeTiles = slot(tiles, theme.at("id"));
        for (const auto& entry : theme.at("tiles")) {
            Tile& tile = slot(themeTiles, entry.at("id"));
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
        Glyph& glyph = slot(glyphs, entry.at("id"));
        glyph.image = readSprite(image);
        glyph.image.offsetX = entry.at("offset_x");
        glyph.image.offsetY = -entry.at("baseline_offset_y").get<int>();
        glyph.advance = entry.at("advance");
    }
    const auto interfaceData = readJson(definitions.at("interface"));
    for (const auto& image : interfaceData.at("images").items()) {
        interfaceImages[image.key()] = readSprite(image.value());
    }
    for (const auto& icon : interfaceData.at("help_icons")) {
        slot(helpIcons, icon.at("id")) = readSprite(icon.at("image"));
    }
}

void Assets::loadDefinitions(const Json& catalog) {
    const auto& definitions = catalog.at("definitions");
    const auto entityData = readJson(definitions.at("entity_parameters"));
    for (const auto& entry : entityData["records"]) {
        auto& p = slot(entities, entry.at("id"));
        const auto& values = entry.at("parameters");
        if (values.empty()) { continue; }
        p = {values[0], values[1], values[2], values[3], values[4], values[5],
             values[6], values[7], values[8], values[9], values[10], values[11]};
    }
    const auto projectileData = readJson(definitions.at("projectile_parameters"));
    for (const auto& entry : projectileData["records"]) {
        const auto& values = entry.at("parameters");
        slot(projectiles, entry.at("id")) = {values[0], values[1], values[2], values[3], values[4], values[5], values[6]};
    }
    const auto animationData = readJson(definitions.at("character_animations"));
    for (const auto& entry : animationData.at("animations")) {
        Animation animation;
        animation.first = entry.at("range").at(0);
        animation.last = entry.at("range").at(1);
        if (entry.contains("fire_frames")) { animation.fireFrames = entry.at("fire_frames").get<std::vector<int>>(); }
        animations[entry.at("name").get<std::string>()] = animation;
    }
    const auto poses = readJson(definitions.at("player_poses"));
    for (const auto& entry : poses.at("weapon_frames")) {
        weaponFrames.push_back({entry.at("weapon_id"), entry.at("frame_id"), entry.at("part_slots").get<std::array<int, 8>>()});
    }
    const auto& crouchData = poses.at("crouch");
    crouch.frames = crouchData.at("frame_ids").get<std::vector<int>>();
    crouch.slot = crouchData.at("slot");
    crouch.part = crouchData.at("crouching_part");
    crouch.height = -crouchData.at("height_delta").get<int>();
    const auto& armorData = poses.at("armor");
    armor.firstFrame = armorData.at("frame_range").at(0);
    armor.lastFrame = armorData.at("frame_range").at(1);
    armor.torsoPart = armorData.at("find_part");
    armor.searchSlots = armorData.at("search_slots").get<std::vector<int>>();
    for (const auto& part : armorData.at("armor_parts").items()) {
        armor.parts[std::stoi(part.key())] = part.value().get<int>();
    }
}

void Assets::loadVisuals(const Json& catalog) {
    const auto& definitions = catalog.at("definitions");
    const auto effectData = readJson(definitions.at("visual_effects"));
    const auto& shadowColor = effectData.at("shadows").at("color_rgb");
    effects.shadowColor = packRgb(shadowColor.at(0), shadowColor.at(1), shadowColor.at(2));
    const auto& generator = effectData.at("generator");
    effects.generatorIdleColors = readHexColors(generator.at("light_colors_rgb_hex"));
    effects.generatorPoweredColors = readHexColors(generator.at("active_light_colors_rgb_hex"));
    effects.sparkColors = readHexColors(generator.at("spark_colors_rgb_hex"));
    effects.sparkRandomBits = generator.at("spark_random_bits");
    effects.shakeDamping = effectData.at("screen_shake").at("damping_per_tick");

    const auto visualData = readJson(definitions.at("object_visuals"));
    for (const auto& entry : visualData.at("objects")) {
        ObjectVisual visual;
        for (const auto& layer : entry.at("layers")) { visual.layers.push_back(readLayer(layer)); }
        if (entry.contains("shadow")) {
            const auto& shadow = entry.at("shadow");
            visual.hasShadow = true;
            visual.shadow = {shadow.at("x"), shadow.at("y"), shadow.at("width"), shadow.at("anchor") == "baseline"};
        }
        objectVisuals[entry.at("type").get<int>()] = visual;
    }
    const auto& terminal = visualData.at("terminal");
    terminalVisual.doorTiles = terminal.at("door_tiles").get<std::vector<int>>();
    terminalVisual.doorX = terminal.at("door_x");
    terminalVisual.doorlessVariant = terminal.at("doorless_variant");
    terminalVisual.groupTileBase = terminal.at("group_tile_base");
    terminalVisual.groupX = terminal.at("group_x");
    const auto& clip = terminal.at("clip");
    terminalVisual.clipX = clip.at(0); terminalVisual.clipY = clip.at(1);
    terminalVisual.clipWidth = clip.at(2); terminalVisual.clipHeight = clip.at(3);
    for (const auto& entry : visualData.at("projectiles")) {
        slot(projectileVisuals, entry.at("type")) = readProjectileVisual(entry);
    }
    const auto& hudData = visualData.at("hud");
    hud = {hudData.at("health_panel_tile"), hudData.at("weapon_icon_tile_base"), hudData.at("explosive_tile"),
           hudData.at("score_part"), hudData.at("ammo_part_base")};
    weaponActorType = visualData.at("actors").at("weapon_actor_type");
    weaponPartBase = visualData.at("actors").at("weapon_part_base");
}

void Assets::loadText(const Json& catalog) {
    const auto& language = catalog.at("language");
    const auto interfaceText = readJson(language.at("interface"));
    text.labels = interfaceText.at("labels").get<std::map<std::string, std::string>>();
    text.messages = interfaceText.at("messages").get<std::map<std::string, std::vector<std::string>>>();
    helpText = readText(language.at("help"));
    aboutText = readText(language.at("about"));
}

Level Assets::loadLevel(int number) const {
    if (number < 1 || number > levelCount()) { throw std::runtime_error("Unknown level: " + std::to_string(number)); }
    const auto source = readJson(levelPaths[static_cast<std::size_t>(number - 1)]);
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
