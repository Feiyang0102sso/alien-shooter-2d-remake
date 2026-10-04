/** @file world.cpp
 * @brief Reproduce original discrete movement, combat, enemies, and mechanisms.
 */
#include "world.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace alien {
namespace {
bool intersects(int left, int right, int bottom, int top, int otherLeft, int otherRight, int otherBottom, int otherTop) {
    // Original collision tests endpoints; crossing thin rectangles can miss when all endpoints lie outside.
    const bool ownXInside = (left >= otherLeft && left <= otherRight) || (right >= otherLeft && right <= otherRight);
    if (ownXInside && ((top >= otherBottom && top <= otherTop) || (bottom >= otherBottom && bottom <= otherTop))) { return true; }
    const bool otherXInside = (otherLeft >= left && otherLeft <= right) || (otherRight >= left && otherRight <= right);
    return otherXInside && ((otherTop >= bottom && otherTop <= top) || (otherBottom >= bottom && otherBottom <= top));
}
int toward(int current, int target, int amount) {
    return current + std::clamp(target - current, -amount, amount);
}
}
World::World(const Assets& assets) : assets_(assets) {
    for (int type = 0; type <= 5; ++type) { collisionHeights_[type] = assets_.entities[type].top; }
    for (int type = 1; type <= 5; ++type) { enemyHealth_[type] = assets_.entities[type].health; }
}
void World::seedRandom(std::uint64_t seed) {
    randomState_ = seed; continuousRandom_ = true;
}

void World::load(int levelNumber, int difficulty, const Inventory* carry) {
    cheat.disable(*this);
    level = assets_.loadLevel(levelNumber);
    difficulty_ = difficulty;
    inventory = Inventory{};
    if (carry) { inventory = *carry; }
    inventory.explosives = 0;
    inventory.weapon = 0;
    inventory.armor = 0; // e.java resets bk during level initialization.
    enemies.clear(); objects.clear(); projectiles.clear(); sounds.clear(); message.clear(); generators_.clear();
    timeMs = ticks = crouch = shake = healthFlash = airState = playerLight = 0;
    drawOffsetY = 0;
    flashOuterColor = flashInnerColor = -1; scoreFlash_ = 7;
    displayedWeapon_ = weaponPanelY_ = weaponPanelSpeed_ = weaponPanelHold_ = ammoPanelY = 0;
    scoreVisible = explosiveVisible = explosiveBlink_ = false;
    lookingDown_ = false;
    cleared = complete = power = lightsOn = dead = false;
    deathWipeActive = deathWipeDone_ = false; deathColumns = {}; deathParticleSteps_ = 0;
    riding_ = -1; selectedTerminal_ = -1; bob_ = 2; cameraTransit_ = false;
    if (!continuousRandom_) { randomState_ = 12345; }
    compositeTiles = {};
    nextFloorTile_ = 170; nextWallTile_ = 219;
    player = Actor{};
    player.x = level.playerCellX * 16 + assets_.entities[0].left;
    player.y = level.height * 16 - level.playerCellY * 16;
    playerFootRowOverride_ = -1;
    player.frame = 0;
    int terminalGroup = 0;
    bool firstTerminal = true;
    int crateStack = 0;
    std::vector<int> breakableIndices;
    for (const auto& spawn : level.spawns) {
        const int x = spawn.cellX * 16;
        const int y = level.height * 16 - spawn.cellY * 16 - 15;
        if (spawn.type < 10) {
            spawnEnemy(spawn.type, x, y + 8, -1);
            continue;
        }
        Object object;
        object.id = spawn.id; object.type = spawn.type; object.x = x; object.y = y;
        object.cellX = x / 16; object.cellY = y / 16; object.baselineY = y;
        object.lightFloor = object.cellY;
        if (spawn.type < 44) { object.health = assets_.entities[spawn.type].health; }
        if (spawn.type < 44 && assets_.entities[spawn.type].motion == 0) { object.motionFrame = (randomBits(31) % 16) & -2; }
        if (spawn.type >= 10 && spawn.type <= 13) { object.type = 10; object.variant = spawn.type - 9; }
        if (spawn.type >= 14 && spawn.type <= 17) { object.type = 14; object.variant = spawn.type - 13; }
        if (spawn.type == 21 || spawn.type == 22) { object.type = 21; object.variant = spawn.type - 20; }
        if (spawn.type >= 41 && spawn.type <= 43) {
            object.type = 41; object.variant = spawn.type - 41;
            object.direction = 0;
            object.terminalGroup = terminalGroup; object.x += 6;
            if (firstTerminal) { ++terminalGroup; }
            firstTerminal = !firstTerminal;
            if (spawn.type == 42) { setTile(x / 16 + 2, y / 16 + 2, 121); }
            if (spawn.type == 43) { patchWall(x / 16 + 2, y / 16 + 1); }
        }
        if (spawn.type == 36 || spawn.type == 37) {
            GeneratorState generator;
            generator.limit = spawn.spawnLimit;
            generator.types = spawn.spawnTypes;
            generators_.push_back(generator);
        }
        if (spawn.type == 39) {
            setTile(x / 16, y / 16, 137);
            object.active = false;
        }
        initializeCollision(object);
        if (spawn.type == 32) {
            object.stackCount = crateStack;
            int row = object.cellY;
            const int column = object.cellX;
            if (tileAt(column, row) >= 145 && tileAt(column, row) <= nextFloorTile_) {
                object.x += randomBits(32) % 3;
                const int verticalOffset = randomBits(31) % 4;
                object.y += verticalOffset;
                setCollision(column, row, 16); setCollision(column, row + 1, 1 + verticalOffset);
                int previousY = object.y;
                int previousCellY = object.cellY;
                for (int stack = 0; stack < crateStack; ++stack) {
                    auto& upper = objects[breakableIndices[breakableIndices.size() - 1 - stack]];
                    upper.x += randomBits(32) % 3;
                    upper.y = previousY + 12 + randomBits(1);
                    setCollision(column, row, 16); ++row;
                    previousY = upper.y; previousCellY = upper.cellY;
                }
                if (crateStack > 0) { setCollision(column, row, std::min(16, previousY + 16 - previousCellY * 16)); }
                crateStack = 0;
            } else { ++crateStack; }
        }
        if (spawn.type >= 26 && spawn.type <= 32) { breakableIndices.push_back(static_cast<int>(objects.size())); }
        objects.push_back(object);
    }
    std::reverse(generators_.begin(), generators_.end());
    // Initial enemies precede shared difficulty updates; generators and later loads use the new health.
    for (int type = 1; type <= 5; ++type) {
        const int base = assets_.entities[type].health;
        enemyHealth_[type] = base + base * ((difficulty_ - 1) * 20) / 100;
    }
    // The original scans downward once after loading to place the player on the nearest support.
    player.velocityY = -level.height * 16;
    gravity(player);
    player.health = 100;
    dead = false;
    sounds.clear(); beginHealthFlash(true);
    cameraX = std::clamp(player.x - 32, 0, std::max(0, level.width * 16 - ScreenWidth));
    // The original positions the camera before initial landing and follows the landed position next tick.
    cameraBottom = std::clamp(level.height * 16 - level.playerCellY * 16 - 32, 0, std::max(0, level.height * 16 - ScreenHeight));
    decorateLevel();
    updateVisuals();
    updateActorHeights();
    std::cout << "Level " << levelNumber << " loaded: " << level.width << "x" << level.height
              << ", objects=" << objects.size() << ", enemies=" << enemies.size() << "\n";
}

int World::collisionAt(int x, int y) const {
    if (x < 0 || x >= level.width || y < 0) { return 16; }
    if (y >= level.height) { return 0; }
    return level.collision[y][x];
}
int World::tileAt(int x, int y) const {
    if (x < 0 || x >= level.width || y < 0 || y >= level.height) { return 0; }
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
    case 26: case 27: case 28: case 30:
        setCollision(x, y + 1, 16); setCollision(x, y + 2, 4); break;
    case 29:
        setCollision(x + 1, y + 1, 12);
        [[fallthrough]];
    case 31: setCollision(x, y + 1, 12); break;
    }
}

