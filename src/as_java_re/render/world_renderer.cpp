/** @file world_renderer.cpp
 * @brief Draw terrain, composed actors, objects, projectiles, HUD, and the death wipe.
 */
#include "render/renderer.h"
#include "render/pixel.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace alien {
namespace {
std::string padded(int value) {
    std::ostringstream result;
    result << std::setw(layout::DigitCount) << std::setfill('0') << value;
    return result.str();
}
bool contains(const std::vector<int>& values, int value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}
}
void Renderer::actor(const Actor& actor, const World& world) const {
    if (!actor.alive) { return; }
    if (actor.direction == 0 && (actor.behavior & 1) == 0) { return; }
    auto slots = assets_.frames.at(actor.frame);
    int y = ScreenHeight - actor.y + world.cameraBottom + world.drawOffsetY;
    if (actor.type == rules::PlayerType) {
        // Heavy weapons replace parts of the standing and walking frames.
        for (const auto& pose : assets_.weaponFrames) {
            if (pose.weapon == world.inventory.weapon && pose.frame == actor.frame) { slots = pose.slots; break; }
        }
        const auto& crouch = assets_.crouch;
        if (world.crouch > 0 && contains(crouch.frames, actor.frame)) { slots[crouch.slot] = crouch.part; }
        y += world.crouch;
        const auto& armor = assets_.armor;
        const bool armorFrame = actor.frame >= armor.firstFrame && actor.frame <= armor.lastFrame;
        if (world.inventory.armor > 0 && armorFrame) {
            for (int slot : armor.searchSlots) {
                if (slots[slot] == armor.torsoPart) { slots[slot + 1] = armor.parts.at(world.inventory.armor); break; }
            }
        }
    }
    for (int id : slots) { part(id, actor.x - world.cameraX, y, actor.direction); }
    if (actor.type == rules::PlayerType && world.playerLight != 0) { part(world.playerLight, actor.x - world.cameraX, y, actor.direction); }
    if (actor.type == assets_.weaponActorType) { part(assets_.weaponPartBase + actor.shotType, actor.x - world.cameraX, y, actor.direction); }
}

void Renderer::objectLayers(const ObjectVisual& visual, const Object& object, int theme, int x, int y, int baseline) const {
    for (const auto& layer : visual.layers) {
        if (!layer.states.empty() && !contains(layer.states, object.state)) { continue; }
        int id = layer.id;
        if (layer.frame == ObjectLayer::Frame::Variant) { id += layer.step * object.variant; }
        if (layer.frame == ObjectLayer::Frame::VisualFrame) { id += layer.step * object.visualFrame; }
        int top = y;
        if (layer.baseline) { top = baseline; }
        if (layer.part) { part(id, x + layer.x, top + layer.y); }
        else { tile(theme, id, x + layer.x, top + layer.y, true); }
    }
    if (visual.hasShadow) {
        int top = y;
        if (visual.shadow.baseline) { top = baseline; }
        pixelShadow(x + visual.shadow.x, top + visual.shadow.y, visual.shadow.width, rgb(assets_.effects.shadowColor));
    }
}

void Renderer::generatorEffects(const Object& object, const World& world, int x, int y) const {
    const auto& effects = assets_.effects;
    for (int line = 0; line < effect_shape::LampCount; ++line) {
        Color light = rgb(effects.generatorIdleColors[object.visualFrame]);
        if (world.power) { light = rgb(effects.generatorPoweredColors[object.visualFrame]); }
        const int lineY = y + effect_shape::LampTop - line * effect_shape::LampSpacing;
        pixelLine(x + effect_shape::LampLeft, lineY, x + effect_shape::LampRight, lineY, light);
    }
    if (!world.power) { return; }
    // The two spark colors swap on the first lamp frame.
    Color first = rgb(effects.sparkColors[1]), second = rgb(effects.sparkColors[0]);
    if (object.visualFrame == 0) { first = rgb(effects.sparkColors[0]); second = rgb(effects.sparkColors[1]); }
    const auto& arcs = object.lightArcs;
    using namespace effect_shape;
    pixelLine(x + InnerArcLeft - arcs[0], y + InnerArcY - arcs[0], x + InnerArcRight + arcs[1], y + InnerArcY - arcs[1], first);
    pixelLine(x + OuterArcLeft + arcs[0], y + OuterArcY + arcs[0], x + OuterArcRight - arcs[1], y + OuterArcY + arcs[1], second);
    pixelLine(x + InnerArcLeft - arcs[2], y + InnerArcY - arcs[2], x + InnerArcRight + arcs[3], y + InnerArcY - arcs[3], second);
}

