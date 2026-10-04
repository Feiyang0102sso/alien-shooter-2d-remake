/** @file renderer.cpp
 * @brief Draw original sprites, terrain, menus, text, and effects.
 */
#include "renderer.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace alien {
namespace {
Color rgb(unsigned int value) {
    return Color{static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value), 255};
}
void pixelBorder(int x, int y, int width, int height, Color color) {
    // Use integer filled rectangles to avoid OpenGL half-pixel endpoint expansion.
    DrawRectangle(x, y, width, 1, color);
    DrawRectangle(x, y + height - 1, width, 1, color);
    DrawRectangle(x, y + 1, 1, height - 2, color);
    DrawRectangle(x + width - 1, y + 1, 1, height - 2, color);
}
void pixelTriangle(int x, int y, int width, int direction, Color color) {
    while (width > 0) {
        DrawRectangle(x, y, width, 1, color);
        ++x; y += direction; width -= 2;
    }
}
void pixelShadow(int x, int y, int width, Color color) {
    // Match the four-pixel MIDP ellipse: empty first row and full middle rows, without smoothing.
    int inset = 1;
    int edgeWidth = width - 1;
    if (width == 15) { inset = 2; edgeWidth = 12; }
    DrawRectangle(x + inset, y + 1, edgeWidth, 1, color);
    DrawRectangle(x, y + 2, width, 1, color);
    DrawRectangle(x + inset, y + 3, edgeWidth, 1, color);
}
void pixelLine(int x, int y, int endX, int endY, Color color) {
    // MIDP lines include both endpoints, unlike OpenGL line rasterization.
    const int width = std::abs(endX - x), height = -std::abs(endY - y);
    int stepX = 1, stepY = 1;
    if (x > endX) { stepX = -1; }
    if (y > endY) { stepY = -1; }
    int error = width + height;
    while (true) {
        DrawPixel(x, y, color);
        if (x == endX && y == endY) { break; }
        const int twiceError = error * 2;
        if (twiceError >= height) { error += height; x += stepX; }
        if (twiceError <= width) { error += width; y += stepY; }
    }
}
std::string padded(int value) {
    std::ostringstream result;
    result << std::setw(4) << std::setfill('0') << value;
    return result.str();
}
int glyphId(unsigned char character) {
    if (character >= 'a' && character <= 'z') { character -= 32; }
    if (character < 32 || character >= 132) { return 0; }
    return character - 32;
}
}
MenuLayout menuLayout(int count, bool hasTitle, bool overlay) {
    MenuLayout layout;
    if (overlay) { layout.left = 75; layout.top = 118; layout.width = 90; layout.height = 84; }
    int titleHeight = 0;
    if (hasTitle) { titleHeight = 12; }
    const int available = layout.height - titleHeight;
    const int used = 2 + count * 12;
    layout.firstItem = layout.top + titleHeight + (available - used) / 2 + 2;
    return layout;
}
Renderer::Renderer(const Assets& assets) : assets_(assets) {
    for (const std::string name : {"textures/characters.png", "textures/world.png", "textures/interface.png", "fonts/pixel_green.png"}) {
        const auto path = (assets.root() / name).u8string();
        auto texture = LoadTexture(path.c_str());
        if (!IsTextureValid(texture)) { throw std::runtime_error("Cannot load texture: " + path); }
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
        textures_.emplace(name, texture);
    }
}
Renderer::~Renderer() { for (auto& entry : textures_) { UnloadTexture(entry.second); } }

