/** @file world_object.cpp
 * @brief Update pickups, platforms, generators, terminals, and breakable objects.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace alien {
namespace {
bool isSource(int type) {
    return type == object_type::TeleporterSource || type == object_type::HiveSource;
}
bool isBreakable(int type) {
    return type >= object_type::BreakableFirst && type <= object_type::BreakableLast;
}
}
int World::activeSources() const {
    int count = 0;
    for (const auto& object : objects) {
        if (object.active && isSource(object.type) && object.state != source_state::Destroyed) { ++count; }
    }
    return count;
}

void World::updateTerminal(Object& object, int index, bool overlap) {
    if (object.direction != 0) {
        object.doorOffset += object.direction * rules::TerminalDoorStep;
        if (object.direction < 0 && object.doorOffset < rules::TerminalDoorOpen) {
            object.direction = 1;
            if (selectedTerminal_ >= 0) {
                const auto& departure = objects[selectedTerminal_];
                if (departure.variant == terminal_variant::Powered && departure.terminalGroup == rules::ExitTerminalGroup) {
                    if (cleared) { complete = true; sounds.push_back(sound::LevelComplete); }
                    else { showMessage(message_key::ExitUnavailable); }
                    return;
                }
                for (int target = 0; target < static_cast<int>(objects.size()); ++target) {
                    auto& destination = objects[target];
                    if (target != selectedTerminal_ && destination.type == object_type::Terminal && destination.terminalGroup == departure.terminalGroup) {
                        player.x = destination.x + rules::TerminalArrivalOffsetX; player.y = destination.y;
                        playerFootRowOverride_ = destination.cellY - 1;
                        player.velocityY = 0; player.supportY = -1; riding_ = -1;
                        destination.direction = 1; destination.doorOffset = rules::TerminalDoorOpen;
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
        if (object.variant == terminal_variant::Powered) {
            if (--object.animationFrame <= 0) { object.animationFrame = rules::TerminalArrowFrames; }
            selectedTerminal_ = index;
            showTutorial(tutorial::Terminal, message_key::TerminalTravel);
        }
        if (object.variant == terminal_variant::Doorless && object.terminalGroup == rules::ExitTerminalGroup && cleared) {
            complete = true; sounds.push_back(sound::LevelComplete);
        }
    } else {
        object.animationFrame = 0;
        if (selectedTerminal_ == index) { selectedTerminal_ = -1; }
    }
}

int World::updateObjects() {
    playerBounds_ = {player.x - rules::PlayerHalfWidth, player.x + rules::PlayerHalfWidth, player.y, player.y + actorHeight(player)};
    int generatorOrdinal = 0;
    int activeSourceCount = 0;
    for (int index = static_cast<int>(objects.size()) - 1; index >= 0; --index) {
        auto& object = objects[index];
        if (!object.active) { continue; }
        const bool overlap = overlapsPlayer(object);
        const int previousX = object.x, previousY = object.y;
        // Test pickup collision against the pre-bob bounds before updating the floating position.
        if (assets_.hasParameters(object.type) && assets_.entities[object.type].motion == 0) {
            object.y += rules::PickupBob[object.motionFrame / 2];
            object.motionFrame = (object.motionFrame + 2) % rules::PickupBobFrames;
        }
        if (overlap) {
            int tutorialBit = -1;
            const char* tutorialKey = nullptr;
            bool picked = false;
            switch (object.type) {
            case object_type::WeaponPickup:
                inventory.owned[object.variant] = true;
                inventory.ammo[object.variant] = assets_.projectiles[object.variant].capacity;
                inventory.weapon = object.variant; startAnimation(player, rules::StateIdle);
                picked = true; tutorialBit = tutorial::Weapon; tutorialKey = message_key::SwitchWeapon; break;
            case object_type::AmmoPickup:
                if (inventory.owned[object.variant] && inventory.ammo[object.variant] < assets_.projectiles[object.variant].capacity) {
                    inventory.ammo[object.variant] = assets_.projectiles[object.variant].capacity;
                    if (object.variant > inventory.weapon) { inventory.weapon = object.variant; }
                    picked = true; tutorialBit = tutorial::Ammo; tutorialKey = message_key::AmmoPickedUp;
                }
                break;
            case object_type::ScorePickup: inventory.score += rules::PickupScore; scoreFlash_ = rules::ScoreFlashTicks; picked = true; break;
            case object_type::ExplosivePickup:
                ++inventory.explosives; picked = true; tutorialBit = tutorial::Explosive; tutorialKey = message_key::FindDynamitePlace; break;
            case object_type::Medkit:
                if (player.health < rules::MaxHealth) { player.health = rules::MaxHealth; picked = true; beginHealthFlash(true); }
                sounds.push_back(sound::Pickup);
                break;
            case object_type::ArmorPickup:
                if (inventory.armor <= object.variant) {
                    inventory.armor = object.variant; picked = true; tutorialBit = tutorial::ArmorBase + object.variant;
                    tutorialKey = message_key::HeavyArmor;
                    if (object.variant == rules::LightArmor) { tutorialKey = message_key::LightArmor; }
                }
                break;
            case object_type::PowerSwitch: {
                const bool wasLit = lightsOn;
                object.active = false; power = true; sounds.push_back(sound::PowerOn);
                setTile(object.x / TileSize, object.y / TileSize, tile_id::UsedSocket);
                // Report up to two preceding terminals; the nearest one names the opened group.
                message.assign(4, "");
                int line = 1;
                for (int previous = index - 1; previous >= std::max(0, index - 2); --previous) {
                    auto& terminal = objects[previous];
                    if (terminal.type != object_type::Terminal) { break; }
                    terminal.variant = terminal_variant::Powered;
                    setTile(terminal.x / TileSize + 2, terminal.y / TileSize + 2, tile_id::PoweredTerminalWall);
                    if (previous == index - 1) {
                        if (!wasLit) { line = 0; }
                        if (terminal.terminalGroup > rules::ExitTerminalGroup) {
                            message[line++] = assets_.text.label("terminal_prefix") + std::to_string(terminal.terminalGroup) + ":";
                            message[line++] = assets_.text.label("terminal_activated");
                        } else {
                            message[line++] = assets_.text.label("sector_exit"); message[line++] = assets_.text.label("sector_exit_open");
                        }
                    }
                }
                if (!wasLit) { message[line++] = assets_.text.label("generator"); message[line] = assets_.text.label("generator_running"); }
                setLighting(true, false);
                break;
            }
            case object_type::FloorFlame: damagePlayer(assets_.entities[object_type::FloorFlame].damage); break;
            }
            if (picked) {
                object.active = false;
                if (object.type != object_type::Medkit) { sounds.push_back(sound::Pickup); }
                if (tutorialBit >= 0) { showTutorial(tutorialBit, tutorialKey); }
            }
        }
        if (object.type == object_type::VerticalPlatform || object.type == object_type::HorizontalPlatform) {
            const auto& p = assets_.entities[object.type];
            bool onPlatform = intersects(object.x, object.x + p.right, object.y, object.y + p.top,
                                         player.x - rules::PlatformFootReach, player.x + rules::PlatformFootReach,
                                         playerBounds_[2], player.y + rules::PlatformFootHeight);
            // Original rules ground a rising jump that still overlaps the lift, cancelling air steering.
            if (modernControls && airState == rules::AirJumping && player.velocityY > 0) { onPlatform = false; }
            if (onPlatform) {
                airState = rules::AirGrounded;
                if (riding_ != index && player.velocityY <= 0) {
                    applyFallDamage();
                    riding_ = index; player.x = object.x + rules::PlatformRiderOffset; player.y = object.y + rules::PlatformRiderOffset;
                    player.velocityY = rules::PlatformBoardVelocity; player.supportY = player.y;
                    startAnimation(player, rules::StateIdle);
                }
            } else if (riding_ == index) {
                riding_ = -1; player.supportY = -1; player.velocityY = 0;
                if (!modernControls || !stepOffPlatform()) {
                    airState = rules::AirFalling; startAnimation(player, rules::StateAir);
                }
            }
            const int nextX = object.x + p.speedX * object.direction;
            const int nextY = object.y + p.speedY * object.direction;
            int offsetY = 0, offsetX = 0;
            if (object.type == object_type::VerticalPlatform) {
                offsetY = rules::PlatformProbeBelow;
                if (object.direction > 0) {
                    offsetY = rules::PlatformProbeAbove;
                }
            } else if (object.direction > 0) {
                offsetX = rules::PlatformProbeAhead;
            }
            if (collisionAt(nextX / TileSize + offsetX, nextY / TileSize + offsetY) <= 0 && nextY < level.height * TileSize) {
                if (riding_ == index) {
                    player.x += nextX - object.x; player.y += nextY - object.y; player.supportY = player.y;
                    if (object.type == object_type::VerticalPlatform) { playerFootRowOverride_ = nextY / TileSize; }
                }
                object.x = nextX; object.y = nextY;
                object.cellX = nextX / TileSize; object.cellY = nextY / TileSize;
            } else { object.direction = -object.direction; }
        }
        if (isSource(object.type)) {
            // Renumber each traversal; destroyed type 37 becomes 33, shifting later generators to earlier slots.
            const int generatorSlot = generatorOrdinal++;
            auto& generator = generators_[generatorSlot];
            // Original l() still counts a generator on its destruction tick; the clear message follows next tick.
            if (object.state == source_state::Idle || object.state == source_state::Armed) { ++activeSourceCount; }
            if (object.state == source_state::Idle) {
                int alive = 0;
                for (const auto& enemy : enemies) { if (enemy.alive && enemy.source == generatorSlot) { ++alive; } }
                if (alive < generator.limit && timeMs - generator.lastSpawn > rules::GeneratorSpawnIntervalMs) {
                    const int type = generator.types[generator.cursor];
                    generator.cursor = (generator.cursor + 1) % rules::GeneratorSpawnPattern;
                    if (type != rules::EmptySpawnType) { generator.lastSpawn = timeMs; }
                    spawnEnemy(type, object.x + rules::SourceCenterOffset, object.cellY * TileSize + rules::SourceCenterOffset, index, generatorSlot);
                }
                if (overlap && inventory.explosives > 0) {
                    --inventory.explosives; object.state = source_state::Armed;
                    object.countdown = rules::DynamiteCountdownMs; object.animationFrame = rules::DynamiteFirstDigit;
                } else if (overlap) {
                    showTutorial(tutorial::Source, message_key::PlantDynamite);
                }
            } else if (object.state == source_state::Armed) {
                object.countdown -= TickMilliseconds;
                object.animationFrame = object.countdown >> rules::DynamiteDigitShift;
                if (object.countdown <= 0) {
                    object.state = source_state::Destroyed; shake = rules::SourceBlastShake; sounds.push_back(sound::SourceExplosion);
                    const auto& p = assets_.entities[object.type];
                    for (int burst = 0; burst < rules::SourceBlastBursts; ++burst) {
                        const int x = previousX - p.right + randomBits(31) % (p.right * 2);
                        const int y = previousY - p.right + randomBits(31) % (p.top * 2);
                        addProjectile(rules::ExplosionProjectile, x, y, 0, 0, false);
                    }
                    addProjectile(rules::ExplosionProjectile, previousX + rules::SourceCenterOffset, previousY + TileSize, 0, 0, false);
                    projectiles.back().damage = rules::SourceBlastDamage; projectiles.back().radius = rules::SourceBlastRadius;
                    const int x = object.cellX, y = object.cellY;
                    patchWall(x, y + 1); patchWall(x + 1, y + 1);
                    if (tileAt(x, y + 3) == tile_id::Wall && tileAt(x + 1, y + 3) == tile_id::Wall) {
                        patchWall(x, y + 2); patchWall(x + 1, y + 2);
                    }
                    patchFloor(x - 1, y); patchFloor(x, y); patchFloor(x + 2, y);
                    if (object.type == object_type::HiveSource) { object.type = object_type::DestroyedHive; }
                }
            }
        }
        if (object.type == object_type::Terminal) {
            updateTerminal(object, index, overlap);
            if (cameraTransit_) { break; }
        }
        if (object.type == object_type::PowerSwitch && --object.animationFrame < 0) { object.animationFrame = rules::SwitchArrowLastFrame; }
        // Floor flames burn out when their health counts down to zero.
        if (object.type == object_type::FloorFlame) { if (--object.health <= 0) { object.active = false; } }
    }
    return activeSourceCount;
}

bool World::stepOffPlatform() {
    // The fixed 10px walk step can leave the centre over the gap beside a lift; use the whole foot span instead.
    const int feetRow = player.y / TileSize;
    const int leftColumn = std::max(0, (player.x - rules::PlayerHalfWidth) / TileSize);
    const int rightColumn = std::min(level.width - 1, (player.x + rules::PlayerHalfWidth) / TileSize);
    int bestColumn = -1, bestSurface = 0, bestDistance = 0;
    for (int column = leftColumn; column <= rightColumn; ++column) {
        for (int row = std::max(0, feetRow - 1); row <= feetRow + 1; ++row) {
            const int height = collisionAt(column, row);
            if (height <= 0) { continue; }
            // Skip solid wall cells; the cell above the surface must be open.
            if (collisionAt(column, row + 1) > 0) { continue; }
            const int surface = row * TileSize + height;
            if (std::abs(surface - player.y) > rules::LiftExitTolerance) { continue; }
            const int distance = std::abs(column * TileSize + TileSize / 2 - player.x);
            if (bestColumn < 0 || distance < bestDistance) {
                bestColumn = column; bestSurface = surface; bestDistance = distance;
            }
        }
    }
    if (bestColumn < 0) { return false; }
    // Move the centre onto the chosen column so later walk and gravity checks see solid ground.
    player.x = std::clamp(player.x, bestColumn * TileSize, bestColumn * TileSize + TileSize - 1);
    player.y = bestSurface; player.supportY = bestSurface; player.velocityY = 0;
    playerFootRowOverride_ = -1;
    airState = rules::AirGrounded;
    std::cout << "Lift exit assist: landed at " << player.x << ',' << player.y << '\n';
    return true;
}

Object& World::spawnObject(int type, int x, int y) {
    Object object;
    object.type = type; object.x = x; object.y = y; object.baselineY = y;
    object.cellX = x / TileSize; object.cellY = y / TileSize;
    object.health = assets_.entities[type].health;
    objects.push_back(object);
    return objects.back();
}
void World::destroyObject(int index, const Projectile& projectile) {
    auto& object = objects[index];
    object.active = false; shake = rules::ObjectBreakShake;
    const int x = object.cellX, y = object.cellY;
    if (object.type == object_type::Drum) { setCollision(x + 1, y + 1, 0); }
    if (object.type == object_type::ToxicTank || object.type == object_type::ToxicTankLow) {
        spawnObject(object_type::FloorFlame, object.x + rules::BreakFlameOffsetX, object.y);
        sounds.push_back(sound::TankBurst);
    } else if (object.type != object_type::Crate && x + 1 < level.width && collisionAt(x + 1, y + 1) == 0) {
        patchWall(x, y + 1); patchFloor(x, y);
    }
    addProjectile(rules::SmokeProjectile, object.x + rules::BreakEffectOffsetX, object.y + rules::BreakSmokeOffsetY, 0, 1, false);
    if (object.type != object_type::Crate) {
        addProjectile(rules::ExplosionProjectile, object.x + rules::BreakEffectOffsetX, object.y + rules::BreakBlastOffsetY, 0, 0, false);
        const int firstX = object.x + randomBits(2);
        const int firstY = object.y + rules::BreakBlastSpreadY + randomBits(4);
        addProjectile(rules::ExplosionProjectile, firstX, firstY, 0, 0, false);
        const int secondX = object.x + TileSize - randomBits(2);
        const int secondY = object.y + rules::BreakBlastSpreadY + randomBits(4);
        addProjectile(rules::ExplosionProjectile, secondX, secondY, 0, 0, false);
    } else if (tileAt(x, y) == tile_id::Floor && randomBits(2) < 2) { spawnObject(object_type::ScorePickup, x * TileSize, y * TileSize); }
    switch (object.type) {
    case object_type::GasCylinder:
        projectiles.back().radius = rules::GasCylinderBlastRadius;
        [[fallthrough]];
    case object_type::Barrel: case object_type::CrushedBarrel: case object_type::ToxicTank:
        setCollision(x, y + 2, 0);
        [[fallthrough]];
    case object_type::Drum: case object_type::ToxicTankLow:
        setCollision(x, y + 1, 0);
        break;
    case object_type::Crate: {
        const int tile = tileAt(x, y);
        int support = 0;
        if (tile >= tile_id::Floor && tile <= nextFloorTile_) { support = rules::CrateFloorSupport; }
        else {
            for (int previous = index + 1; previous < static_cast<int>(objects.size()); ++previous) {
                const auto& lower = objects[previous];
                if (lower.active && isBreakable(lower.type)) {
                    support = std::clamp(lower.y - lower.cellY * TileSize, 0, TileSize);
                    break;
                }
            }
        }
        setCollision(x, y, support);
        for (int row = y + 1; row <= y + 1 + object.stackCount; ++row) { setCollision(x, row, 0); }
        for (int piece = 0; piece < rules::CrateDebrisPieces; ++piece) {
            const int horizontal = randomBits(32) % rules::CrateDebrisSpreadX;
            const int vertical = randomBits(31) % rules::CrateDebrisSpreadY;
            addProjectile(rules::DebrisProjectile, projectile.x, projectile.y, horizontal, vertical, false);
            projectiles.back().variant = 1; --projectiles.back().life;
        }
        // Crates stacked above collapse with the destroyed one unless they rest on floor.
        int remaining = object.stackCount;
        for (int upper = index - 1; upper >= 0 && remaining > 0; --upper) {
            auto& crate = objects[upper];
            if (!crate.active || !isBreakable(crate.type)) { continue; }
            if (crate.type != object_type::Crate) { break; }
            const int base = tileAt(crate.cellX, crate.cellY);
            if (base >= tile_id::Floor && base <= nextFloorTile_) { break; }
            crate.active = false; --remaining;
        }
        player.supportY = -1;
        break;
    }
    }
}
}
