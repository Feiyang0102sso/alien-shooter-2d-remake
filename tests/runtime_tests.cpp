/** @file runtime_tests.cpp
 * @brief Verify runtime tests against runtime and original-game contracts.
 */
#include "assets.h"
#include "midi.h"
#include "world.h"
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}
}
int main(int argc, char** argv) {
    try {
        require(argc == 5, "Expected asset root, player, level and combat fixtures.");
        alien::Assets assets(argv[1]);
        assets.load();
        const std::array<int, 17> notes{364, 25, 76, 9, 10, 49, 36, 54, 31, 27, 428, 595, 70, 70, 85, 300, 926};
        for (int id = 0; id < 17; ++id) {
            const auto midi = alien::MidiSequence::load(assets.root() / assets.midiPaths[id]);
            require(midi.noteOnCount == notes[id], "MIDI note count mismatch: " + std::to_string(id));
            require(midi.endSample > 0, "MIDI duration must be positive.");
            std::uint64_t previous = 0;
            for (const auto& event : midi.events) { require(event.sample >= previous, "MIDI event order changed."); previous = event.sample; }
            std::cout << "MIDI " << id << ": notes=" << midi.noteOnCount << ", samples=" << midi.endSample << '\n';
        }
        const auto menuMidi = alien::MidiSequence::load(assets.root() / assets.midiPaths[16]);
        require(menuMidi.endSample == 39ULL * alien::AudioSampleRate, "Menu MIDI should be exactly 39 seconds.");
        alien::World world(assets);
        std::ifstream levelFixtureStream(argv[3]);
        const auto levelFixture = alien::Json::parse(levelFixtureStream);
        for (int level = 1; level <= 8; ++level) {
            world.load(level, 1);
            const auto& original = levelFixture["levels"][level - 1];
            require(alien::Json(world.level.tiles) == original["tiles"], "Original tile grid mismatch: level=" + std::to_string(level));
            require(alien::Json(world.level.collision) == original["collision"], "Original collision grid mismatch: level=" + std::to_string(level));
            require(alien::Json::array({world.player.x, world.player.y}) == original["player"], "Original spawn mismatch.");
            require(alien::Json::array({world.cameraX, world.cameraBottom}) == original["camera"], "Original camera mismatch.");
            require(world.player.supportY >= 0, "Player must start on solid ground.");
            require(world.player.x > 0 && world.player.y >= 0, "Player spawn out of bounds.");
            require(world.activeEnemies() <= 29, "Original enemy slot limit exceeded.");
            require(!world.dead && !world.complete, "Invalid initial level state.");
            for (int step = 0; step < 1000; ++step) {
                world.dismissMessage();
                if (world.dead) { break; }
                alien::Input input;
                input.right = step % 80 < 40;
                input.left = !input.right;
                input.jump = step % 31 == 0;
                input.fire = step % 3 == 0;
                input.down = step % 17 == 0;
                world.tick(input);
                require(world.player.frame >= 0 && world.player.frame < 85, "Player animation out of range.");
            }
        }
        world.load(1, 1);
        world.selectWeapon(4);
        require(world.inventory.weapon == 0, "Unowned weapon must remain locked.");
        world.inventory.owned[1] = true;
        world.cycleWeapon(1);
        require(world.inventory.weapon == 1, "Weapon cycling failed.");
        world.cycleWeapon(-1);
        require(world.inventory.weapon == 0, "Reverse weapon cycling failed.");
        world.inventory.armor = 1;
        world.damagePlayer(16);
        require(world.player.health == 95, "Light armor must use original 5/16 damage formula.");
        world.inventory.armor = 2;
        world.damagePlayer(10);
        require(world.player.health == 90, "Heavy armor must halve damage.");
        std::ifstream fixtureStream(argv[2]);
        const auto fixture = alien::Json::parse(fixtureStream);
        for (auto scenario = fixture["scenarios"].begin(); scenario != fixture["scenarios"].end(); ++scenario) {
            world.load(1, 1);
            int tick = 0;
            for (const auto& expected : scenario.value()) {
                alien::Input input;
                if (scenario.key() == "walk") { input.right = true; }
                if (scenario.key() == "jump" && tick == 0) { input.jump = true; }
                if (scenario.key() == "fire") { input.fire = true; }
                if (scenario.key() == "crouch") { input.down = true; }
                if (scenario.key() == "turn") { input.right = tick < 5; input.left = tick >= 5; }
                world.dismissMessage(); world.tick(input);
                const auto& player = world.player;
                const alien::Json actual{{"x", player.x}, {"y", player.y}, {"vy", player.velocityY}, {"ground", player.supportY},
                                         {"state", player.state}, {"frame", player.frame}, {"air", world.airState},
                                         {"crouch", world.crouch}, {"camera", {world.cameraX, world.cameraBottom}}};
                require(actual == expected, "Original player mismatch: " + scenario.key() + " tick=" + std::to_string(tick)
                        + " expected=" + expected.dump() + " actual=" + actual.dump());
                ++tick;
            }
            std::cout << "Original trace matched: " << scenario.key() << " (" << tick << " ticks)\n";
        }
        std::ifstream combatStream(argv[4]);
        const auto combatFixture = alien::Json::parse(combatStream);
        for (auto scenario = combatFixture["scenarios"].begin(); scenario != combatFixture["scenarios"].end(); ++scenario) {
            world.load(6, 1);
            const bool firing = scenario.key().rfind("weapon", 0) == 0;
            int type = 5;
            if (!firing) { type = std::stoi(scenario.key().substr(5)); }
            alien::Actor target;
            target.type = type; target.x = world.player.x + 120; target.y = world.player.y;
            target.supportY = target.y; target.direction = -1; target.frame = 26 + type * 10;
            target.health = assets.entities[type].health; target.shotType = assets.entities[type].damage;
            world.enemies.assign(1, target); world.objects.clear();
            if (firing) {
                const int weapon = std::stoi(scenario.key().substr(6));
                world.inventory.owned[weapon] = true;
                world.inventory.ammo[weapon] = assets.projectiles[weapon].capacity;
                world.selectWeapon(weapon);
            }
            int tick = 0;
            for (const auto& expected : scenario.value()) {
                world.dismissMessage();
                alien::Input input; input.fire = firing;
                world.tick(input);
                const auto& player = world.player;
                const auto& enemy = world.enemies[0];
                alien::Json shots = alien::Json::array();
                for (auto shot = world.projectiles.rbegin(); shot != world.projectiles.rend(); ++shot) {
                    int visual = shot->originalDirection;
                    if (shot->type == 7 || shot->type == 8) { visual = shot->variant; }
                    shots.push_back({shot->type, shot->x, shot->y, shot->dx, shot->dy,
                                     shot->remainingX, shot->remainingY, shot->damage, shot->radius, visual, shot->life, shot->owner});
                }
                const alien::Json actual{
                    {"player", {player.x, player.y, player.health, player.state, player.frame}},
                    {"enemy", {enemy.x, enemy.y, enemy.health, enemy.direction, enemy.state, enemy.frame, enemy.alive}},
                    {"weapon", world.inventory.weapon}, {"ammo", world.inventory.ammo[world.inventory.weapon]}, {"projectiles", shots}};
                require(actual == expected, "Original combat mismatch: " + scenario.key() + " tick=" + std::to_string(tick)
                        + " expected=" + expected.dump() + " actual=" + actual.dump());
                ++tick;
            }
            std::cout << "Original combat matched: " << scenario.key() << " (" << tick << " ticks)\n";
        }
        std::cout << "All runtime contracts passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
}