int World::randomBits(int bits) {
    randomState_ = (randomState_ * 25214903917ULL + 11) & 281474976710655ULL;
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(randomState_ >> (48 - bits)));
}
int World::findDecorationFloor(int x) {
    int row = 2 + randomBits(31) % (level.height - 4);
    while (row > 0) {
        bool floor = true;
        for (int column = x - 1; column <= x + 1; ++column) {
            const int tile = tileAt(column, row);
            if (tile < 145 || tile > 151) { floor = false; }
        }
        if (floor) { break; }
        --row;
    }
    return row;
}
int World::compositeTile(int base, int overlay) {
    for (int id = 170; id < 220; ++id) {
        if (compositeTiles[id][0] == base && compositeTiles[id][1] == overlay) { return id; }
    }
    int id = nextWallTile_--;
    if (base >= 145 && base <= 169) { ++nextWallTile_; id = nextFloorTile_++; }
    compositeTiles.at(id) = {base, overlay};
    return id;
}
void World::decorateCell(int x, int y, int overlay) {
    const int tile = tileAt(x, y);
    if (collisionAt(x, y) > 0) {
        if (tile == 145 || (tile >= 149 && tile <= 151)) { setTile(x, y, compositeTile(tile, overlay)); }
    } else if (tile == 105 || tile == 122 || tile == 126 || (tile >= 112 && tile <= 116)) {
        setTile(x, y, compositeTile(tile, overlay));
    }
}
void World::decorateCorpse(int type, int x, int y, int direction) {
    if (x <= 0 || y < 0 || x > level.width - 2 || y > level.height - 3) { return; }
    if (type == 1) {
        decorateCell(x, y, 76);
        int overlay = 80;
        if (randomBits(2) < 2) { overlay = 52; }
        decorateCell(x, y + 1, overlay);
        return;
    }
    int first = 80, second = 80, lower = 77;
    if (randomBits(2) < 2) { first = 52; }
    if (randomBits(2) < 2) { second = 52; }
    if (randomBits(2) < 3) { lower = 76; }
    if (type != 2 && type != 4) {
        decorateCell(x, y, 78); decorateCell(x + direction, y, 76);
        if (randomBits(2) < 2) { decorateCell(x, y + 1, 79); }
    } else { decorateCell(x, y, 77); }
    decorateCell(x - direction, y, lower);
    decorateCell(x, y + 1 + randomBits(1), first);
    decorateCell(x - direction, y + 1 + randomBits(1), second);
}
void World::patchWall(int x, int y) {
    if (x < 1 || y < 1 || x > level.width - 2 || y > level.height - 2) { return; }
    const int center = tileAt(x, y);
    if (center == 105 || (center >= 112 && center <= 116) || (center >= 120 && center <= 124) || center >= nextWallTile_) { setTile(x, y, 113); }
    const std::array<std::array<int, 3>, 4> edges{{{-1, 0, 116}, {1, 0, 115}, {0, 1, 112}, {0, -1, 114}}};
    for (const auto& edge : edges) {
        const int tile = tileAt(x + edge[0], y + edge[1]);
        if (tile == 105 || (tile >= nextWallTile_ && tile < 220)) { setTile(x + edge[0], y + edge[1], edge[2]); }
    }
}
void World::patchFloor(int x, int y) {
    if (x < 1 || y < 1 || x > level.width - 2 || y > level.height - 2) { return; }
    for (int offset = -1; offset <= 1; ++offset) {
        const int tile = tileAt(x + offset, y);
        if ((tile >= 145 && tile <= 148) || (tile >= 170 && tile <= nextFloorTile_)) { setTile(x + offset, y, 150 + offset); }
    }
}
void World::decorateLevel() {
    const int amount = (level.id - 1) * 5;
    for (int item = 0; item < amount; ++item) {
        const int x = 2 + randomBits(31) % (level.width - 4);
        const int y = findDecorationFloor(x);
        if (y > 0) {
            const int type = 1 + randomBits(31) % 5;
            const int direction = randomBits(1) * 2 - 1;
            decorateCorpse(type, x, y, direction);
        }
    }
    for (int item = 0; item < amount; ++item) {
        const int x = 2 + randomBits(31) % (level.width - 4);
        const int y = findDecorationFloor(x);
        if (y > 0 && randomBits(2) < 4) {
            if (tileAt(x, y + 1) == 105 && tileAt(x, y + 2) == 105 && tileAt(x - 1, y + 1) == 105 && tileAt(x + 1, y + 1) == 105) { patchWall(x, y + 1); }
            else if (tileAt(x, y) == 145 && tileAt(x - 1, y) == 145 && tileAt(x + 1, y) == 145) { patchFloor(x, y); }
        }
    }
    for (int item = 0; item < (level.id - 1) * 3; ++item) {
        const int x = 2 + randomBits(31) % (level.width - 4);
        const int y = findDecorationFloor(x);
        if (y > 0) {
            Object flame;
            flame.type = 40; flame.x = x * 16; flame.y = y * 16; flame.baselineY = flame.y;
            flame.cellX = x; flame.cellY = y; flame.health = assets_.entities[40].health;
            objects.push_back(flame);
        }
    }
}

int World::actorHeight(const Actor& actor) const {
    return collisionHeights_[actor.type];
}
int World::frameHeight(const Actor& actor) const {
    int height = 0;
    for (int part : assets_.frames.at(actor.frame)) {
        if (part > 0) { height = std::max(height, -assets_.parts[part][0].offsetY); }
    }
    if (actor.type == 0) { height -= crouch; }
    return std::max(1, height);
}
void World::updateActorHeights() {
    if (player.alive) { collisionHeights_[0] = frameHeight(player); }
    // Original paint traverses visible enemies in reverse slot order and writes shared type heights.
    // Preserve that order so the desktop refresh rate cannot change the next tick's hit results.
    for (auto enemy = enemies.rbegin(); enemy != enemies.rend(); ++enemy) {
        if (!enemy->alive) { continue; }
        if (enemy->direction == 0 && (enemy->behavior & 1) == 0) { continue; }
        const auto& bounds = assets_.entities[enemy->type];
        if (enemy->x + bounds.right < cameraX - 16 || enemy->x - bounds.left > cameraX + ScreenWidth + 16) { continue; }
        if (enemy->y + collisionHeights_[enemy->type] < cameraBottom || enemy->y - bounds.bottom > cameraBottom + ScreenHeight) { continue; }
        collisionHeights_[enemy->type] = frameHeight(*enemy);
    }
}
void World::startAnimation(Actor& actor, int state) {
    actor.state = state;
    if (actor.type == 0) {
        const int pose = std::min(inventory.weapon, 2);
        if (state == 0) { actor.frame = pose; }
        if (state == 1 || state == 3) { actor.frame = pose * 4 + 3; }
        if (state == 2) { actor.frame = inventory.weapon * 4 + 15; }
    } else {
        actor.frame = 26 + actor.type * 10;
        if (state == 2) { actor.frame += 5; }
    }
}
bool World::advanceAnimation(Actor& actor) {
    int start = 26 + actor.type * 10;
    int end = start + 4;
    if (actor.type == 0) {
        start = std::min(inventory.weapon, 2) * 4 + 3;
        end = start + 3;
        if (actor.state == 2) { start = inventory.weapon * 4 + 15; end = start + 3; }
    } else if (actor.state == 2) { start += 5; end = start + 3; }
    ++actor.frame;
    while (actor.frame <= end && assets_.frames.at(actor.frame)[0] == 0) { ++actor.frame; }
    if (actor.frame > end) {
        if (actor.state == 1) { actor.frame = start; return true; }
        actor.frame = end;
        return false;
    }
    return true;
}

