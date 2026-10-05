/** @file renderer.h
 * @brief Draw original sprites, terrain, menus, text, and effects.
 */
#pragma once
#include "core/assets.h"
#include "world/world.h"
#include <raylib.h>
#include <map>

namespace alien {
inline constexpr Color PixelGreen{0, 255, 0, 255};
struct MenuLayout {
    int left = layout::MenuLeft, top = layout::MenuTop, width = layout::MenuWidth, height = layout::MenuHeight, firstItem = 0;
};
MenuLayout menuLayout(int count, bool hasTitle, bool overlay);
float messageDuration(const std::vector<std::string>& lines);

class Renderer {
public:
    explicit Renderer(const Assets& assets);
    ~Renderer();
    void sprite(const Sprite& image, int x, int y, bool offset = true, Color tint = WHITE) const;
    void image(const std::string& name, int x, int y) const;
    void part(int id, int x, int y, int direction = 1) const;
    void tile(int theme, int id, int x, int y, bool offset = false) const;
    void actor(const Actor& actor, const World& world) const;
    void world(const World& world, bool collisionDebug = false, bool showHealth = true) const;
    void deathWipe(const World& world) const;
    void soundPrompt(bool visible) const;
    void text(const std::string& value, int x, int y, int alignment = 0, Color tint = WHITE, int spacing = 0) const;
    int textWidth(const std::string& value) const;
    void menu(const std::string& title, const std::vector<std::string>& items, int selected,
              const std::vector<bool>& enabled, bool overlay = false, int phase = 0, int spread = 0) const;
    void message(const std::vector<std::string>& lines, bool failure = false, float age = 1000.0f) const;
    int richPage(const std::string& contents, int scroll) const;
    // Fraction of the next tick already elapsed; 1 draws the current tick exactly (tests, menus).
    void setBlend(float blend) { blend_ = blend; }
private:
    int blended(int previous, int current) const;
    int viewCameraX(const World& world) const;
    int viewCameraBottom(const World& world) const;
    void object(const Object& object, const World& world) const;
    void objectLayers(const ObjectVisual& visual, const Object& object, int theme, int x, int y, int baseline) const;
    void generatorEffects(const Object& object, const World& world, int x, int y) const;
    void switchLasers(const Object& object, int x, int y) const;
    void terminalEffects(const Object& object, int theme, int x, int y) const;
    void projectile(const Projectile& projectile, const World& world) const;
    void hud(const World& world, bool showHealth) const;
    const Assets& assets_;
    std::map<std::string, Texture2D> textures_;
    float blend_ = 1.0f;
};
}
