/** @file assets.h
 * @brief Load original level, sprite, font, animation, composition, text, and MIDI metadata.
 */
#pragma once

#include "core/constants.h"
#include <array>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace alien {
using Json = nlohmann::json;

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

// Inclusive original frame range; stepping skips empty frames inside it.
struct Animation {
    int first = 0, last = 0;
    std::vector<int> fireFrames;
};
// Replace the head part while crouching and lower the body.
struct CrouchPose {
    std::vector<int> frames;
    int slot = 0, part = 0, height = 0;
};
// Insert the armor part after the torso part.
struct ArmorPose {
    int firstFrame = 0, lastFrame = 0, torsoPart = 0;
    std::vector<int> searchSlots;
    std::map<int, int> parts;
};
struct WeaponFrame {
    int weapon = 0, frame = 0;
    std::array<int, 8> slots{};
};

// One sprite layer of a composed world object.
struct ObjectLayer {
    enum class Frame { None, Variant, VisualFrame };
    bool part = false;
    int id = 0, x = 0, y = 0, step = 1;
    bool baseline = false;
    Frame frame = Frame::None;
    std::vector<int> states;
};
struct ObjectShadow {
    int x = 0, y = 0, width = 0;
    bool baseline = true;
};
struct ObjectVisual {
    std::vector<ObjectLayer> layers;
    bool hasShadow = false;
    ObjectShadow shadow;
};
struct TerminalVisual {
    std::vector<int> doorTiles;
    int doorX = 0, doorlessVariant = 0, groupTileBase = 0, groupX = 0;
    int clipX = 0, clipY = 0, clipWidth = 0, clipHeight = 0;
};
struct ProjectileVisual {
    enum class Facing { Velocity, Right, Impact };
    int part = 0, lifeStep = 0, variantStep = 0;
    Facing facing = Facing::Velocity;
};
struct HudVisual {
    int healthPanelTile = 0, weaponIconTileBase = 0, explosiveTile = 0, scorePart = 0, ammoPartBase = 0;
};
struct VisualEffects {
    unsigned int shadowColor = 0;
    std::vector<unsigned int> generatorIdleColors, generatorPoweredColors, sparkColors;
    int sparkRandomBits = 0, shakeDamping = 0;
};

// English interface strings addressed by name.
class Localization {
public:
    const std::string& label(const std::string& key) const;
    const std::vector<std::string>& message(const std::string& key) const;
    std::map<std::string, std::string> labels;
    std::map<std::string, std::vector<std::string>> messages;
};

class Assets {
public:
    explicit Assets(std::filesystem::path root);
    void load();
    Level loadLevel(int number) const;
    int levelCount() const { return static_cast<int>(levelPaths.size()); }
    const Animation& animation(const std::string& name) const;
    bool hasParameters(int type) const { return type >= 0 && type < static_cast<int>(entities.size()); }
    Json readJson(const std::string& relative) const;
    std::string readText(const std::string& relative) const;
    const std::filesystem::path& root() const { return root_; }

    // Indexed by original ids; vector sizes follow the largest id in the data.
    std::vector<std::array<Sprite, 2>> parts;
    std::vector<std::array<int, 8>> frames;
    std::vector<std::vector<Tile>> tiles;
    std::vector<Glyph> glyphs;
    std::vector<EntityParameters> entities;
    std::vector<ProjectileParameters> projectiles;
    std::vector<std::string> midiPaths;
    std::vector<std::string> levelPaths;
    std::vector<std::string> textures;
    std::string soundfontPath;
    std::string helpText, aboutText;
    Localization text;
    std::map<std::string, Sprite> interfaceImages;
    std::vector<Sprite> helpIcons;
    std::map<std::string, Animation> animations;
    std::vector<WeaponFrame> weaponFrames;
    CrouchPose crouch;
    ArmorPose armor;
    VisualEffects effects;
    std::map<int, ObjectVisual> objectVisuals;
    TerminalVisual terminalVisual;
    std::vector<ProjectileVisual> projectileVisuals;
    HudVisual hud;
    int weaponActorType = 0, weaponPartBase = 0;
private:
    void loadGraphics(const Json& catalog);
    void loadDefinitions(const Json& catalog);
    void loadVisuals(const Json& catalog);
    void loadText(const Json& catalog);
    std::filesystem::path root_;
};
Sprite readSprite(const Json& source);
}
