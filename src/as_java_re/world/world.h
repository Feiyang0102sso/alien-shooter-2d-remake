/** @file world.h
 * @brief Reproduce original discrete movement, combat, enemies, and mechanisms.
 */
#pragma once
#include "core/assets.h"
#include "world/cheat.h"
#include <array>
#include <cstdint>
#include <deque>
#include <limits>

namespace alien {
struct Input {
    bool left = false, right = false, jump = false, down = false, fire = false;
    int weapon = -1, cycleWeapon = 0;
};
// Render-only marker: no position from the previous tick, so draw without interpolation.
inline constexpr int NoPreviousPosition = std::numeric_limits<int>::min();

struct Actor {
    int type = 0, x = 0, y = 0, health = rules::MaxHealth, direction = 1;
    int velocityY = 0, supportY = -1, frame = 0, state = 0;
    int attackStarted = -10000, shotType = 0;
    int source = -1; // Generator parameter slot at spawn time, not an index into objects.
    int spawnTime = -10000;
    int behavior = 0;
    // Render-only position before the latest tick; never read by world logic.
    int previousX = NoPreviousPosition, previousY = 0;
    bool alive = true;
};
struct Object {
    int id = 0, type = 0, x = 0, y = 0, health = 0;
    int direction = 1, state = 0, variant = 0, terminalGroup = -1;
    int countdown = 0;
    int cellX = 0, cellY = 0, baselineY = 0, motionFrame = 0, stackCount = 0;
    int animationFrame = 0, doorOffset = 0;
    int visualFrame = 0, visualCounter = 0, lightFloor = 0;
    std::array<int, 4> lightArcs{};
    int previousX = NoPreviousPosition, previousY = 0;
    bool active = true;
};
struct Projectile {
    int type = 0, x = 0, y = 0, dx = 0, dy = 0;
    int remainingX = 240, remainingY = 240, damage = 0, life = rules::DefaultProjectileLife;
    int originalDirection = 0, radius = rules::ExplosionRadius, variant = 0;
    // 0: player; 1: enemy; 2: player explosion that hit an enemy, excluding self damage.
    int owner = 0;
    int previousX = NoPreviousPosition, previousY = 0;
    bool alive = true;
};
struct Inventory {
    std::array<bool, rules::WeaponCount> owned{true, false, false, false, false};
    std::array<int, rules::WeaponCount> ammo{rules::PistolAmmo, 0, 0, 0, 0};
    int weapon = 0, armor = 0, explosives = 0, score = 0;
    unsigned int tutorialFlags = 0;
};

// Original coordinates: bottom-left origin, integer pixels, and 80ms ticks.
class World {
public:
    explicit World(const Assets& assets);
    void load(int levelNumber, int difficulty, const Inventory* carry = nullptr);
    void seedRandom(std::uint64_t seed);
    void tick(const Input& input);
    void dismissMessage();
    void prepareMessage();
    void beginDeathEffect();
    bool advanceDeathEffect();
    void selectWeapon(int weapon);
    void cycleWeapon(int delta);
    int collisionAt(int cellX, int cellY) const;
    int tileAt(int cellX, int cellY) const;
    int actorHeight(const Actor& actor) const;
    int activeEnemies() const;
    int activeSources() const;
    void damagePlayer(int amount);
    void addProjectile(int type, int x, int y, int direction, int vertical, bool hostile);
    bool inCameraTransit() const { return cameraTransit_; }
    bool advanceCameraTransition(bool downHeld);