void Renderer::sprite(const Sprite& image, int x, int y, bool offset, Color tint) const {
    if (image.texture.empty() || image.width == 0 || image.height == 0) { return; }
    if (offset) { x += image.offsetX; y += image.offsetY; }
    Rectangle source{static_cast<float>(image.x), static_cast<float>(image.y), static_cast<float>(image.width), static_cast<float>(image.height)};
    DrawTextureRec(textures_.at(image.texture), source, Vector2{static_cast<float>(x), static_cast<float>(y)}, tint);
}
void Renderer::image(const std::string& name, int x, int y) const { sprite(readSprite(assets_.interface["images"].at(name)), x, y); }
void Renderer::part(int id, int x, int y, int direction) const {
    if (id <= 0 || id >= 155) { return; }
    int side = 0;
    if (direction < 0) { side = 1; }
    sprite(assets_.parts[id][side], x, y);
}
void Renderer::tile(int theme, int id, int x, int y, bool offset) const {
    if (id <= 0 || id >= 220) { return; }
    const auto& tile = assets_.tiles[theme][id];
    if (!tile.image.texture.empty()) { sprite(tile.image, x, y, offset); }
    else if (tile.base > 0 && tile.overlay > 0) {
        sprite(assets_.tiles[theme][tile.base].image, x, y, offset);
        sprite(assets_.tiles[theme][tile.overlay].image, x, y, true);
    }
}
int Renderer::textWidth(const std::string& value) const {
    int width = 0;
    for (unsigned char character : value) { width += assets_.glyphs[glyphId(character)].advance; }
    if (!value.empty()) { --width; }
    return width;
}
void Renderer::text(const std::string& value, int x, int y, int alignment, Color tint, int spacing) const {
    int width = textWidth(value);
    if (!value.empty()) { width += spacing * (static_cast<int>(value.size()) - 1); }
    if (alignment == 1) { x -= width / 2; }
    if (alignment == 2) { x -= width; }
    for (unsigned char character : value) {
        const auto& glyph = assets_.glyphs[glyphId(character)];
        sprite(glyph.image, x, y + 6 - glyph.image.height, true, tint);
        x += glyph.advance + spacing;
    }
}
void Renderer::actor(const Actor& actor, const World& world) const {
    if (!actor.alive) { return; }
    if (actor.direction == 0 && (actor.behavior & 1) == 0) { return; }
    auto slots = assets_.frames.at(actor.frame);
    int y = ScreenHeight - actor.y + world.cameraBottom + world.drawOffsetY;
    if (actor.type == 0) {
        if (world.inventory.weapon >= 2) {
            for (const auto& pose : assets_.poses["weapon_frames"]) {
                if (pose["weapon_id"] == world.inventory.weapon && pose["frame_id"] == actor.frame) {
                    slots = pose["part_slots"].get<std::array<int, 8>>(); break;
                }
            }
        }
        if (world.crouch > 0 && (actor.frame <= 2 || (actor.frame >= 15 && actor.frame <= 18))) { slots[0] = 35; }
        y += world.crouch;
        if (world.inventory.armor > 0) {
            for (int slot = 1; slot <= 3; ++slot) {
                if (slots[slot] == 5) { slots[slot + 1] = 32 + world.inventory.armor; break; }
            }
        }
    }
    for (int id : slots) { part(id, actor.x - world.cameraX, y, actor.direction); }
    if (actor.type == 0 && world.playerLight != 0) { part(world.playerLight, actor.x - world.cameraX, y, actor.direction); }
    if (actor.type == 5) { part(148 + actor.shotType, actor.x - world.cameraX, y, actor.direction); }
}

