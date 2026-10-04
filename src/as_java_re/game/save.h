/** @file save.h
 * @brief Persist level-start checkpoints with atomic replacement and backup recovery.
 */
#pragma once
#include "world/world.h"

namespace alien {
struct Progress {
    int unlocked = 0;
    int difficulty = rules::DefaultDifficulty;
    bool sound = true;
    Inventory checkpoint;
};

// Save level-start equipment; rebuild positions and enemy state from the original level.
class SaveStore {
public:
    SaveStore(const Assets& assets, std::filesystem::path directory);
    Progress load();
    void write(const Progress& progress);
private:
    Progress read(const std::filesystem::path& path) const;
    const Assets& assets_;
    std::filesystem::path directory_;
    bool primaryValid_ = false;
};
}
