/** @file world_effects.cpp
 * @brief Advance paint-time visuals, health flashes, and the death sequence.
 */
#include "world/world.h"
#include "world/geometry.h"
#include <algorithm>

namespace alien {
void World::beginDeathEffect() {
    deathParticleSteps_ = rules::DeathParticleSteps; deathWipeActive = deathWipeDone_ = false; deathColumns = {};
    for (int count = 0; count < rules::DeathParticles; ++count) {
        const int horizontal = randomBits(rules::BloodHorizontalBits) - rules::BloodHorizontalCenter, vertical = randomBits(3);
        addProjectile(rules::BloodProjectile, player.x, player.y + rules::BloodSpawnHeight, horizontal, vertical, false);
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
        deathColumns[column] += randomBits(31) % rules::DeathWipeMaxStep;
        if (deathColumns[column] > ScreenHeight) { deathWipeDone_ = true; }
    }
    return false;
}
void World::beginHealthFlash(bool healing) {
    if (healthFlash != 0) { return; }
    healthFlash = rules::HealthFlashTicks; flashColor_ = rules::DamageFlashColor; flashStep_ = rules::DamageFlashStep;
    if (healing) { flashColor_ = rules::HealFlashColor; flashStep_ = rules::HealFlashStep; }
}

void World::updateVisuals() {
    // Update original paint-time animations per logical tick, never per desktop refresh.
    // Apply the current shake to the whole canvas, including HUD, before decaying the next offset.
    drawOffsetY = shake;
    const int damping = assets_.effects.shakeDamping;
    if (shake > 0) { shake = -shake + damping; }
    else if (shake < 0) { shake = -shake - damping; }
    const int generatorFrames = static_cast<int>(assets_.effects.generatorIdleColors.size());
    for (auto item = objects.rbegin(); item != objects.rend(); ++item) {
        if (!item->active || !assets_.hasParameters(item->type)) { continue; }
        const auto& p = assets_.entities[item->type];
        if (item->x + p.right < cameraX - rules::VisibilityMargin || item->x - p.left > cameraX + ScreenWidth + rules::VisibilityMargin
            || item->y + p.top < cameraBottom || item->y - p.bottom > cameraBottom + ScreenHeight) { continue; }
        item->visualFrame = item->variant;
        if (item->type == object_type::Medkit) {
            if (--item->variant < 0) { item->variant = rules::MedkitFrames - 1; }
        } else if (item->type == object_type::Generator) {
            if (power) { for (int& value : item->lightArcs) { value = randomBits(assets_.effects.sparkRandomBits); } }
            item->variant = (item->variant + 1) % generatorFrames;
        } else if (item->type == object_type::FloorFlame) {
            if (item->visualCounter++ > rules::FlameFrameHold) { item->visualCounter = 0; item->variant = 1 - item->variant; }
        }
    }
    scoreVisible = scoreFlash_ == 0 || (scoreFlash_ & 1) == 0;
    if (scoreFlash_ > 0) { --scoreFlash_; }
    explosiveVisible = explosiveBlink_;
    if (inventory.explosives > 0) { explosiveBlink_ = !explosiveBlink_; }
    // The weapon panel slides down, holds, then slides back up whenever a gun is selected.
    if (displayedWeapon_ != inventory.weapon) {
        displayedWeapon_ = inventory.weapon; weaponPanelHold_ = 0;
        if (inventory.weapon > rules::Pistol) {
            if (weaponPanelY_ == 0) { weaponPanelY_ = 1; }
            weaponPanelSpeed_ = rules::WeaponPanelSpeed;
        } else { weaponPanelY_ = 0; weaponPanelSpeed_ = 0; }
    }
    ammoPanelY = weaponPanelY_;
    if (weaponPanelY_ != 0 && inventory.weapon > rules::Pistol) {
        if (timeMs - weaponPanelHold_ > rules::WeaponPanelHoldMs) { weaponPanelHold_ = 0; }
        if (weaponPanelHold_ == 0) {
            weaponPanelY_ += weaponPanelSpeed_;
            if (weaponPanelY_ >= rules::WeaponPanelSlide) {
                weaponPanelY_ = rules::WeaponPanelSlide; weaponPanelSpeed_ = -weaponPanelSpeed_; weaponPanelHold_ = timeMs;
            }
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
}