    Level level;
    Actor player;
    std::vector<Actor> enemies;
    std::deque<Object> objects;
    std::deque<Projectile> projectiles;
    Inventory inventory;
    Cheat cheat;
    std::vector<int> sounds;
    std::vector<std::string> message;
    int cameraX = 0, cameraBottom = 0, timeMs = 0, ticks = 0;
    // Render-only camera before the latest tick.
    int previousCameraX = NoPreviousPosition, previousCameraBottom = 0;
    int crouch = 0, shake = 0, healthFlash = 0, airState = 0, playerLight = 0;
    int drawOffsetY = 0;
    int flashOuterColor = -1, flashInnerColor = -1, ammoPanelY = 0;
    bool scoreVisible = false, explosiveVisible = false;
    bool cleared = false, complete = false, power = false, lightsOn = false, dead = false;
    bool deathWipeActive = false;
    std::array<int, ScreenWidth / rules::DeathWipeColumnWidth> deathColumns{};
    std::array<std::array<int, 2>, tile_id::IdLimit> compositeTiles{};
private:
    // Original generator parameters retain initial reverse order after scene generators are destroyed.
    struct GeneratorState {
        int limit = 0, cursor = 0, lastSpawn = -5000;
        std::array<int, 4> types{};
    };
    void moveActor(Actor& actor, int direction, bool airborne = false);
    void gravity(Actor& actor);
    void applyFallDamage();
    void updatePlayer(const Input& input);
    void applyPlayerInput(const Input& input);
    void updateActorHeights();
    int frameHeight(const Actor& actor) const;
    const Animation& animationFor(const Actor& actor) const;
    void updateVisuals();
    void setLighting(bool enabled, bool flicker);
    void beginHealthFlash(bool healing);
    void updateEnemies();
    int updateObjects();
    void updateProjectiles();
    void updateCamera(bool lookingDown);
    void rememberPositions();
    void startAnimation(Actor& actor, int state);
    bool advanceAnimation(Actor& actor);
    void firePlayer();
    void spawnEnemy(int type, int x, int y, int generatorObjectIndex, int generatorSlot = -1);
    void destroyObject(int index, const Projectile& projectile);
    void damageEnemy(Actor& actor, int damage, int impactY, int incomingX);
    void impactProjectile(Projectile& projectile, int left, int right);
    void spawnBlood(int x, int y, bool fatal);
    Object& spawnObject(int type, int x, int y);
    void setCollision(int x, int y, int height);
    void setTile(int x, int y, int tile);
    void showMessage(const std::string& key);
    void showTutorial(int bit, const std::string& key);
    void initializeCollision(const Object& object);
    int randomBits(int bits);
    int findDecorationFloor(int x);
    int compositeTile(int base, int overlay);
    void decorateCell(int x, int y, int overlay);
    void decorateCorpse(int type, int x, int y, int direction);
    void patchWall(int x, int y);
    void patchFloor(int x, int y);
    void decorateLevel();
    void updateTerminal(Object& object, int index, bool overlap);
    bool overlapsPlayer(const Object& object) const;
    bool projectileHits(const Projectile& projectile, int nextX, int nextY, int x, int y, const EntityParameters& p) const;
    const Assets& assets_;
    // Animations resolved once by name: player poses, player weapons, and enemy types 1-5.
    std::array<Animation, rules::PoseCount> idleAnimations_{}, walkAnimations_{};
    std::array<Animation, rules::WeaponCount> attackAnimations_{};
    std::array<Animation, rules::MaxEnemyType + 1> enemyWalkAnimations_{}, enemyAttackAnimations_{};
    std::vector<GeneratorState> generators_;
    std::array<int, rules::MaxEnemyType + 1> enemyHealth_{};
    std::array<int, rules::MaxEnemyType + 1> collisionHeights_{};
    std::uint64_t randomState_ = DefaultRandomSeed;
    bool continuousRandom_ = false;
    int nextFloorTile_ = tile_id::CompositeFirst, nextWallTile_ = tile_id::CompositeLast;
    int difficulty_ = rules::DefaultDifficulty, bob_ = rules::RecoilBobFrames, riding_ = -1, selectedTerminal_ = -1;
    // Platforms and teleports override the foot row until the next vertical update.
    int playerFootRowOverride_ = -1;
    std::array<int, 4> playerBounds_{};
    int flashColor_ = 0, flashStep_ = 0, scoreFlash_ = rules::ScoreFlashTicks;
    int displayedWeapon_ = 0, weaponPanelY_ = 0, weaponPanelSpeed_ = 0, weaponPanelHold_ = 0;
    bool explosiveBlink_ = false;
    bool lookingDown_ = false, cameraTransit_ = false;
    int deathParticleSteps_ = 0;
    bool deathWipeDone_ = false;
};
}