void World::moveActor(Actor& actor, int direction, bool airborne) {
    if (dead || actor.y < 0) { return; }
    const auto& p = assets_.entities[actor.type];
    if (actor.direction != direction) { actor.direction = direction; return; }
    if (actor.supportY >= 0) {
        if (actor.state != 1) { startAnimation(actor, 1); }
        else { advanceAnimation(actor); }
    }
    int speed = p.speedX;
    if (airborne) { speed /= 2; }
    if (direction < 0 && actor.x - speed - p.left < 0) {
        actor.x = p.left + 1;
        if (actor.type != 0) { actor.supportY = -1; }
        return;
    }
    if (direction > 0 && actor.x + speed + p.right - 1 >= level.width * 16) {
        actor.x = level.width * 16 - p.right - 1;
        if (actor.type != 0) { actor.supportY = -1; }
        return;
    }
    const int candidateX = actor.x + speed * direction;
    int side = (candidateX + p.right) / 16;
    if (direction < 0) { side = (candidateX - p.left) / 16; }
    int feetCell = actor.y / 16;
    if (actor.type == 0 && playerFootRowOverride_ >= 0) { feetCell = playerFootRowOverride_; }
    const int topCell = (actor.y + actorHeight(actor)) / 16;
    for (int row = topCell; row > feetCell; --row) {
        if (collisionAt(side, row) > 0) {
            if (actor.type != 0) { actor.supportY = -1; }
            return;
        }
    }
    int supportCell = candidateX / 16;
    if (actor.type != 0) { supportCell = side; }
    const int height = collisionAt(supportCell, feetCell);
    const int surface = feetCell * 16 + height;
    if (height > 0 && actor.velocityY <= 0) {
        if (surface - actor.y > 2) {
            if (actor.type != 0) { actor.supportY = -1; }
            return;
        }
        actor.y = surface; actor.supportY = surface;
        if (actor.type == 0) {
            playerFootRowOverride_ = -1;
            if (airState != 0 && actor.state != 1) { startAnimation(actor, 0); }
            airState = 0;
        }
    } else {
        actor.supportY = -1;
        if (actor.type != 0) { return; }
        if (airState == 0 && riding_ < 0) { airState = 3; startAnimation(actor, 3); }
    }
    actor.x = candidateX;
}

void World::gravity(Actor& actor) {
    if (actor.supportY >= 0) { return; }
    // Original g() enters gravity before airborne movement; new support does not skip that gravity step.
    if (actor.type == 0 && airState == 1) { moveActor(actor, actor.direction, true); }
    const int previousY = actor.y;
    int previousRow = previousY / 16;
    if (actor.type == 0 && playerFootRowOverride_ >= 0) { previousRow = playerFootRowOverride_; }
    actor.y += actor.velocityY;
    if (actor.type == 0) { playerFootRowOverride_ = -1; }
    actor.velocityY += assets_.entities[actor.type].gravity;
    const int column = actor.x / 16;
    if (actor.y > previousY) {
        const int top = (actor.y + actorHeight(actor)) / 16;
        if (collisionAt(column, top) > 0) {
            actor.y = top * 16 - actorHeight(actor) - 1; actor.velocityY = 0;
        }
        return;
    }
    for (int row = previousRow; row >= std::max(0, actor.y / 16); --row) {
        const int height = collisionAt(column, row);
        const int surface = row * 16 + height;
        if (height > 0 && surface > actor.y && surface <= previousY + 16) {
            actor.y = surface;
            actor.supportY = surface;
            // Broken crates clear support without necessarily entering an airborne state or interrupting fire.
            if (actor.type == 0 && airState > 0) {
                const auto& bounds = assets_.entities[0];
                const int left = (actor.x - bounds.left) / 16;
                const int right = (actor.x + bounds.right) / 16;
                const int above = actor.y / 16 + 1;
                if (collisionAt(left, above) > 0) { actor.x += bounds.left; }
                if (collisionAt(right, above) > 0) { actor.x -= bounds.right; }
                airState = 0;
                startAnimation(actor, 0);
                // Apply gravity, compute landing damage from the updated velocity, then clear velocity.
                applyFallDamage();
            } else if (actor.type != 0) {
                startAnimation(actor, 1);
            }
            actor.velocityY = 0;
            return;
        }
    }
    if (actor.y < 0 && actor.type == 0) { damagePlayer(100); }
}
void World::applyFallDamage() {
    if (player.velocityY < -24) { damagePlayer(100); }
    else if (player.velocityY < -20) { damagePlayer(5 - 10 * (player.velocityY + 20)); }
}

void World::selectWeapon(int weapon) {
    if (weapon < 0 || weapon >= 5 || !inventory.owned[weapon] || player.state == 2) { return; }
    inventory.weapon = weapon;
    displayedWeapon_ = -1;
    startAnimation(player, 0);
}
void World::cycleWeapon(int delta) {
    int weapon = inventory.weapon;
    for (int attempt = 0; attempt < 5; ++attempt) {
        weapon = (weapon + delta + 5) % 5;
        if (inventory.owned[weapon]) { selectWeapon(weapon); return; }
    }
}
void World::firePlayer() {
    --bob_;
    int height = 24 + bob_;
    int vertical = 0;
    const int weapon = inventory.weapon;
    const std::array<int, 5> sound{3, 4, 8, 5, 5};
    if (weapon == 0) { height = 35 + bob_ - crouch; vertical = -1; }
    if (weapon == 2) { height = 30 + bob_ * 2; vertical = -2; }
    sounds.push_back(sound[weapon]);
    addProjectile(weapon, player.x + player.direction * 12, player.y + height, player.direction, vertical, false);
    if (bob_ < 0) { bob_ = 2; }
    if (weapon > 0) {
        --inventory.ammo[weapon];
        if (inventory.ammo[weapon] <= 0) {
            inventory.ammo[weapon] = 0;
            int replacement = weapon - 1;
            while (replacement > 0 && (!inventory.owned[replacement] || inventory.ammo[replacement] <= 0)) { --replacement; }
            inventory.weapon = replacement;
        }
    }
}
void World::updatePlayer(const Input& input) {
    int feetRow = player.y / 16;
    if (playerFootRowOverride_ >= 0) { feetRow = playerFootRowOverride_; }
    if (inventory.armor != 2) {
        const int tile = tileAt(player.x / 16, feetRow);
        const int below = tileAt(player.x / 16, feetRow - 1);
        playerLight = 0;
        if (tile >= 146 && tile <= 148) { playerLight = tile - 134; }
        else if (below >= 146 && below <= 148) { playerLight = below - 134; }
        if (playerLight != 0 && player.direction < 0) { playerLight = 26 - playerLight; }
    }
    if (player.state == 1 && !input.left && !input.right && !input.fire && !input.down && !input.jump && airState == 0) {
        const int end = std::min(inventory.weapon, 2) * 4 + 6;
        if (player.frame < end) { player.frame = end; return; }
        startAnimation(player, 0);
    }
    if (player.state == 2) {
        const int previousFrame = player.frame;
        const bool continuing = advanceAnimation(player);
        // Finish the old animation before the last shot; depleted-ammo idle frames retain the old weapon.
        if (!continuing) { startAnimation(player, 0); }
        bool shoot = !continuing;
        if (inventory.weapon == 0) { shoot = previousFrame == 16 || previousFrame == 18; }
        if (shoot) { firePlayer(); }
    }
    if (riding_ < 0) {
        gravity(player);
    }
}
void World::applyPlayerInput(const Input& input) {
    if (input.weapon >= 0) { selectWeapon(input.weapon); }
    if (input.cycleWeapon != 0) { cycleWeapon(input.cycleWeapon); }
    if (input.jump) {
        if (dead) { return; }
        if (airState != 0 && airState != 2) { return; }
        if (crouch != 0) {
            if (player.state == 0) { crouch = 0; }
        }
        else if (player.supportY >= 0) {
            // Jump can cancel standing fire; horizontal movement and another shot remain attack-locked.
            riding_ = -1; player.velocityY = assets_.entities[0].jump;
            player.supportY = -1; airState = 1; startAnimation(player, 3);
        }
    } else if (input.fire && inventory.ammo[inventory.weapon] > 0) {
        if (dead) { return; }
        if (player.state == 2 || airState != 0) { return; }
        if (timeMs - player.attackStarted > assets_.projectiles[inventory.weapon].cooldownMs) {
            if (inventory.weapon > 0) { crouch = 0; }
            startAnimation(player, 2); player.attackStarted = timeMs;
        }
    } else if (input.left != input.right) {
        if (player.state == 2 || airState != 0) { return; }
        crouch = 0;
        int direction = 1;
        if (input.left) { direction = -1; }
        moveActor(player, direction);
    } else if (input.down) {
        if (dead) { return; }
        if (airState != 0 && airState != 2) { return; }
        if (selectedTerminal_ >= 0) {
            objects[selectedTerminal_].direction = -1;
            sounds.push_back(14);
        } else if (crouch == 0 && player.state == 0) { crouch = 8; }
        else if (player.state == 0) { lookingDown_ = true; }
    }
}