void Renderer::object(const Object& object, const World& world) const {
    if (!object.active || object.type >= 100) { return; }
    const auto& p = assets_.entities.at(object.type);
    if (object.x + p.right < world.cameraX - 16 || object.x - p.left > world.cameraX + ScreenWidth + 16
        || object.y + p.top < world.cameraBottom || object.y - p.bottom > world.cameraBottom + ScreenHeight) { return; }
    const int x = object.x - world.cameraX - p.left;
    const int y = ScreenHeight - object.y + world.cameraBottom + world.drawOffsetY;
    const int theme = world.level.theme;
    const int baseline = y + object.y - object.baselineY;
    const Color shadow = rgb(3158064);
    switch (object.type) {
    case 10:
        tile(theme, 9 + object.variant, x, y, true);
        pixelShadow(x + 5, baseline - 11, 15, shadow); break;
    case 14:
        part(96 + object.variant, x, y);
        pixelShadow(x + 1, baseline - 11, 10, shadow); break;
    case 18:
        part(96, x, y);
        pixelShadow(x + 2, baseline - 11, 8, shadow); break;
    case 19: tile(theme, 19, x, y, true); break;
    case 20:
        part(95 - object.visualFrame, x, y);
        pixelShadow(x, y - 11, 8, shadow); break;
    case 21:
        part(32 + object.variant, x + 10, y + 10);
        pixelShadow(x + 2, baseline - 11, 10, shadow); break;
    case 23: tile(theme, 18, x, y, true); break;
    case 24: for (int id = 38; id <= 44; ++id) { tile(theme, id, x, y, true); } break;
    case 25:
        for (int id = 32; id <= 37; ++id) { tile(theme, id, x, y, true); }
        for (int line = 0; line < 3; ++line) {
            const std::array<unsigned int, 3> colors{0x80, 0xb0, 0xff};
            Color light = rgb(colors[object.visualFrame] << 16);
            if (world.power) { light = rgb(colors[object.visualFrame] << 8); }
            pixelLine(x + 12, y - 19 - line * 2, x + 17, y - 19 - line * 2, light);
        }
        if (world.power) {
            Color first = rgb(0x0080ff), second = rgb(0x00ccff);
            if (object.visualFrame == 0) { first = rgb(0x00ccff); second = rgb(0x0080ff); }
            const auto& arcs = object.lightArcs;
            pixelLine(x + 11 - arcs[0], y - 50 - arcs[0], x + 18 + arcs[1], y - 50 - arcs[1], first);
            pixelLine(x + 4 + arcs[0], y - 57 + arcs[0], x + 25 - arcs[1], y - 57 + arcs[1], second);
            pixelLine(x + 11 - arcs[2], y - 50 - arcs[2], x + 18 + arcs[3], y - 50 - arcs[3], second);
        }
        break;
    case 26: for (int id = 84; id <= 86; ++id) { tile(theme, id, x, y, true); } break;
    case 27: case 28:
        tile(theme, 87 + (object.type - 27) * 2, x, y, true);
        tile(theme, 88 + (object.type - 27) * 2, x, y, true);
        tile(theme, 97, x + 18, y - 14, true); break;
    case 29: for (int id = 98; id <= 101; ++id) { tile(theme, id, x, y, true); } break;
    case 30: for (int id = 91; id <= 95; ++id) { tile(theme, id, x, y, true); } break;
    case 31:
        tile(theme, 91, x, y + 16, true);
        for (int id = 93; id <= 95; ++id) { tile(theme, id, x, y, true); }
        break;
    case 32: tile(theme, 96, x, y, true); break;
    case 33: for (int id = 69; id <= 73; ++id) { tile(theme, id, x, baseline, true); } break;
    case 34: case 35: tile(theme, 20, x, y, true); tile(theme, 21, x, y, true); break;
    case 36:
        tile(theme, 63, x, baseline, true);
        if (object.state < 2) { tile(theme, 64, x, baseline, true); }
        if (object.state == 0) { tile(theme, 18, x, y, true); }
        if (object.state == 1) { tile(theme, 19, x, baseline, true); text(std::to_string(object.animationFrame), x + 4, y - 38); }
        break;
    case 37:
        tile(theme, 74, x, baseline, true); tile(theme, 75, x, baseline, true);
        if (object.state == 0) { tile(theme, 18, x, y, true); }
        if (object.state == 1) { tile(theme, 19, x, baseline, true); text(std::to_string(object.animationFrame), x + 4, y - 38); }
        break;
    case 38:
        for (int row = 0; row < 3; ++row) {
            pixelLine(x + object.animationFrame - 2 + row, y - 11 + row,
                     x + object.animationFrame - 2 + row, y - 6 - row, rgb(16711680));
            pixelLine(x + 33 - object.animationFrame - row, y - 11 + row,
                     x + 33 - object.animationFrame - row, y - 6 - row, rgb(16711680));
        }
        break;
    case 40: part(89 + object.visualFrame, x, y); break;
    case 41: {
        tile(theme, 47 + object.variant, x - 6, y, true);
        if (object.terminalGroup == 0) { text("EXIT", x + 10, y - 62, 1); }
        if (object.animationFrame != 0) { pixelTriangle(x + 30, y - 44 + object.animationFrame, 7, 1, PixelGreen); }
        BeginScissorMode(x - 6, y - 48, 32, 48);
        const int doorY = y + object.doorOffset;
        if (object.variant != 2) { for (int id = 25; id <= 30; ++id) { tile(theme, id, x - 6, doorY, true); } }
        if (object.terminalGroup > 0) { tile(theme, 80 + object.terminalGroup, x + 2, doorY, true); }
        EndScissorMode();
        break;
    }
    }
}

