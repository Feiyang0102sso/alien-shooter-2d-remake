/** @file reference_scenarios.h
 * @brief Arrange controlled scenes shared with the original bytecode adapter.
 */
#pragma once
#include "world/world.h"
#include <stdexcept>

// Match Java adapter scene injection without implementing or replacing gameplay mechanisms.
class MechanismScenario {
public:
    MechanismScenario(alien::World& world, std::string name)
        : world_(world), name_(std::move(name)), spawnX_(world.player.x), spawnY_(world.player.y) {
        if (!active()) { return; }
        int type = 38;
        if (name_ == "terminal" || name_.rfind("exit", 0) == 0) { type = 41; }
        if (name_.rfind("platform", 0) == 0) { type = std::stoi(name_.substr(8)); }
        if (name_.rfind("generator", 0) == 0) { type = std::stoi(name_.substr(9)); }
        if (name_.rfind("break", 0) == 0) { type = std::stoi(name_.substr(5)); }
        alien::Object* target = nullptr;
        if (name_ == "wall") {
            for (int row = 1; row < world.level.height - 2 && !target; ++row) {
                for (int column = 1; column < world.level.width - 1; ++column) {
                    if (world.tileAt(column, row) != 158 || world.tileAt(column, row - 1) != 152) { continue; }
                    if (world.tileAt(column + 1, row) == 105) { wallDirection_ = 1; }
                    else if (world.tileAt(column - 1, row) == 105) { wallDirection_ = -1; }
                    else { continue; }
                    wall_.x = column * 16; wall_.y = row * 16 + 8; target = &wall_; break;
                }
            }
        } else {
            for (auto item = world.objects.rbegin(); item != world.objects.rend(); ++item) {
                if (!item->active || item->type != type) { continue; }
                if (name_ == "terminal" && item->terminalGroup == 0) { continue; }
                if (name_.rfind("exit", 0) == 0 && item->terminalGroup != 0) { continue; }
                if (!target || (type == 32 && item->stackCount > target->stackCount)) { target = &*item; }
                if (type != 32) { break; }
            }
        }
        if (!target) { throw std::runtime_error("No target for " + name_); }
        targetX_ = target->x; targetY_ = target->y;
        if (type == 41) {
            for (auto& item : world.objects) {
                if (item.type == 41 && item.terminalGroup == target->terminalGroup) {
                    item.variant = 1; world.level.tiles[item.cellY + 2][item.cellX + 2] = 121;
                }
            }
            if (name_ == "exit") { world.cleared = true; }
        }
        if (name_.rfind("break", 0) == 0 || name_ == "wall") {
            world.enemies.clear();
            for (auto& item : world.objects) {
                if (name_ == "wall" || item.type < 26 || item.type > 32) { item.active = false; }
            }
        } else if (name_.rfind("platform", 0) == 0) { place(targetX_ + 10, targetY_ + 10); }
        else {
            place(targetX_, targetY_);
            if (name_.rfind("generator", 0) == 0) { world.inventory.explosives = 1; }
        }
    }
    bool active() const {
        return name_ == "power" || name_ == "wall" || name_ == "terminal" || name_.rfind("exit", 0) == 0 || name_.rfind("platform", 0) == 0
            || name_.rfind("generator", 0) == 0 || name_.rfind("break", 0) == 0;
    }
    void beforeTick(int tick) {
        if (!active()) { return; }
        world_.player.health = 100000;
        if (tick == 1 && (name_ == "power" || name_.rfind("generator", 0) == 0)) { place(spawnX_, spawnY_); }
        if (name_.rfind("break", 0) == 0 && tick == 0) { world_.addProjectile(1, targetX_ - 12, targetY_ + 10, 1, 0, false); }
        if (name_ == "wall" && tick < 60 && tick % 10 == 0) {
            int x = targetX_ - 8;
            if (wallDirection_ < 0) { x = targetX_ + 24; }
            world_.addProjectile(1, x, targetY_, wallDirection_, 0, false);
        }
    }
    alien::Input input(int tick) const {
        alien::Input result;
        if (tick == 1 && (name_ == "terminal" || name_.rfind("exit", 0) == 0)) { result.down = true; }
        return result;
    }
private:
    void place(int x, int y) {
        world_.player.x = x; world_.player.y = y; world_.player.velocityY = 0; world_.player.supportY = y;
        world_.airState = 0;
    }
    alien::World& world_;
    std::string name_;
    int spawnX_, spawnY_, targetX_ = 0, targetY_ = 0, wallDirection_ = 0;
    alien::Object wall_;
};
