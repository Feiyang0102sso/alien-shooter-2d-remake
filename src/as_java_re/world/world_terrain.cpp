/** @file world_terrain.cpp
 * @brief Decorate terrain, patch damaged walls and floors, and switch lighting tiles.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>

namespace alien {
namespace {
bool isDecorationFloor(int tile) {
    return tile >= tile_id::Floor && tile <= tile_id::FloorHoleRight;
}
bool isWallHole(int tile) {
    return tile >= tile_id::WallHoleTop && tile <= tile_id::WallHoleLeft;
}
}
int World::findDecorationFloor(int x) {
    int row = rules::DecorationMargin + randomBits(31) % (level.height - 2 * rules::DecorationMargin);
    while (row > 0) {
        bool floor = true;
        for (int column = x - 1; column <= x + 1; ++column) {
            const int tile = tileAt(column, row);
            if (!isDecorationFloor(tile)) { floor = false; }
        }
        if (floor) { break; }
        --row;
    }
    return row;
}
int World::compositeTile(int base, int overlay) {
    for (int id = tile_id::CompositeFirst; id < tile_id::IdLimit; ++id) {
        if (compositeTiles[id][0] == base && compositeTiles[id][1] == overlay) { return id; }
    }
    // Wall composites take slots from the top; floor composites grow from the bottom.
    int id = nextWallTile_--;
    if (base >= tile_id::Floor && base <= tile_id::FloorLast) { ++nextWallTile_; id = nextFloorTile_++; }
    compositeTiles.at(id) = {base, overlay};
    return id;
}
void World::decorateCell(int x, int y, int overlay) {
    const int tile = tileAt(x, y);
    if (collisionAt(x, y) > 0) {
        if (tile == tile_id::Floor || (tile >= tile_id::FloorHoleLeft && tile <= tile_id::FloorHoleRight)) { setTile(x, y, compositeTile(tile, overlay)); }
    } else if (tile == tile_id::Wall || tile == tile_id::MarkedWallA || tile == tile_id::MarkedWallB || isWallHole(tile)) {
        setTile(x, y, compositeTile(tile, overlay));
    }
}
void World::decorateCorpse(int type, int x, int y, int direction) {
    if (x <= 0 || y < 0 || x > level.width - 2 || y > level.height - 3) { return; }
    // Each randomBits draw below is part of the original stream; keep the order and thresholds.
    if (type == 1) {
        decorateCell(x, y, tile_id::BloodSmear);
        int overlay = tile_id::DebrisC;
        if (randomBits(2) < 2) { overlay = tile_id::BloodSplash; }
        decorateCell(x, y + 1, overlay);
        return;
    }
    int first = tile_id::DebrisC, second = tile_id::DebrisC, lower = tile_id::BloodPool;
    if (randomBits(2) < 2) { first = tile_id::BloodSplash; }
    if (randomBits(2) < 2) { second = tile_id::BloodSplash; }
    if (randomBits(2) < 3) { lower = tile_id::BloodSmear; }
    if (type != 2 && type != 4) {
        decorateCell(x, y, tile_id::DebrisA); decorateCell(x + direction, y, tile_id::BloodSmear);
        if (randomBits(2) < 2) { decorateCell(x, y + 1, tile_id::DebrisB); }
    } else { decorateCell(x, y, tile_id::BloodPool); }
    decorateCell(x - direction, y, lower);
    decorateCell(x, y + 1 + randomBits(1), first);
    decorateCell(x - direction, y + 1 + randomBits(1), second);
}
void World::patchWall(int x, int y) {
    if (x < 1 || y < 1 || x > level.width - 2 || y > level.height - 2) { return; }
    const int center = tileAt(x, y);
    if (center == tile_id::Wall || isWallHole(center) || (center >= tile_id::LitWallAboveFloor && center <= tile_id::WallVariantLast)
        || center >= nextWallTile_) {
        setTile(x, y, tile_id::WallHole);
    }
    const std::array<std::array<int, 3>, 4> edges{{{-1, 0, tile_id::WallHoleLeft}, {1, 0, tile_id::WallHoleRight},
                                                   {0, 1, tile_id::WallHoleTop}, {0, -1, tile_id::WallHoleBottom}}};
    for (const auto& edge : edges) {
        const int tile = tileAt(x + edge[0], y + edge[1]);
        if (tile == tile_id::Wall || (tile >= nextWallTile_ && tile < tile_id::IdLimit)) { setTile(x + edge[0], y + edge[1], edge[2]); }
    }
}
void World::patchFloor(int x, int y) {
    if (x < 1 || y < 1 || x > level.width - 2 || y > level.height - 2) { return; }
    for (int offset = -1; offset <= 1; ++offset) {
        const int tile = tileAt(x + offset, y);
        if ((tile >= tile_id::Floor && tile <= tile_id::LitFloorRight) || (tile >= tile_id::CompositeFirst && tile <= nextFloorTile_)) {
            setTile(x + offset, y, tile_id::FloorHoleCenter + offset);
        }
    }
}
void World::decorateLevel() {
    const int amount = (level.id - 1) * rules::CorpsesPerLevel;
    const int span = level.width - 2 * rules::DecorationMargin;
    for (int item = 0; item < amount; ++item) {
        const int x = rules::DecorationMargin + randomBits(31) % span;
        const int y = findDecorationFloor(x);
        if (y > 0) {
            const int type = 1 + randomBits(31) % rules::MaxEnemyType;
            const int direction = randomBits(1) * 2 - 1;
            decorateCorpse(type, x, y, direction);
        }
    }
    for (int item = 0; item < amount; ++item) {
        const int x = rules::DecorationMargin + randomBits(31) % span;
        const int y = findDecorationFloor(x);
        if (y > 0 && randomBits(2) < 4) {
            if (tileAt(x, y + 1) == tile_id::Wall && tileAt(x, y + 2) == tile_id::Wall
                && tileAt(x - 1, y + 1) == tile_id::Wall && tileAt(x + 1, y + 1) == tile_id::Wall) { patchWall(x, y + 1); }
            else if (tileAt(x, y) == tile_id::Floor && tileAt(x - 1, y) == tile_id::Floor && tileAt(x + 1, y) == tile_id::Floor) { patchFloor(x, y); }
        }
    }
    for (int item = 0; item < (level.id - 1) * rules::FlamesPerLevel; ++item) {
        const int x = rules::DecorationMargin + randomBits(31) % span;
        const int y = findDecorationFloor(x);
        if (y > 0) {
            Object flame;
            flame.type = object_type::FloorFlame; flame.x = x * TileSize; flame.y = y * TileSize; flame.baselineY = flame.y;
            flame.cellX = x; flame.cellY = y; flame.health = assets_.entities[object_type::FloorFlame].health;
            objects.push_back(flame);
        }
    }
}

void World::setLighting(bool enabled, bool flicker) {
    lightsOn = enabled;
    for (auto light = objects.rbegin(); light != objects.rend(); ++light) {
        if (light->type != object_type::CeilingLight && light->type != object_type::WallLight) { continue; }
        if (flicker && randomBits(4) >= rules::LightFlickerKeep) { continue; }
        const int x = light->cellX, y = light->cellY;
        int floor = light->lightFloor;
        light->state = 0;
        if (!enabled) {
            playerLight = 0;
            setTile(x, y, tile_id::UnlitLamp);
            if (light->type == object_type::WallLight && tileAt(x, y - 1) == tile_id::WallLampGlow) { setTile(x, y - 1, tile_id::Wall); }
            if (floor != y) {
                if (tileAt(x, floor) == tile_id::LitFloorCenter) { setTile(x, floor, tile_id::Floor); }
                if (tileAt(x - 1, floor) == tile_id::LitFloorLeft) { setTile(x - 1, floor, tile_id::Floor); }
                if (tileAt(x + 1, floor) == tile_id::LitFloorRight) { setTile(x + 1, floor, tile_id::Floor); }
                if (tileAt(x, floor + 1) == tile_id::LitWallAboveFloor) { setTile(x, floor + 1, tile_id::Wall); }
            }
            continue;
        }
        light->state = 1;
        if (light->type == object_type::CeilingLight) {
            floor = 0;
            if (collisionAt(x, y - rules::CeilingLightReach) > 0) { floor = y - rules::CeilingLightReach; }
        } else {
            floor = y - 1;
            while (floor > 0 && collisionAt(x, floor) == 0) { --floor; }
        }
        if (floor <= 0) { light->lightFloor = y; continue; }
        light->lightFloor = floor;
        if (light->type == object_type::WallLight) {
            if (tileAt(x, y) == tile_id::UnlitLamp) { setTile(x, y, tile_id::WallLampLit); }
            if (tileAt(x, y - 1) == tile_id::Wall) { setTile(x, y - 1, tile_id::WallLampGlow); }
        } else if (tileAt(x, y) == tile_id::UnlitLamp) { setTile(x, y, tile_id::CeilingLampLit); }
        if (tileAt(x, floor) == tile_id::Floor && tileAt(x - 1, floor) == tile_id::Floor && tileAt(x + 1, floor) == tile_id::Floor) {
            setTile(x, floor, tile_id::LitFloorCenter); setTile(x - 1, floor, tile_id::LitFloorLeft); setTile(x + 1, floor, tile_id::LitFloorRight);
            if (tileAt(x, floor + 1) == tile_id::Wall) { setTile(x, floor + 1, tile_id::LitWallAboveFloor); }
        }
    }
}
}