void Renderer::hud(const World& world, bool showHealth) const {
    const auto& inv = world.inventory;
    const Color panel = rgb(2631728), border = rgb(10067347);
    const int offsetY = world.drawOffsetY;
    const int ammoY = world.ammoPanelY + offsetY;
    DrawRectangle(0, ammoY, 51, 14, panel); pixelBorder(0, ammoY, 51, 14, border);
    const int capacity = assets_.projectiles[inv.weapon].capacity;
    const int bar = std::clamp(inv.ammo[inv.weapon] * 16 / capacity, 0, 16);
    unsigned int barColor = 65280;
    if (bar <= 5) { barColor = 16742144; }
    DrawRectangle(3, ammoY + 4, bar, 6, rgb(barColor));
    for (int column = 4; column < 4 + bar; column += 2) { pixelLine(column, ammoY + 4, column, ammoY + 9, rgb(barColor & 9408399)); }
    text(padded(inv.ammo[inv.weapon]), 21, ammoY + 4);
    if (world.ammoPanelY != 0 && inv.weapon > 0) {
        DrawRectangle(0, ammoY - 20, 51, 21, panel); pixelBorder(0, ammoY - 20, 51, 21, border);
        const auto& weapon = assets_.tiles[world.level.theme][9 + inv.weapon].image;
        sprite(weapon, 25 - weapon.width / 2, ammoY - 19, false);
    }
    DrawRectangle(ScreenWidth - 46, offsetY, 46, 14, panel);
    pixelBorder(ScreenWidth - 46, offsetY, 46, 14, border);
    sprite(assets_.parts[96][0], ScreenWidth - 43, 2 + offsetY, false);
    if (world.scoreVisible) { text(padded(inv.score), ScreenWidth - 3, 4 + offsetY, 2); }
    if (showHealth) {
        const auto& healthPanel = assets_.tiles[world.level.theme][9].image;
        sprite(healthPanel, 0, ScreenHeight - healthPanel.height + offsetY, false);
        text(std::to_string(world.player.health), 16, ScreenHeight - 17 + offsetY, 1);
        if (inv.weapon > 0) {
            const auto& ammo = assets_.parts[inv.weapon + 96][0];
            sprite(ammo, 40 - ammo.width / 2, ScreenHeight - 5 - ammo.height + offsetY, false);
        }
    }
    if (world.explosiveVisible) {
        BeginScissorMode(0, 15 + offsetY, ScreenWidth, 12);
        for (int item = 0; item < inv.explosives; ++item) {
            sprite(assets_.tiles[world.level.theme][18].image, ScreenWidth - 14 - item * 14, 15 + offsetY, false);
        }
        EndScissorMode();
    }
    if (showHealth && world.flashOuterColor >= 0) {
        pixelBorder(0, offsetY, ScreenWidth, ScreenHeight, rgb(world.flashOuterColor));
        pixelBorder(1, 1 + offsetY, ScreenWidth - 2, ScreenHeight - 2, rgb(world.flashInnerColor));
    }
}