void World::spawnEnemy(int type, int x, int y, int generatorObjectIndex, int generatorSlot) {
    if (type <= 0 || type == 15 || activeEnemies() >= 29) { return; }
    Actor enemy;
    enemy.type = std::min(type, 5); enemy.x = x; enemy.y = y; enemy.source = generatorSlot;
    const auto& p = assets_.entities[enemy.type];
    enemy.health = enemyHealth_[enemy.type];
    enemy.direction = -1; enemy.supportY = y; enemy.shotType = p.damage;
    if (type >= 6) { enemy.shotType = type - 3; }
    if (generatorSlot >= 0) {
        enemy.spawnTime = timeMs;
        enemy.attackStarted = timeMs;
        const auto& generator = objects[generatorObjectIndex];
        if (generator.type == 36) { enemy.direction = 0; enemy.behavior = 100; }
        else if (generator.x < cameraX - 16 || generator.x > cameraX + ScreenWidth + 64
                 || generator.y < cameraBottom - 32 || generator.y > cameraBottom + ScreenHeight + 32) {
            enemy.x -= randomBits(31) % 60;
        }
    }
    startAnimation(enemy, 0);
    for (auto& slot : enemies) { if (!slot.alive) { slot = enemy; return; } }
    enemies.push_back(enemy);
}
int World::activeEnemies() const {
    int count = 0;
    for (const auto& enemy : enemies) { if (enemy.alive) { ++count; } }
    return count;
}
int World::activeSources() const {
    int count = 0;
    for (const auto& object : objects) {
        if (object.active && (object.type == 36 || object.type == 37) && object.state != 2) { ++count; }
    }
    return count;
}
void World::damagePlayer(int amount) {
    if (cheat.enabled()) { return; }
    if (dead || complete) { return; }
    if (inventory.armor == 1) { amount = (amount * 5 + 15) / 16; }
    if (inventory.armor == 2) { amount = (amount + 1) / 2; }
    player.health = std::max(0, player.health - amount);
    beginHealthFlash(false); sounds.push_back(6);
    if (player.health == 0) { dead = true; player.alive = false; }
}
void World::spawnBlood(int x, int y, bool fatal) {
    int count = 1;
    if (fatal) { count = 2; }
    for (int particle = 0; particle < count; ++particle) {
        // Preserve RNG order; blood particles affect subsequent random enemy turns.
        const int horizontal = randomBits(3) - 4;
        const int vertical = randomBits(3);
        addProjectile(7, x, y, horizontal, vertical, false);
        if (fatal) { projectiles.back().variant = randomBits(1); }
        else {
            projectiles.back().variant = 1;
            if (randomBits(2) < 3) { projectiles.back().variant = 2; }
        }
        --projectiles.back().life;
    }
}
void World::damageEnemy(Actor& actor, int damage, int impactY, int incomingX) {
    actor.health -= damage;
    if (actor.health > 0) {
        if (incomingX > 0) { actor.direction = -1; }
        else if (incomingX < 0) { actor.direction = 1; }
        spawnBlood(actor.x, impactY, false);
        return;
    }
    actor.alive = false;
    inventory.score += 5 * actor.type;
    scoreFlash_ = 7;
    int sound = 6;
    if (actor.type == 2 || actor.type == 4) { sound = 12; }
    if (actor.type == 3 || actor.type == 5) { sound = 13; }
    sounds.push_back(sound);
    spawnBlood(actor.x, impactY, true);
    decorateCorpse(actor.type, actor.x / 16, actor.y / 16, actor.direction);
    if (actor.source >= 0) { generators_[actor.source].lastSpawn = timeMs; }
}
void World::updateEnemies() {
    for (auto& enemy : enemies) {
        if (!enemy.alive) { continue; }
        const auto& p = assets_.entities[enemy.type];
        const int left = enemy.x - p.left, right = enemy.x + p.right;
        const int top = enemy.y + actorHeight(enemy), bottom = enemy.y - p.bottom;
        const int playerLeft = playerBounds_[0], playerRight = playerBounds_[1];
        const int playerTop = playerBounds_[3], playerBottom = playerBounds_[2];
        if (!intersects(left, right, bottom, top, playerLeft - 480, playerRight + 480, playerBottom - 640, playerTop + 640)) { continue; }
        if (enemy.direction == 0) {
            --enemy.behavior;
            if (timeMs - enemy.spawnTime > 1000) { enemy.direction = -1; }
            continue;
        }
        const int distance = std::abs(player.x - enemy.x);
        int facing = -1;
        if (player.x > enemy.x) { facing = 1; }
        const int initialDirection = enemy.direction;
        const int initialState = enemy.state;
        const int elapsed = timeMs - enemy.attackStarted;
        bool threat = intersects(left, enemy.x + 360, bottom, top, playerLeft, playerRight, playerBottom, playerTop);
        if (initialDirection < 0) { threat = intersects(enemy.x - 360, right, bottom, top, playerLeft, playerRight, playerBottom, playerTop); }
        bool canMove = enemy.health > 0 && enemy.supportY >= 0;
        bool wantsAttack = initialState == 2;
        int attackLeft = enemy.x, attackRight = right;
        if (enemy.type >= 4) { attackRight = enemy.x + 210; }
        if (initialDirection < 0) {
            attackRight = enemy.x; attackLeft = left;
            if (enemy.type >= 4) { attackLeft = enemy.x - 210; }
        }
        wantsAttack = wantsAttack || intersects(attackLeft, attackRight, bottom, top, playerLeft, playerRight, playerBottom, playerTop);
        if (enemy.type < 4) {
            canMove = canMove && !wantsAttack;
            if (initialState != 2) { wantsAttack = wantsAttack && elapsed > 1000; }
        } else {
            const int cooldown = assets_.projectiles[enemy.shotType].cooldownMs;
            if (enemy.behavior != 0) {
                wantsAttack = false; canMove = true;
                if (distance < ScreenWidth || elapsed > cooldown) { enemy.behavior = 0; }
            } else {
                canMove = canMove && !wantsAttack;
                if (initialState != 2) {
                    wantsAttack = wantsAttack && elapsed > cooldown * 2;
                    if (threat && !wantsAttack && distance > 120 && randomBits(31) % 100 < 20) {
                        canMove = true; enemy.behavior = 1; enemy.attackStarted = timeMs - cooldown + 500;
                    }
                }
            }
        }
        if (randomBits(31) % 100 < 1) { enemy.direction = -initialDirection; }
        if (threat) { enemy.direction = facing; }
        if (canMove) { moveActor(enemy, enemy.direction); }
        if (wantsAttack) {
            if (initialState != 2) {
                enemy.attackStarted = timeMs; startAnimation(enemy, 2);
                if (enemy.type == 3) { enemy.x += initialDirection * 2; }
            } else if (!advanceAnimation(enemy)) {
                startAnimation(enemy, 1);
                if (enemy.type == 3) { enemy.x -= initialDirection * 2; }
                if (enemy.type < 4) {
                    damagePlayer(enemy.shotType);
                    spawnBlood(player.x, enemy.y + 24, false);
                }
                else {
                    int muzzle = right, height = 20;
                    if (enemy.direction < 0) { muzzle = left; }
                    if (enemy.type == 5) { height = 40; }
                    addProjectile(enemy.shotType, muzzle, enemy.y + height - bob_, initialDirection, 0, true);
                    --bob_;
                    if (bob_ < 0) { bob_ = 2; }
                }
                if (dead) { break; }
            }
        }
        if (enemy.supportY == -1) {
            enemy.supportY = enemy.y;
            if (canMove) {
                enemy.direction = -initialDirection;
                moveActor(enemy, -initialDirection);
            }
        }
    }
}

