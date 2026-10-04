/** @file renderer.h
 * @brief Draw original sprites, terrain, menus, text, and effects.
 */
#pragma once
#include "assets.h"
#include "world.h"
#include <raylib.h>
#include <map>

namespace alien {
inline constexpr Color PixelGreen{0, 255, 0, 255};
struct MenuLayout {
    int left = 4, top = 82, width = 162, height = 224, firstItem = 0;
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
private:
    void object(const Object& object, const World& world) const;
    void hud(const World& world, bool showHealth) const;
    const Assets& assets_;
    std::map<std::string, Texture2D> textures_;
};
}