void Renderer::deathWipe(const World& world) const {
    if (!world.deathWipeActive) { return; }
    for (int column = 0; column < static_cast<int>(world.deathColumns.size()); ++column) {
        const int center = world.deathColumns[column];
        int left = center, right = center;
        if (column > 0 && column < static_cast<int>(world.deathColumns.size()) - 2) {
            left = center - 2; right = center - 2;
            if (world.deathColumns[column - 1] > center) { left = center + 2; }
            if (world.deathColumns[column + 1] > center) { right = center + 2; }
        }
        const int x = 2 + column * 3;
        pixelLine(x - 1, 0, x - 1, left, rgb(8388608));
        pixelLine(x, 0, x, center, rgb(8388608));
        pixelLine(x + 1, 0, x + 1, right, rgb(8388608));
    }
}
void Renderer::world(const World& world, bool collisionDebug, bool showHealth) const {
    // Do not clear the original canvas; shaken edges retain the preceding frame's pixels.
    const int firstRow = std::max(0, world.cameraBottom / 16);
    const int lastRow = std::min(world.level.height - 1, (world.cameraBottom + ScreenHeight) / 16 + 1);
    const int firstColumn = std::max(0, world.cameraX / 16);
    const int lastColumn = std::min(world.level.width - 1, (world.cameraX + ScreenWidth) / 16 + 1);
    for (int row = lastRow; row >= firstRow; --row) {
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const int x = column * 16 - world.cameraX;
            const int y = ScreenHeight - (row + 1) * 16 + world.cameraBottom + world.drawOffsetY;
            const int id = world.level.tiles[row][column];
            if (id >= 170) {
                const auto& composition = world.compositeTiles[id];
                if (composition[0] == 0 || composition[1] == 0) { DrawRectangle(x, y, 16, 16, BLACK); }
                else {
                    tile(world.level.theme, composition[0], x, y);
                    tile(world.level.theme, composition[1], x, y, true);
                }
            } else if (id == 0) { DrawRectangle(x, y, 16, 16, BLACK); }
            else { tile(world.level.theme, id, x, y); }
            if (collisionDebug && world.level.collision[row][column] > 0) {
                const int height = world.level.collision[row][column];
                DrawRectangle(x, y + 16 - height, 16, height, Fade(RED, 0.35f));
            }
        }
    }
    for (auto iterator = world.objects.rbegin(); iterator != world.objects.rend(); ++iterator) {
        if (iterator->type < 26 || iterator->type > 32) { object(*iterator, world); }
    }
    for (auto iterator = world.objects.rbegin(); iterator != world.objects.rend(); ++iterator) {
        if (iterator->type >= 26 && iterator->type <= 32) { object(*iterator, world); }
    }
    actor(world.player, world);
    for (auto iterator = world.enemies.rbegin(); iterator != world.enemies.rend(); ++iterator) { actor(*iterator, world); }
    for (auto iterator = world.projectiles.rbegin(); iterator != world.projectiles.rend(); ++iterator) {
        const auto& projectile = *iterator;
        const int x = projectile.x - world.cameraX;
        const int y = ScreenHeight - projectile.y + world.cameraBottom;
        if (x + 20 < 0 || x - 20 > ScreenWidth || y + 20 < 0 || y - 20 > ScreenHeight) { continue; }
        int id = 53;
        int direction = 1;
        if (projectile.dx < 0) { direction = -1; }
        if (projectile.type == 2 || projectile.type == 5) { id = 77 - projectile.life; direction = 1; }
        if (projectile.type == 3) { id = 54 + projectile.life; }
        if (projectile.type == 4) { id = 56; }
        if (projectile.type == 6) {
            id = 60 - projectile.life; direction = 1;
            if (projectile.originalDirection > 0) { direction = -1; }
        }
        if (projectile.type == 7) { id = 68 + projectile.variant * 3 - projectile.life; direction = 1; }
        if (projectile.type == 8) { id = 85 + projectile.variant * 3 - projectile.life; direction = 1; }
        if (projectile.type == 9) { id = 82 - projectile.life; direction = 1; }
        part(id, x, y + world.drawOffsetY, direction);
    }
    hud(world, showHealth);
}