bool World::overlapsPlayer(const Object& object) const {
    if (object.type >= 44) { return false; }
    const auto& p = assets_.entities[object.type];
    return intersects(object.x - p.left, object.x + p.right, object.y - p.bottom, object.y + p.top,
                      playerBounds_[0], playerBounds_[1], playerBounds_[2], playerBounds_[3]);
}
void World::showMessage(int row) {
    message = assets_.text.at(row);
    if (row == 5) { message = {"PRESS S", "TO TRAVEL", "THROUGH THE TERMINAL"}; }
    if (row == 6) { message = {"Q / E OR 1-5", "TO SWITCH", "YOUR CURRENT WEAPON"}; }
}
void World::dismissMessage() { message.clear(); }
void World::prepareMessage() {
    // Land the player before a message without updating enemies or the level clock.
    while (airState != 0 && player.alive && player.y >= 0) {
        gravity(player);
        updateCamera(false);
        updateVisuals();
        updateActorHeights();
    }
    startAnimation(player, 0);
    updateVisuals();
    updateActorHeights();
}
void World::beginDeathEffect() {
    deathParticleSteps_ = 20; deathWipeActive = deathWipeDone_ = false; deathColumns = {};
    for (int count = 0; count < 5; ++count) {
        const int horizontal = randomBits(3) - 4, vertical = randomBits(3);
        addProjectile(7, player.x, player.y + 24, horizontal, vertical, false);
        projectiles.back().variant = 0; --projectiles.back().life;
    }
    updateVisuals();
}
bool World::advanceDeathEffect() {
    sounds.clear();
    if (!deathWipeActive) {
        if (deathParticleSteps_ > 0 && !projectiles.empty()) {
            --deathParticleSteps_; updateProjectiles(); updateVisuals();
            return false;
        }
        deathWipeActive = true;
        return false;
    }
    if (deathWipeDone_) {
        // The final original repaint still consumes RNG while retaining the preceding wipe heights.
        for (int column = 0; column < static_cast<int>(deathColumns.size()); ++column) { randomBits(31); }
        return true;
    }
    for (int column = static_cast<int>(deathColumns.size()) - 1; column >= 0; --column) {
        deathColumns[column] += randomBits(31) % 40;
        if (deathColumns[column] > ScreenHeight) { deathWipeDone_ = true; }
    }
    return false;
}
void World::beginHealthFlash(bool healing) {
    if (healthFlash != 0) { return; }
    healthFlash = 3; flashColor_ = 0xff0000; flashStep_ = 0x400000;
    if (healing) { flashColor_ = 0x00ff00; flashStep_ = 0x002000; }
}

void World::setLighting(bool enabled, bool flicker) {
    lightsOn = enabled;
    for (auto light = objects.rbegin(); light != objects.rend(); ++light) {
        if (light->type != 100 && light->type != 101) { continue; }
        if (flicker && randomBits(4) >= 10) { continue; }
        const int x = light->cellX, y = light->cellY;
        int floor = light->lightFloor;
        light->state = 0;
        if (!enabled) {
            playerLight = 0;
            setTile(x, y, 107);
            if (light->type == 101 && tileAt(x, y - 1) == 119) { setTile(x, y - 1, 105); }
            if (floor != y) {
                if (tileAt(x, floor) == 147) { setTile(x, floor, 145); }
                if (tileAt(x - 1, floor) == 146) { setTile(x - 1, floor, 145); }
                if (tileAt(x + 1, floor) == 148) { setTile(x + 1, floor, 145); }
                if (tileAt(x, floor + 1) == 120) { setTile(x, floor + 1, 105); }
            }
            continue;
        }
        light->state = 1;
        if (light->type == 100) {
            floor = 0;
            if (collisionAt(x, y - 3) > 0) { floor = y - 3; }
        } else {
            floor = y - 1;
            while (floor > 0 && collisionAt(x, floor) == 0) { --floor; }
        }
        if (floor <= 0) { light->lightFloor = y; continue; }
        light->lightFloor = floor;
        if (light->type == 101) {
            if (tileAt(x, y) == 107) { setTile(x, y, 118); }
            if (tileAt(x, y - 1) == 105) { setTile(x, y - 1, 119); }
        } else if (tileAt(x, y) == 107) { setTile(x, y, 117); }
        if (tileAt(x, floor) == 145 && tileAt(x - 1, floor) == 145 && tileAt(x + 1, floor) == 145) {
            setTile(x, floor, 147); setTile(x - 1, floor, 146); setTile(x + 1, floor, 148);
            if (tileAt(x, floor + 1) == 105) { setTile(x, floor + 1, 120); }
        }
    }
}

void World::updateVisuals() {
    // Update original paint-time animations per logical tick, never per desktop refresh.
    // Apply the current shake to the whole canvas, including HUD, before decaying the next offset.
    drawOffsetY = shake;
    if (shake > 0) { shake = -shake + 1; }
    else if (shake < 0) { shake = -shake - 1; }
    for (auto item = objects.rbegin(); item != objects.rend(); ++item) {
        if (!item->active || item->type >= 100) { continue; }
        const auto& p = assets_.entities[item->type];
        if (item->x + p.right < cameraX - 16 || item->x - p.left > cameraX + ScreenWidth + 16
            || item->y + p.top < cameraBottom || item->y - p.bottom > cameraBottom + ScreenHeight) { continue; }
        item->visualFrame = item->variant;
        if (item->type == 20) {
            if (--item->variant < 0) { item->variant = 4; }
        } else if (item->type == 25) {
            if (power) { for (int& value : item->lightArcs) { value = randomBits(3); } }
            item->variant = (item->variant + 1) % 3;
        } else if (item->type == 40) {
            if (item->visualCounter++ > 1) { item->visualCounter = 0; item->variant = 1 - item->variant; }
        }
    }
    scoreVisible = scoreFlash_ == 0 || (scoreFlash_ & 1) == 0;
    if (scoreFlash_ > 0) { --scoreFlash_; }
    explosiveVisible = explosiveBlink_;
    if (inventory.explosives > 0) { explosiveBlink_ = !explosiveBlink_; }
    if (displayedWeapon_ != inventory.weapon) {
        displayedWeapon_ = inventory.weapon; weaponPanelHold_ = 0;
        if (inventory.weapon > 0) {
            if (weaponPanelY_ == 0) { weaponPanelY_ = 1; }
            weaponPanelSpeed_ = 5;
        } else { weaponPanelY_ = 0; weaponPanelSpeed_ = 0; }
    }
    ammoPanelY = weaponPanelY_;
    if (weaponPanelY_ != 0 && inventory.weapon > 0) {
        if (timeMs - weaponPanelHold_ > 3000) { weaponPanelHold_ = 0; }
        if (weaponPanelHold_ == 0) {
            weaponPanelY_ += weaponPanelSpeed_;
            if (weaponPanelY_ >= 20) { weaponPanelY_ = 20; weaponPanelSpeed_ = -weaponPanelSpeed_; weaponPanelHold_ = timeMs; }
            else if (weaponPanelY_ < 0) { weaponPanelY_ = 0; weaponPanelSpeed_ = 0; }
        }
    }
    flashOuterColor = flashInnerColor = -1;
    if (healthFlash > 0) {
        flashOuterColor = flashColor_ - flashStep_; flashInnerColor = flashColor_;
        if ((healthFlash & 1) != 0) { flashColor_ -= flashStep_; }
        else { flashColor_ += flashStep_; }
        --healthFlash;
    }
}

