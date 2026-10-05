/** @file world_level.cpp
 * @brief Load level records into objects, enemies, and collision grids.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>
#include <iostream>

namespace alien {
void World::load(int levelNumber, int difficulty, const Inventory* carry) {
    cheat.disable(*this);
    level = assets_.loadLevel(levelNumber);
    difficulty_ = difficulty;
    inventory = Inventory{};
    if (carry) { inventory = *carry; }
    inventory.explosives = 0;
    inventory.weapon = rules::Pistol;
    inventory.armor = 0; // e.java resets bk during level initialization.
    enemies.clear(); objects.clear(); projectiles.clear(); sounds.clear(); message.clear(); generators_.clear();
    timeMs = ticks = crouch = shake = healthFlash = airState = playerLight = 0;
    drawOffsetY = 0;
    flashOuterColor = flashInnerColor = -1; scoreFlash_ = rules::ScoreFlashTicks;
    displayedWeapon_ = weaponPanelY_ = weaponPanelSpeed_ = weaponPanelHold_ = ammoPanelY = 0;
    scoreVisible = explosiveVisible = explosiveBlink_ = false;
    lookingDown_ = false;
    cleared = complete = power = lightsOn = dead = false;
    deathWipeActive = deathWipeDone_ = false; deathColumns = {}; deathParticleSteps_ = 0;
    riding_ = -1; selectedTerminal_ = -1; bob_ = rules::RecoilBobFrames; cameraTransit_ = false;
    if (!continuousRandom_) { randomState_ = DefaultRandomSeed; }
    compositeTiles = {};
    nextFloorTile_ = tile_id::CompositeFirst; nextWallTile_ = tile_id::CompositeLast;
    player = Actor{};
    previousCameraX = NoPreviousPosition;
    jumpBuffer_ = 0;
    player.x = level.playerCellX * TileSize + assets_.entities[0].left;
    player.y = level.height * TileSize - level.playerCellY * TileSize;
    playerFootRowOverride_ = -1;
    player.frame = 0;
    int terminalGroup = 0;
    bool firstTerminal = true;
    int crateStack = 0;
    std::vector<int> breakableIndices;
    for (const auto& spawn : level.spawns) {
        const int x = spawn.cellX * TileSize;
        const int y = level.height * TileSize - spawn.cellY * TileSize - rules::ObjectSpawnLift;
        if (spawn.type < object_type::FirstObjectType) {
            spawnEnemy(spawn.type, x, y + rules::EnemySpawnLift, -1);
            continue;
        }
        Object object;
        object.id = spawn.id; object.type = spawn.type; object.x = x; object.y = y;
        object.cellX = x / TileSize; object.cellY = y / TileSize; object.baselineY = y;
        object.lightFloor = object.cellY;
        if (assets_.hasParameters(spawn.type)) { object.health = assets_.entities[spawn.type].health; }
        if (assets_.hasParameters(spawn.type) && assets_.entities[spawn.type].motion == 0) {
            object.motionFrame = (randomBits(31) % rules::PickupBobFrames) & -2;
        }
        // Collapse numbered records into one object type plus a variant.
        if (spawn.type >= object_type::WeaponPickup && spawn.type <= object_type::WeaponPickupLast) {
            object.type = object_type::WeaponPickup; object.variant = spawn.type - object_type::WeaponPickup + 1;
        }
        if (spawn.type >= object_type::AmmoPickup && spawn.type <= object_type::AmmoPickupLast) {
            object.type = object_type::AmmoPickup; object.variant = spawn.type - object_type::AmmoPickup + 1;
        }
        if (spawn.type >= object_type::ArmorPickup && spawn.type <= object_type::ArmorPickupLast) {
            object.type = object_type::ArmorPickup; object.variant = spawn.type - object_type::ArmorPickup + 1;
        }
        if (spawn.type >= object_type::Terminal && spawn.type <= object_type::TerminalLast) {
            object.type = object_type::Terminal; object.variant = spawn.type - object_type::Terminal;
            object.direction = 0;
            object.terminalGroup = terminalGroup; object.x += rules::TerminalOffsetX;
            if (firstTerminal) { ++terminalGroup; }
            firstTerminal = !firstTerminal;
            if (spawn.type == object_type::PoweredTerminalRecord) { setTile(x / TileSize + 2, y / TileSize + 2, tile_id::PoweredTerminalWall); }
            if (spawn.type == object_type::DamagedTerminalRecord) { patchWall(x / TileSize + 2, y / TileSize + 1); }
        }
        if (spawn.type == object_type::TeleporterSource || spawn.type == object_type::HiveSource) {
            GeneratorState generator;
            generator.limit = spawn.spawnLimit;
            generator.types = spawn.spawnTypes;
            generators_.push_back(generator);
        }
        if (spawn.type == object_type::UsedSwitch) {
            setTile(x / TileSize, y / TileSize, tile_id::UsedSocket);
            object.active = false;
        }
        initializeCollision(object);
        if (spawn.type == object_type::Crate) {
            object.stackCount = crateStack;
            int row = object.cellY;
            const int column = object.cellX;
            if (tileAt(column, row) >= tile_id::Floor && tileAt(column, row) <= nextFloorTile_) {
                object.x += randomBits(32) % rules::CrateJitterX;
                const int verticalOffset = randomBits(31) % rules::CrateJitterY;
                object.y += verticalOffset;
                setCollision(column, row, TileSize); setCollision(column, row + 1, 1 + verticalOffset);
                int previousY = object.y;
                int previousCellY = object.cellY;
                for (int stack = 0; stack < crateStack; ++stack) {
                    auto& upper = objects[breakableIndices[breakableIndices.size() - 1 - stack]];
                    upper.x += randomBits(32) % rules::CrateJitterX;
                    upper.y = previousY + rules::CrateStackGap + randomBits(1);
                    setCollision(column, row, TileSize); ++row;
                    previousY = upper.y; previousCellY = upper.cellY;
                }
                if (crateStack > 0) { setCollision(column, row, std::min(TileSize, previousY + TileSize - previousCellY * TileSize)); }
                crateStack = 0;
            } else { ++crateStack; }
        }
        if (spawn.type >= object_type::BreakableFirst && spawn.type <= object_type::BreakableLast) { breakableIndices.push_back(static_cast<int>(objects.size())); }
        objects.push_back(object);
    }
    std::reverse(generators_.begin(), generators_.end());
    // Initial enemies precede shared difficulty updates; generators and later loads use the new health.
    for (int type = 1; type <= rules::MaxEnemyType; ++type) {
        const int base = assets_.entities[type].health;
        enemyHealth_[type] = base + base * ((difficulty_ - 1) * rules::DifficultyHealthPercent) / 100;
    }
    // The original scans downward once after loading to place the player on the nearest support.
    player.velocityY = -level.height * TileSize;
    gravity(player);
    player.health = rules::MaxHealth;
    dead = false;
    sounds.clear(); beginHealthFlash(true);
    cameraX = std::clamp(player.x - rules::CameraLead, 0, std::max(0, level.width * TileSize - ScreenWidth));
    // The original positions the camera before initial landing and follows the landed position next tick.
    cameraBottom = std::clamp(level.height * TileSize - level.playerCellY * TileSize - rules::CameraLead, 0,
                              std::max(0, level.height * TileSize - ScreenHeight));
    decorateLevel();
    updateVisuals();
    updateActorHeights();
    std::cout << "Level " << levelNumber << " loaded: " << level.width << "x" << level.height
              << ", objects=" << objects.size() << ", enemies=" << enemies.size() << "\n";
}

int World::collisionAt(int x, int y) const {
    if (x < 0 || x >= level.width || y < 0) { return TileSize; }
    if (y >= level.height) { return 0; }
    return level.collision[y][x];
}
int World::tileAt(int x, int y) const {
    if (x < 0 || x >= level.width || y < 0 || y >= level.height) { return tile_id::Empty; }
    return level.tiles[y][x];
}
void World::setCollision(int x, int y, int height) {
    if (x >= 0 && x < level.width && y >= 0 && y < level.height) { level.collision[y][x] = height; }
}
void World::setTile(int x, int y, int tile) {
    if (x >= 0 && x < level.width && y >= 0 && y < level.height) { level.tiles[y][x] = tile; }
}
void World::initializeCollision(const Object& object) {
    const int x = object.cellX, y = object.cellY;
    switch (object.type) {
    case object_type::GasCylinder: case object_type::Barrel: case object_type::CrushedBarrel: case object_type::ToxicTank:
        setCollision(x, y + 1, TileSize); setCollision(x, y + 2, rules::TallBreakableCapHeight); break;
    case object_type::Drum:
        setCollision(x + 1, y + 1, rules::LowBreakableHeight);
        [[fallthrough]];
    case object_type::ToxicTankLow: setCollision(x, y + 1, rules::LowBreakableHeight); break;
    }
}
}