void Renderer::soundPrompt(bool visible) const {
    ClearBackground(BLACK);
    image("menu_background", 0, 0);
    if (visible) { text("ENABLE", 120, 152, 1); text("SOUND?", 120, 161, 1); }
    DrawRectangle(0, 310, ScreenWidth, 10, BLACK);
    text("YES", 2, 312); text("NO", 238, 312, 2);
}

void Renderer::menu(const std::string& title, const std::vector<std::string>& items, int selected,
                    const std::vector<bool>& enabled, bool overlay, int phase, int spread) const {
    const auto layout = menuLayout(static_cast<int>(items.size()), !title.empty(), overlay);
    const int x = layout.left, y = layout.top, width = layout.width, height = layout.height;
    if (!overlay) { ClearBackground(BLACK); image("menu_background", 0, 0); image("menu_overlay", 4, 4); }
    else { DrawRectangle(x, y, width, height, BLACK); }
    if (!overlay) {
        for (int line = y + 2; line < y + height; line += 2) { pixelLine(x + 2, line, x + width - 1, line, rgb(22566)); }
        pixelBorder(x, y, width, height, PixelGreen);
    } else { pixelBorder(x, y, width + 1, height + 1, PixelGreen); }
    pixelTriangle(x + width / 2 - 2, y - 2, 5, -1, BLACK);
    pixelTriangle(x + width / 2 - 2, y + height + 2, 5, 1, BLACK);
    if (!title.empty()) {
        DrawRectangle(x + 1, y + 1, width - 2, 12, BLACK);
        text(title, x + width / 2, y + 4, 1);
        pixelLine(x, y + 12, x + width - 1, y + 12, PixelGreen);
    }
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int itemY = layout.firstItem + index * 12;
        if (index == selected) {
            DrawRectangle(x + 2, itemY, width - 3, 10, BLACK);
            for (int column = 1; column < width - 1; ++column) {
                const int topShifted = (column + phase) % width;
                const int topIntensity = std::min(topShifted, width - 1 - topShifted) * 255 / (width / 2);
                const int shifted = (column + phase + 2) % width;
                const int bottomIntensity = std::min(shifted, width - 1 - shifted) * 255 / (width / 2);
                const Color topColor{0, static_cast<unsigned char>(topIntensity), 0, 255};
                const Color bottomColor{0, static_cast<unsigned char>(bottomIntensity), 0, 255};
                DrawPixel(x + column, itemY - 2, topColor); DrawPixel(x + column, itemY - 1, topColor);
                DrawPixel(x + column, itemY + 10, bottomColor); DrawPixel(x + column, itemY + 11, bottomColor);
            }
        }
        if (index == selected && spread > 0) {
            const auto split = items[index].size() / 2 + 1;
            BeginScissorMode(x + 2, itemY + 2, width - 4, 6);
            text(items[index].substr(0, split), x + 2 + width / 2 - spread, itemY + 2, 2, WHITE, spread);
            text(items[index].substr(split), x + 2 + width / 2 + spread, itemY + 2, 0, WHITE, spread);
            EndScissorMode();
            continue;
        }
        text(items[index], x + 2 + width / 2, itemY + 2, 1);
        if (index < static_cast<int>(enabled.size()) && !enabled[index]) {
            int firstLine = itemY + 1;
            if (((y & 1) != 0) != (((itemY + 2) & 1) == 0)) { --firstLine; }
            for (int line = firstLine; line <= itemY + 10; line += 2) { pixelLine(x + 1, line, x + width - 2, line, rgb(22566)); }
        }
    }
    DrawRectangle(0, 310, textWidth("BACK") + 4, 10, BLACK);
    pixelBorder(0, 310, textWidth("BACK") + 5, 10, PixelGreen);
    text("SELECT", 2, 312);
    if (!overlay) {
        const int rightWidth = textWidth("SELECT") + 4;
        DrawRectangle(ScreenWidth - rightWidth, 310, rightWidth, 10, BLACK);
        pixelBorder(ScreenWidth - rightWidth - 1, 310, rightWidth + 1, 10, PixelGreen);
        text("BACK", ScreenWidth - 2, 312, 2);
    } else {
        for (int row = 0; row < 3; ++row) { pixelLine(228, 309 - row * 2, 234, 309 - row * 2, PixelGreen); }
        pixelTriangle(226, 311, 11, 1, PixelGreen);
    }
}
float messageDuration(const std::vector<std::string>& lines) {
    int characters = 0;
    for (const auto& line : lines) { characters += static_cast<int>(line.size()); }
    return 0.15f + characters * 0.05f;
}
void Renderer::message(const std::vector<std::string>& lines, bool failure, float age) const {
    Color border = PixelGreen;
    if (failure) { border = rgb(16711680); }
    if (age < 0.15f) {
        const int step = std::min(2, static_cast<int>(age / 0.05f));
        const int halfWidth = 10 + step * 10, halfHeight = 5 + step * 5;
        DrawRectangle(120 - halfWidth, 160 - halfHeight, halfWidth * 2, halfHeight * 2, BLACK);
        pixelBorder(120 - halfWidth, 160 - halfHeight, halfWidth * 2, halfHeight * 2 + 1, border);
        return;
    }
    int width = 40;
    for (const auto& line : lines) { width = std::max(width, textWidth(line)); }
    const int halfHeight = (9 * static_cast<int>(lines.size()) + 7) / 2;
    const int left = 120 - width / 2, top = 160 - halfHeight;
    // Final messages use a full-width black band with independently centered text.
    DrawRectangle(-1, top, 242, halfHeight * 2, BLACK);
    pixelBorder(-1, top, 242, halfHeight * 2 + 1, border);
    int visible = std::max(0, static_cast<int>((age - 0.15f) / 0.05f) + 1);
    for (int row = 0; row < static_cast<int>(lines.size()); ++row) {
        const int count = std::min(visible, static_cast<int>(lines[row].size()));
        const auto fragment = lines[row].substr(0, count);
        text(fragment, left, top + 5 + row * 9);
        if (count > 0 && count < static_cast<int>(lines[row].size())) {
            DrawRectangle(left + textWidth(fragment) + 1, top + 4 + row * 9, 5, 8, PixelGreen);
        }
        visible -= count;
    }
    for (int row = 0; row < 3; ++row) { pixelLine(228, 305 + row * 2, 234, 305 + row * 2, PixelGreen); }
    pixelTriangle(226, 311, 11, 1, PixelGreen);
}

