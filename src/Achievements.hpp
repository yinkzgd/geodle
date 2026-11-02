#pragma once
#include <Geode/Geode.hpp>
#include <functional>
#include <string>
#include <vector>
#include "Save.hpp"

namespace geodle::ach {

    // To add an achievement, append one `add(...)` line in Achievements.cpp::build().
    struct AchievementDef {
        std::string id;           // unique + stable (stored in save data)
        std::string name;
        std::string description;
        std::string category;
        std::string frame;        // optional vanilla sprite frame used as icon
        std::string glyph;        // fallback text drawn when the frame does not exist
        cocos2d::ccColor3B color; // badge colour
        std::function<bool(save::Stats const&, save::HistoryEntry const&)> test;
    };

    std::vector<AchievementDef> const& all();
    AchievementDef const* find(std::string const& id);

    // Evaluates every locked achievement after a finished game, persists and returns the new ones.
    std::vector<AchievementDef const*> evaluate(save::Stats const& stats, save::HistoryEntry const& last);

    cocos2d::CCNode* makeIcon(AchievementDef const& def, bool unlocked, float size);

    // Animated unlock notification (queued, survives scene changes).
    void toast(std::string const& id);
}