void Renderer::switchLasers(const Object& object, int x, int y) const {
    using namespace effect_shape;
    const Color laser = rgb(color::SwitchLaser);
    for (int row = 0; row < LaserRows; ++row) {
        const int leftX = x + object.animationFrame + LaserLeftX + row;
        const int rightX = x + LaserRightX - object.animationFrame - row;
        pixelLine(leftX, y + LaserTop + row, leftX, y + LaserBottom - row, laser);
        pixelLine(rightX, y + LaserTop + row, rightX, y + LaserBottom - row, laser);
    }
}

void Renderer::terminalEffects(const Object& object, int theme, int x, int y) const {
    if (object.terminalGroup == rules::ExitTerminalGroup) {
        text(assets_.text.label("terminal_exit"), x + layout::ExitLabelX, y + layout::ExitLabelY, 1);
    }
    if (object.animationFrame != 0) {
        pixelTriangle(x + layout::TerminalArrowX, y + layout::TerminalArrowY + object.animationFrame, layout::TerminalArrowWidth, 1, PixelGreen);
    }
    // Doors slide inside the frame; the group number plate moves with them.
    const auto& visual = assets_.terminalVisual;
    BeginScissorMode(x + visual.clipX, y + visual.clipY, visual.clipWidth, visual.clipHeight);
    const int doorY = y + object.doorOffset;
    if (object.variant != visual.doorlessVariant) {
        for (int id : visual.doorTiles) { tile(theme, id, x + visual.doorX, doorY, true); }
    }
    if (object.terminalGroup > rules::ExitTerminalGroup) { tile(theme, visual.groupTileBase + object.terminalGroup, x + visual.groupX, doorY, true); }
    EndScissorMode();
}

void Renderer::object(const Object& object, const World& world) const {
    if (!object.active || !assets_.hasParameters(object.type)) { return; }
    const auto& p = assets_.entities.at(object.type);
    if (object.x + p.right < world.cameraX - rules::VisibilityMargin || object.x - p.left > world.cameraX + ScreenWidth + rules::VisibilityMargin
        || object.y + p.top < world.cameraBottom || object.y - p.bottom > world.cameraBottom + ScreenHeight) { return; }
    const int x = object.x - world.cameraX - p.left;
    const int y = ScreenHeight - object.y + world.cameraBottom + world.drawOffsetY;
    const int theme = world.level.theme;
    const int baseline = y + object.y - object.baselineY;
    const auto visual = assets_.objectVisuals.find(object.type);
    if (visual != assets_.objectVisuals.end()) { objectLayers(visual->second, object, theme, x, y, baseline); }
    switch (object.type) {
    case object_type::Generator: generatorEffects(object, world, x, y); break;
    case object_type::TeleporterSource: case object_type::HiveSource:
        if (object.state == source_state::Armed) {
            text(std::to_string(object.animationFrame), x + layout::CountdownTextX, y + layout::CountdownTextY);
        }
        break;
    case object_type::PowerSwitch: switchLasers(object, x, y); break;
    case object_type::Terminal: terminalEffects(object, theme, x, y); break;
    }
}