int Renderer::richPage(const std::string& contents, int scroll) const {
    ClearBackground(BLACK);
    DrawRectangle(0, 19, ScreenWidth, 286, rgb(11283));
    image("text_background", 96, 127);
    const auto titleEnd = contents.find('\n');
    const std::string title = contents.substr(0, titleEnd);
    const std::string body = contents.substr(titleEnd + 1);
    BeginScissorMode(10, 19, 220, 286);
    int cursor = 22 - scroll, alignment = 0;
    std::vector<int> lineOffsets;
    std::size_t position = 0;
    while (position <= body.size()) {
        const auto newline = body.find('\n', position);
        std::string line = body.substr(position, newline - position);
        int icon = -1;
        while (!line.empty() && line[0] == '<') {
            const auto end = line.find('>');
            if (end == std::string::npos) { break; }
            const auto tag = line.substr(0, end + 1);
            if (tag == "<align:left>") { alignment = 0; }
            else if (tag == "<align:center>") { alignment = 1; }
            else if (tag == "<align:right>") { alignment = 2; }
            else if (tag.rfind("<img:", 0) == 0) { icon = std::stoi(tag.substr(5)); }
            else { break; }
            line.erase(0, end + 1);
        }
        // Decode entities once; never interpret decoded text as markup again.
        std::string decoded;
        for (std::size_t i = 0; i < line.size();) {
            if (line.compare(i, 5, "&amp;") == 0) { decoded += '&'; i += 5; }
            else if (line.compare(i, 4, "&lt;") == 0) { decoded += '<'; i += 4; }
            else if (line.compare(i, 4, "&gt;") == 0) { decoded += '>'; i += 4; }
            else { decoded += line[i++]; }
        }
        int iconWidth = 0, lineHeight = 6;
        Sprite iconImage;
        if (icon >= 0 && icon < 21) {
            iconImage = readSprite(assets_.interface["help_icons"].at(icon)["image"]);
            iconWidth = iconImage.width + 1; lineHeight = iconImage.height;
        }
        do {
            lineOffsets.push_back(cursor + scroll - 22);
            const int available = 220 - iconWidth;
            std::size_t count = decoded.size();
            while (count > 0 && textWidth(decoded.substr(0, count)) + 1 > available) { --count; }
            if (count < decoded.size()) {
                const auto space = decoded.find_last_of(" ,.", count);
                if (space != std::string::npos && space > 0) { count = space + 1; }
            }
            if (count == 0 && !decoded.empty()) { count = 1; }
            const auto fragment = decoded.substr(0, count);
            const int width = iconWidth + textWidth(fragment) + 1;
            int left = 10;
            if (alignment == 1) { left = ScreenWidth / 2 - width / 2; }
            if (alignment == 2) { left = ScreenWidth - 11 - width; }
            if (cursor + lineHeight <= 302 || (iconWidth > 0 && cursor + 6 <= 302)) {
                if (iconWidth > 0) { sprite(iconImage, left, cursor, false); }
                text(fragment, left + iconWidth, cursor + (lineHeight - 6) / 2);
            }
            cursor += lineHeight + 7;
            decoded.erase(0, count);
            iconWidth = 0; lineHeight = 6;
        } while (!decoded.empty());
        if (newline == std::string::npos) { break; }
        position = newline + 1;
    }
    EndScissorMode();
    // Original help pages have black top and bottom bars with green corner gradients.
    DrawRectangle(0, 0, ScreenWidth, 19, BLACK);
    DrawRectangle(0, 305, ScreenWidth, 15, BLACK);
    for (int column = 0; column < 30; column += 2) {
        const Color color{0, static_cast<unsigned char>(255 - column * 255 / 30), 0, 255};
        DrawRectangle(column, 0, 1, 19, color); DrawRectangle(239 - column, 0, 1, 19, color);
        DrawRectangle(column, 305, 1, 15, color); DrawRectangle(239 - column, 305, 1, 15, color);
    }
    pixelBorder(0, 18, ScreenWidth, 288, PixelGreen);
    text(title, 120 - (textWidth(title) + 1) / 2, 8);
    pixelTriangle(235, 21, 1, 1, PixelGreen);
    pixelLine(234, 22, 236, 22, PixelGreen); pixelLine(233, 23, 237, 23, PixelGreen);
    pixelLine(235, 302, 235, 302, PixelGreen);
    pixelLine(234, 301, 236, 301, PixelGreen); pixelLine(233, 300, 237, 300, PixelGreen);
    for (int dot = 0; dot < 3; ++dot) {
        DrawPixel(235, 25 + dot * 2, PixelGreen); DrawPixel(235, 298 - dot * 2, PixelGreen);
    }
    const int contentHeight = cursor + scroll - 35;
    // Scroll whole rows and expose the entire last row, not merely the scrollbar's nominal range.
    int maximum = 0;
    const int required = contentHeight - 274;
    for (int offset : lineOffsets) {
        if (offset >= required) { maximum = offset; break; }
    }
    const int thumb = std::clamp(260 * 260 / std::max(1, contentHeight), 2, 260);
    int thumbY = 32;
    if (contentHeight > 280) { thumbY += std::min(260 - thumb, (260 - thumb) * scroll / (contentHeight - 280)); }
    BeginScissorMode(233, 32, 5, 260);
    pixelLine(234, thumbY, 236, thumbY, PixelGreen); thumbY += 2;
    for (int height = 4; height < thumb; height += 2) { pixelLine(233, thumbY, 237, thumbY, PixelGreen); thumbY += 2; }
    pixelLine(234, thumbY, 236, thumbY, PixelGreen);
    EndScissorMode();
    return maximum;
}
}