void World::updateTerminal(Object& object, int index, bool overlap) {
    if (object.direction != 0) {
        object.doorOffset += object.direction * 6;
        if (object.direction < 0 && object.doorOffset < -48) {
            object.direction = 1;
            if (selectedTerminal_ >= 0) {
                const auto& departure = objects[selectedTerminal_];
                if (departure.variant == 1 && departure.terminalGroup == 0) {
                    if (cleared) { complete = true; sounds.push_back(10); }
                    else { showMessage(14); }
                    return;
                }
                for (int target = 0; target < static_cast<int>(objects.size()); ++target) {
                    auto& destination = objects[target];
                    if (target != selectedTerminal_ && destination.type == 41 && destination.terminalGroup == departure.terminalGroup) {
                        player.x = destination.x + 16; player.y = destination.y;
                        playerFootRowOverride_ = destination.cellY - 1;
                        player.velocityY = 0; player.supportY = -1; riding_ = -1;
                        destination.direction = 1; destination.doorOffset = -48;
                        selectedTerminal_ = target; cameraTransit_ = true;
                        // The teleport tick still finishes the departure terminal's arrow and overlap handling.
                        break;
                    }
                }
            }
        } else if (object.direction > 0 && object.doorOffset > 0) {
            object.direction = 0; object.doorOffset = 0;
        }
    }
    if (overlap) {
        if (object.variant == 1) {
            if (--object.animationFrame <= 0) { object.animationFrame = 4; }
            selectedTerminal_ = index;
            if ((inventory.tutorialFlags & 2U) == 0) { inventory.tutorialFlags |= 2U; showMessage(5); }
        }
        if (object.variant == 2 && object.terminalGroup == 0 && cleared) { complete = true; sounds.push_back(10); }
    } else {
        object.animationFrame = 0;
        if (selectedTerminal_ == index) { selectedTerminal_ = -1; }
    }
}

int World::updateObjects() {
    playerBounds_ = {player.x - 11, player.x + 11, player.y, player.y + actorHeight(player)};
    int generatorOrdinal = 0;
    int activeSourceCount = 0;
    for (int index = static_cast<int>(objects.size()) - 1; index >= 0; --index) {
        auto& object = objects[index];
        if (!object.active) { continue; }
        const bool overlap = overlapsPlayer(object);
        const int previousX = object.x, previousY = object.y;
        // Test pickup collision against the pre-bob bounds before updating the floating position.
        if (object.type < 44 && assets_.entities[object.type].motion == 0) {
            constexpr std::array<int, 8> verticalOffsets{0, 1, 0, -1, 0, -1, 0, 1};
            object.y += verticalOffsets[object.motionFrame / 2];
            object.motionFrame = (object.motionFrame + 2) % 16;
        }
        if (overlap) {
            int tutorial = -1;
            bool picked = false;
            switch (object.type) {
            case 10:
                inventory.owned[object.variant] = true;
                inventory.ammo[object.variant] = assets_.projectiles[object.variant].capacity;
                inventory.weapon = object.variant; startAnimation(player, 0);
                picked = true; tutorial = 6; break;
            case 14:
                if (inventory.owned[object.variant] && inventory.ammo[object.variant] < assets_.projectiles[object.variant].capacity) {
                    inventory.ammo[object.variant] = assets_.projectiles[object.variant].capacity;
                    if (object.variant > inventory.weapon) { inventory.weapon = object.variant; }
                    picked = true; tutorial = 7;
                }
                break;
            case 18: inventory.score += 10; scoreFlash_ = 7; picked = true; break;
            case 19: ++inventory.explosives; picked = true; tutorial = 4; break;
            case 20:
                if (player.health < 100) { player.health = 100; picked = true; beginHealthFlash(true); }
                sounds.push_back(1);
                break;
            case 21:
                if (inventory.armor <= object.variant) { inventory.armor = object.variant; picked = true; tutorial = 8 + object.variant; }
                break;
            case 38: {
                const bool wasLit = lightsOn;
                object.active = false; power = true; sounds.push_back(0);
                setTile(object.x / 16, object.y / 16, 137);
                message.assign(4, "");
                int line = 1;
                for (int previous = index - 1; previous >= std::max(0, index - 2); --previous) {
                    auto& terminal = objects[previous];
                    if (terminal.type != 41) { break; }
                    terminal.variant = 1;
                    setTile(terminal.x / 16 + 2, terminal.y / 16 + 2, 121);
                    if (previous == index - 1) {
                        if (!wasLit) { line = 0; }
                        if (terminal.terminalGroup > 0) {
                            message[line++] = assets_.text[3][0] + std::to_string(terminal.terminalGroup) + ":";
                            message[line++] = assets_.text[3][1];
                        } else {
                            message[line++] = assets_.text[3][4]; message[line++] = assets_.text[3][5];
                        }
                    }
                }
                if (!wasLit) { message[line++] = assets_.text[3][2]; message[line] = assets_.text[3][3]; }
                setLighting(true, false);
                break;
            }
            case 40: damagePlayer(assets_.entities[40].damage); break;
            }
            if (picked) {
                object.active = false;
                if (object.type != 20) { sounds.push_back(1); }
                if (tutorial >= 0 && (inventory.tutorialFlags & (1U << tutorial)) == 0) {
                    inventory.tutorialFlags |= 1U << tutorial; showMessage(tutorial);
                }
            }
        }
        if (object.type == 34 || object.type == 35) {
            const auto& p = assets_.entities[object.type];
            const bool onPlatform = intersects(object.x, object.x + p.right, object.y, object.y + p.top,
                                               player.x - 2, player.x + 2, playerBounds_[2], player.y + 4);
            if (onPlatform) {
                airState = 0;
                if (riding_ != index && player.velocityY <= 0) {
                    applyFallDamage();
                    riding_ = index; player.x = object.x + 10; player.y = object.y + 10;
                    player.velocityY = 1; player.supportY = player.y;
                    startAnimation(player, 0);
                }
            } else if (riding_ == index) {
                riding_ = -1; player.supportY = -1; player.velocityY = 0;
                airState = 3; startAnimation(player, 3);
            }
            const int nextX = object.x + p.speedX * object.direction;
            const int nextY = object.y + p.speedY * object.direction;
            int offsetY = 0, offsetX = 0;
            if (object.type == 34) {
                offsetY = 1;
                if (object.direction > 0) {
                    offsetY = 4;
                }
            } else if (object.direction > 0) {
                offsetX = 1;
            }
            if (collisionAt(nextX / 16 + offsetX, nextY / 16 + offsetY) <= 0 && nextY < level.height * 16) {
                if (riding_ == index) {
                    player.x += nextX - object.x; player.y += nextY - object.y; player.supportY = player.y;
                    if (object.type == 34) { playerFootRowOverride_ = nextY / 16; }
                }
                object.x = nextX; object.y = nextY;
                object.cellX = nextX / 16; object.cellY = nextY / 16;
            } else { object.direction = -object.direction; }
        }
        if (object.type == 36 || object.type == 37) {
            // Renumber each traversal; destroyed type 37 becomes 33, shifting later generators to earlier slots.
            const int generatorSlot = generatorOrdinal++;
            auto& generator = generators_[generatorSlot];
            // Original l() still counts a generator on its destruction tick; the clear message follows next tick.
            if (object.state == 0 || object.state == 1) { ++activeSourceCount; }
            if (object.state == 0) {
                int alive = 0;
                for (const auto& enemy : enemies) { if (enemy.alive && enemy.source == generatorSlot) { ++alive; } }
                if (alive < generator.limit && timeMs - generator.lastSpawn > 5000) {
                    const int type = generator.types[generator.cursor];
                    generator.cursor = (generator.cursor + 1) % 4;
                    if (type != 15) { generator.lastSpawn = timeMs; }
                    spawnEnemy(type, object.x + 8, object.cellY * 16 + 8, index, generatorSlot);
                }
                if (overlap && inventory.explosives > 0) {
                    --inventory.explosives; object.state = 1; object.countdown = 5800; object.animationFrame = 9;
                } else if (overlap && (inventory.tutorialFlags & (1U << 8)) == 0) {
                    inventory.tutorialFlags |= 1U << 8; showMessage(8);
                }
            } else if (object.state == 1) {
                object.countdown -= TickMilliseconds;
                object.animationFrame = object.countdown >> 10;
                if (object.countdown <= 0) {
                    object.state = 2; shake = 8; sounds.push_back(15);
                    const auto& p = assets_.entities[object.type];
                    for (int burst = 0; burst < 15; ++burst) {
                        const int x = previousX - p.right + randomBits(31) % (p.right * 2);
                        const int y = previousY - p.right + randomBits(31) % (p.top * 2);
                        addProjectile(5, x, y, 0, 0, false);
                    }
                    addProjectile(5, previousX + 8, previousY + 16, 0, 0, false);
                    projectiles.back().damage = 1000; projectiles.back().radius = 72;
                    const int x = object.cellX, y = object.cellY;
                    patchWall(x, y + 1); patchWall(x + 1, y + 1);
                    if (tileAt(x, y + 3) == 105 && tileAt(x + 1, y + 3) == 105) {
                        patchWall(x, y + 2); patchWall(x + 1, y + 2);
                    }
                    patchFloor(x - 1, y); patchFloor(x, y); patchFloor(x + 2, y);
                    if (object.type == 37) { object.type = 33; }
                }
            }
        }
        if (object.type == 41) {
            updateTerminal(object, index, overlap);
            if (cameraTransit_) { break; }
        }
        if (object.type == 38 && --object.animationFrame < 0) { object.animationFrame = 3; }
        if (object.type == 40) { if (--object.health <= 0) { object.active = false; } }
    }
    return activeSourceCount;
}