void Renderer::projectile(const Projectile& projectile, const World& world) const {
    const int x = projectile.x - world.cameraX;
    const int y = ScreenHeight - projectile.y + world.cameraBottom;
    const int margin = layout::ProjectileCullMargin;
    if (x + margin < 0 || x - margin > ScreenWidth || y + margin < 0 || y - margin > ScreenHeight) { return; }
    const auto& visual = assets_.projectileVisuals.at(projectile.type);
    const int id = visual.part + projectile.life * visual.lifeStep + projectile.variant * visual.variantStep;
    int direction = 1;
    if (visual.facing == ProjectileVisual::Facing::Velocity && projectile.dx < 0) { direction = -1; }
    if (visual.facing == ProjectileVisual::Facing::Impact && projectile.originalDirection > 0) { direction = -1; }
    part(id, x, y + world.drawOffsetY, direction);
}

void Renderer::hud(const World& world, bool showHealth) const {
    using namespace layout;
    const auto& inv = world.inventory;
    const auto& themeTiles = assets_.tiles[world.level.theme];
    const Color panel = rgb(color::HudPanel), border = rgb(color::HudBorder);
    const int offsetY = world.drawOffsetY;
    const int ammoY = world.ammoPanelY + offsetY;
    DrawRectangle(0, ammoY, AmmoPanelWidth, PanelHeight, panel); pixelBorder(0, ammoY, AmmoPanelWidth, PanelHeight, border);
    const int capacity = assets_.projectiles[inv.weapon].capacity;
    const int bar = std::clamp(inv.ammo[inv.weapon] * AmmoBarWidth / capacity, 0, AmmoBarWidth);
    unsigned int barColor = color::AmmoFull;
    if (bar <= AmmoBarLow) { barColor = color::AmmoLow; }
    DrawRectangle(AmmoBarX, ammoY + AmmoBarY, bar, AmmoBarHeight, rgb(barColor));
    // Darker stripes on every second column of the bar.
    for (int column = AmmoBarX + 1; column < AmmoBarX + 1 + bar; column += 2) {
        pixelLine(column, ammoY + AmmoBarY, column, ammoY + AmmoBarY + AmmoBarHeight - 1, rgb(barColor & color::AmmoStripeMask));
    }
    text(padded(inv.ammo[inv.weapon]), AmmoTextX, ammoY + PanelTextY);
    if (world.ammoPanelY != 0 && inv.weapon > rules::Pistol) {
        DrawRectangle(0, ammoY + WeaponPanelTop, AmmoPanelWidth, layout::WeaponPanelHeight, panel);
        pixelBorder(0, ammoY + WeaponPanelTop, AmmoPanelWidth, layout::WeaponPanelHeight, border);
        const auto& weapon = themeTiles[assets_.hud.weaponIconTileBase + inv.weapon].image;
        sprite(weapon, WeaponIconCenterX - weapon.width / 2, ammoY + WeaponIconTop, false);
    }
    DrawRectangle(ScreenWidth - ScorePanelWidth, offsetY, ScorePanelWidth, PanelHeight, panel);
    pixelBorder(ScreenWidth - ScorePanelWidth, offsetY, ScorePanelWidth, PanelHeight, border);
    sprite(assets_.parts[assets_.hud.scorePart][0], ScreenWidth - ScoreIconInsetX, ScoreIconY + offsetY, false);
    if (world.scoreVisible) { text(padded(inv.score), ScreenWidth - ScoreTextInsetX, PanelTextY + offsetY, 2); }
    if (showHealth) {
        const auto& healthPanel = themeTiles[assets_.hud.healthPanelTile].image;
        sprite(healthPanel, 0, ScreenHeight - healthPanel.height + offsetY, false);
        text(std::to_string(world.player.health), HealthTextX, ScreenHeight - HealthTextBottom + offsetY, 1);
        if (inv.weapon > rules::Pistol) {
            const auto& ammo = assets_.parts[assets_.hud.ammoPartBase + inv.weapon][0];
            sprite(ammo, AmmoIconCenterX - ammo.width / 2, ScreenHeight - AmmoIconBottom - ammo.height + offsetY, false);
        }
    }
    if (world.explosiveVisible) {
        BeginScissorMode(0, ExplosiveRowY + offsetY, ScreenWidth, ExplosiveRowHeight);
        for (int item = 0; item < inv.explosives; ++item) {
            sprite(themeTiles[assets_.hud.explosiveTile].image, ScreenWidth - ExplosiveIconInsetX - item * ExplosiveIconSpacing, ExplosiveRowY + offsetY, false);
        }
        EndScissorMode();
    }
    if (showHealth && world.flashOuterColor >= 0) {
        pixelBorder(0, offsetY, ScreenWidth, ScreenHeight, rgb(static_cast<unsigned int>(world.flashOuterColor)));
        pixelBorder(1, 1 + offsetY, ScreenWidth - 2, ScreenHeight - 2, rgb(static_cast<unsigned int>(world.flashInnerColor)));
    }
}