void World::addProjectile(int type, int x, int y, int direction, int vertical, bool hostile) {
    const auto& p = assets_.projectiles.at(type);
    Projectile projectile;
    projectile.type = type; projectile.x = x; projectile.y = y;
    projectile.dx = p.speedX * direction; projectile.dy = p.speedY * vertical;
    projectile.remainingX = p.rangeX; projectile.remainingY = p.rangeY;
    projectile.damage = p.damage;
    if (hostile) { projectile.owner = 1; }
    if (type == 3) { projectile.life = 1; }
    if (type == 9) { projectile.life = 4; }
    projectiles.push_back(projectile);
}
bool World::projectileHits(const Projectile& projectile, int nextX, int nextY, int x, int y, const EntityParameters& p) const {
    int left = std::min(projectile.x, nextX), right = std::max(projectile.x, nextX);
    int bottom = std::min(projectile.y, nextY), top = std::max(projectile.y, nextY);
    if (projectile.type == 5) {
        left = projectile.x - projectile.radius; right = projectile.x + projectile.radius;
        bottom = projectile.y - projectile.radius; top = projectile.y + projectile.radius;
    }
    return intersects(left, right, bottom, top, x - p.left, x + p.right, y - p.bottom, y + p.top);
}
Object& World::spawnObject(int type, int x, int y) {
    Object object;
    object.type = type; object.x = x; object.y = y; object.baselineY = y;
    object.cellX = x / 16; object.cellY = y / 16;
    object.health = assets_.entities[type].health;
    objects.push_back(object);
    return objects.back();
}
void World::destroyObject(int index, const Projectile& projectile) {
    auto& object = objects[index];
    object.active = false; shake = 3;
    const int x = object.cellX, y = object.cellY;
    if (object.type == 29) { setCollision(x + 1, y + 1, 0); }
    if (object.type == 30 || object.type == 31) {
        spawnObject(40, object.x + 7, object.y);
        sounds.push_back(7);
    } else if (object.type != 32 && x + 1 < level.width && collisionAt(x + 1, y + 1) == 0) {
        patchWall(x, y + 1); patchFloor(x, y);
    }
    addProjectile(9, object.x + 8, object.y + 36, 0, 1, false);
    if (object.type != 32) {
        addProjectile(5, object.x + 8, object.y + 24, 0, 0, false);
        const int firstX = object.x + randomBits(2);
        const int firstY = object.y + 16 + randomBits(4);
        addProjectile(5, firstX, firstY, 0, 0, false);
        const int secondX = object.x + 16 - randomBits(2);
        const int secondY = object.y + 16 + randomBits(4);
        addProjectile(5, secondX, secondY, 0, 0, false);
    } else if (tileAt(x, y) == 145 && randomBits(2) < 2) { spawnObject(18, x * 16, y * 16); }
    switch (object.type) {
    case 26:
        projectiles.back().radius = 32;
        [[fallthrough]];
    case 27: case 28: case 30:
        setCollision(x, y + 2, 0);
        [[fallthrough]];
    case 29: case 31:
        setCollision(x, y + 1, 0);
        break;
    case 32: {
        const int tile = tileAt(x, y);
        int support = 0;
        if (tile >= 145 && tile <= nextFloorTile_) { support = 8; }
        else {
            for (int previous = index + 1; previous < static_cast<int>(objects.size()); ++previous) {
                const auto& lower = objects[previous];
                if (lower.active && lower.type >= 26 && lower.type <= 32) {
                    support = std::clamp(lower.y - lower.cellY * 16, 0, 16);
                    break;
                }
            }
        }
        setCollision(x, y, support);
        for (int row = y + 1; row <= y + 1 + object.stackCount; ++row) { setCollision(x, row, 0); }
        for (int piece = 0; piece < 10; ++piece) {
            const int horizontal = randomBits(32) % 5;
            const int vertical = randomBits(31) % 9;
            addProjectile(8, projectile.x, projectile.y, horizontal, vertical, false);
            projectiles.back().variant = 1; --projectiles.back().life;
        }
        int remaining = object.stackCount;
        for (int upper = index - 1; upper >= 0 && remaining > 0; --upper) {
            auto& crate = objects[upper];
            if (!crate.active || crate.type < 26 || crate.type > 32) { continue; }
            if (crate.type != 32) { break; }
            const int base = tileAt(crate.cellX, crate.cellY);
            if (base >= 145 && base <= nextFloorTile_) { break; }
            crate.active = false; --remaining;
        }
        player.supportY = -1;
        break;
    }
    }
}
void World::impactProjectile(Projectile& projectile, int left, int right) {
    const int originalType = projectile.type;
    if (originalType < 2) { projectile.type = 6; sounds.push_back(8); }
    else {
        projectile.type = 5;
        if (originalType != 5) { sounds.push_back(2); }
    }
    if (projectile.dx > 0) { projectile.x = left + 5; }
    else if (projectile.dx < 0) { projectile.x = right - 5; }
    projectile.originalDirection = projectile.dx;
    projectile.dx = 0; projectile.dy = 0;
    if (originalType != projectile.type) { projectile.life = 3; }
}
void World::updateProjectiles() {
    // Traverse newest projectiles first and defer new impact fragments until the next tick.
    for (int index = static_cast<int>(projectiles.size()) - 1; index >= 0; --index) {
        auto& projectile = projectiles[index];
        const Projectile sweep = projectile;
        const int originalType = projectile.type;
        int nextX = projectile.x + projectile.dx, nextY = projectile.y + projectile.dy;
        const int remainingX = projectile.remainingX - std::abs(projectile.dx);
        const int remainingY = projectile.remainingY - std::abs(projectile.dy);
        if (remainingX <= 0 || remainingY <= 0 || nextX < 0 || nextY < 0 || nextX >= level.width * 16 || nextY >= level.height * 16) {
            projectile.alive = false; continue;
        }
        bool hitTerrain = originalType != 6;
        bool reachedActors = true;
        bool removeBullet = false;
        if (originalType < 6) {
            for (int objectIndex = static_cast<int>(objects.size()) - 1; objectIndex >= 0; --objectIndex) {
                auto& object = objects[objectIndex];
                if (!object.active || object.type < 26 || object.type > 32) { continue; }
                const auto& p = assets_.entities[object.type];
                if (!projectileHits(sweep, sweep.x + sweep.dx, sweep.y + sweep.dy, object.x, object.y, p)) { continue; }
                impactProjectile(projectile, object.x - p.left, object.x + p.right);
                object.health -= projectile.damage;
                nextX = projectile.x; nextY = projectile.y;
                if (object.health <= 0) { destroyObject(objectIndex, projectile); }
                if (originalType != 5) { reachedActors = false; break; }
            }
            hitTerrain = reachedActors;
            if (reachedActors) {
                int lastActor = static_cast<int>(enemies.size());
                if (projectile.owner == 1) { lastActor = 0; }
                int firstActor = 0;
                if (projectile.owner == 2) { firstActor = 1; }
                for (int actorIndex = lastActor; actorIndex >= firstActor; --actorIndex) {
                    Actor* target = &player;
                    if (actorIndex > 0) { target = &enemies[actorIndex - 1]; }
                    if (!target->alive) { continue; }
                    auto p = assets_.entities[target->type]; p.top = actorHeight(*target);
                    if (!projectileHits(sweep, sweep.x + sweep.dx, sweep.y + sweep.dy, target->x, target->y, p)) { continue; }
                    impactProjectile(projectile, target->x - p.left, target->x + p.right);
                    if (projectile.owner == 0) { projectile.owner = 2; }
                    hitTerrain = false; removeBullet = originalType < 2;
                    if (actorIndex == 0) {
                        damagePlayer(projectile.damage);
                        spawnBlood(player.x, projectile.y, dead);
                        if (dead) { decorateCorpse(0, player.x / 16, player.y / 16, player.direction); }
                    } else { damageEnemy(*target, projectile.damage, projectile.y, sweep.dx); }
                    if (originalType != 5) { break; }
                }
            }
            if (removeBullet) { projectile.alive = false; continue; }
            hitTerrain = hitTerrain && projectile.type != 5;
        } else if (originalType == 7 || originalType == 8) { projectile.dy -= 2; }

        const int cellX = nextX / 16;
        int cellY = nextY / 16;
        if (hitTerrain && nextY < cellY * 16 + collisionAt(cellX, cellY)) {
            if (projectile.type >= 6) { projectile.alive = false; continue; }
            const int hitType = projectile.type;
            projectile.type = 5;
            if (hitType < 2) { projectile.type = 6; }
            projectile.x = (cellX + 1) * 16;
            if (sweep.dx > 0) { projectile.x = cellX * 16 + 4; }
            projectile.originalDirection = projectile.dx;
            projectile.dx = 0; projectile.dy = 0; projectile.life = 3;
            int wallDirection = 1;
            if (sweep.dx > 0) { wallDirection = -1; }
            if (hitType < 5 && tileAt(cellX, cellY) == 158 && tileAt(cellX - wallDirection, cellY) == 105) {
                sounds.push_back(9);
                if (tileAt(cellX, cellY - 1) != 152) { --cellY; }
                const int damage = (collisionAt(cellX, cellY) + projectile.damage) & 255;
                setCollision(cellX, cellY, damage);
                for (int speed = 5; speed >= 4; --speed) {
                    addProjectile(8, projectile.x, nextY, wallDirection * speed, randomBits(31) % 10, false);
                    projectiles.back().variant = randomBits(31) % 2; --projectiles.back().life;
                }
                if (damage > 100) {
                    shake = 5;
                    setCollision(cellX, cellY - 1, 10); setTile(cellX, cellY - 1, 153);
                    for (int row = cellY; row <= cellY + 1; ++row) {
                        setCollision(cellX, row, 0); setTile(cellX, row, 113); patchWall(cellX, row);
                        addProjectile(5, projectile.x, row * 16 + 8, 0, 0, false);
                    }
                    setCollision(cellX, cellY + 2, 0); setTile(cellX, cellY + 2, 133);
                }
            }
        } else {
            projectile.x = nextX; projectile.y = nextY;
            projectile.remainingX = remainingX; projectile.remainingY = remainingY;
        }
        switch (projectile.type) {
        case 2: case 5: case 6: case 9:
            if (--projectile.life < 0) { projectile.alive = false; }
            break;
        case 3: projectile.life = 1 - projectile.life; break;
        case 7: case 8:
            if (--projectile.life < 0) { projectile.life = 2; }
            break;
        }
    }
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.alive; }), projectiles.end());
}

void World::updateCamera(bool lookingDown) {
    int anchorX = 32;
    if (player.direction < 0) { anchorX = ScreenWidth - 32; }
    int anchorY = 32;
    if (lookingDown) { anchorY = ScreenHeight - 48; }
    cameraX = toward(cameraX, player.x - anchorX, 20);
    cameraBottom = toward(cameraBottom, player.y - anchorY, std::max(20, std::abs(player.velocityY)));
    cameraX = std::clamp(cameraX, 0, std::max(0, level.width * 16 - ScreenWidth));
    cameraBottom = std::clamp(cameraBottom, 0, std::max(0, level.height * 16 - ScreenHeight));
}
bool World::advanceCameraTransition(bool downHeld) {
    if (!downHeld) { lookingDown_ = false; }
    const int previousX = cameraX, previousY = cameraBottom;
    int anchorX = 32, anchorY = 32;
    if (player.direction < 0) { anchorX = ScreenWidth - 32; }
    if (lookingDown_) { anchorY = ScreenHeight - 48; }
    const int targetX = std::clamp(player.x - anchorX, 0, std::max(0, level.width * 16 - ScreenWidth));
    const int targetY = std::clamp(player.y - anchorY, 0, std::max(0, level.height * 16 - ScreenHeight));
    cameraX = toward(cameraX, targetX, 40); cameraBottom = toward(cameraBottom, targetY, 40);
    const bool finished = previousX == cameraX && previousY == cameraBottom;
    if (!finished) {
        // Teleport repaints advance animations, shared collision heights, and powered-object RNG state.
        updateVisuals();
        updateActorHeights();
    }
    return finished;
}
void World::tick(const Input& input) {
    sounds.clear();
    if (dead || complete || !message.empty()) { return; }
    if (!input.down) { lookingDown_ = false; }
    timeMs += TickMilliseconds; ++ticks;
    if (cameraTransit_) {
        // Teleport camera movement is a physics-paused phase that the desktop layer can display incrementally.
        while (!advanceCameraTransition(input.down)) {}
        cameraTransit_ = false;
        applyPlayerInput(input);
        updateVisuals(); updateActorHeights();
        return;
    }
    if (power) {
        if (!lightsOn) { setLighting(true, false); }
        else if (randomBits(8) < 12) { setLighting(false, true); }
    }
    updatePlayer(input);
    updateCamera(lookingDown_ && input.down);
    const int activeSourceCount = updateObjects();
    updateEnemies();
    if (!cleared && activeEnemies() == 0 && activeSourceCount == 0) { cleared = true; showMessage(11); }
    updateProjectiles();
    applyPlayerInput(input);
    updateVisuals();
    updateActorHeights();
}
}