void Renderer::deathWipe(const World& world) const {
    if (!world.deathWipeActive) { return; }
    const Color wipe = rgb(color::DeathWipe);
    for (int column = 0; column < static_cast<int>(world.deathColumns.size()); ++column) {
        const int center = world.deathColumns[column];
        int left = center, right = center;
        // Inner columns slope toward taller neighbors to form drips.
        if (column > 0 && column < static_cast<int>(world.deathColumns.size()) - 2) {
            left = center - 2; right = center - 2;
            if (world.deathColumns[column - 1] > center) { left = center + 2; }
            if (world.deathColumns[column + 1] > center) { right = center + 2; }
        }
        const int x = 2 + column * rules::DeathWipeColumnWidth;
        pixelLine(x - 1, 0, x - 1, left, wipe);
        pixelLine(x, 0, x, center, wipe);
        pixelLine(x + 1, 0, x + 1, right, wipe);
    }
}
void Renderer::world(const World& world, bool collisionDebug, bool showHealth) const {
    // Do not clear the original canvas; shaken edges retain the preceding frame's pixels.
    const int firstRow = std::max(0, world.cameraBottom / TileSize);
    const int lastRow = std::min(world.level.height - 1, (world.cameraBottom + ScreenHeight) / TileSize + 1);
    const int firstColumn = std::max(0, world.cameraX / TileSize);
    const int lastColumn = std::min(world.level.width - 1, (world.cameraX + ScreenWidth) / TileSize + 1);
    for (int row = lastRow; row >= firstRow; --row) {
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const int x = column * TileSize - world.cameraX;
            const int y = ScreenHeight - (row + 1) * TileSize + world.cameraBottom + world.drawOffsetY;
            const int id = world.level.tiles[row][column];
            if (id >= tile_id::CompositeFirst) {
                const auto& composition = world.compositeTiles[id];
                if (composition[0] == 0 || composition[1] == 0) { DrawRectangle(x, y, TileSize, TileSize, BLACK); }
                else {
                    tile(world.level.theme, composition[0], x, y);
                    tile(world.level.theme, composition[1], x, y, true);
                }
            } else if (id == tile_id::Empty) { DrawRectangle(x, y, TileSize, TileSize, BLACK); }
            else { tile(world.level.theme, id, x, y); }
            if (collisionDebug && world.level.collision[row][column] > 0) {
                const int height = world.level.collision[row][column];
                DrawRectangle(x, y + TileSize - height, TileSize, height, Fade(RED, 0.35f));
            }
        }
    }
    // The original paint draws breakables in a second pass after all other objects.
    for (auto iterator = world.objects.rbegin(); iterator != world.objects.rend(); ++iterator) {
        if (iterator->type < object_type::BreakableFirst || iterator->type > object_type::BreakableLast) { object(*iterator, world); }
    }
    for (auto iterator = world.objects.rbegin(); iterator != world.objects.rend(); ++iterator) {
        if (iterator->type >= object_type::BreakableFirst && iterator->type <= object_type::BreakableLast) { object(*iterator, world); }
    }
    actor(world.player, world);
    for (auto iterator = world.enemies.rbegin(); iterator != world.enemies.rend(); ++iterator) { actor(*iterator, world); }
    for (auto iterator = world.projectiles.rbegin(); iterator != world.projectiles.rend(); ++iterator) { projectile(*iterator, world); }
    hud(world, showHealth);
}
}
