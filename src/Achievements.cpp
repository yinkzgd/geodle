#include "Achievements.hpp"
#include <deque>

using namespace geode::prelude;

namespace geodle::ach {
namespace {
    using S = save::Stats;
    using H = save::HistoryEntry;

    std::vector<AchievementDef> build() {
        std::vector<AchievementDef> v;
        auto add = [&](char const* id, char const* name, char const* desc, char const* cat,
                       char const* frame, char const* glyph, ccColor3B col,
                       std::function<bool(S const&, H const&)> test) {
            v.push_back(AchievementDef{id, name, desc, cat, frame, glyph, col, std::move(test)});
        };

        // Getting started
        add("first_blood", "First Blood", "Complete your first Geodle.", "Getting Started",
            "GJ_starsIcon_001.png", "1", {255, 196, 40}, [](S const& s, H const&) { return s.played >= 1; });
        add("beginners_luck", "Beginner's Luck", "Solve a Geodle in 6 guesses.", "Getting Started",
            "GJ_moonsIcon_001.png", "6", {120, 190, 255}, [](S const&, H const& h) { return h.won && h.guesses == 6; });
        add("sharp_shooter", "Sharp Shooter", "Solve a Geodle in 3 guesses or fewer.", "Getting Started",
            "GJ_orbsIcon_001.png", "3", {80, 220, 255}, [](S const&, H const& h) { return h.won && h.guesses <= 3; });
        add("first_try", "First Try", "Solve a Geodle on your first guess.", "Getting Started",
            "GJ_diamondsIcon_001.png", "!", {255, 90, 200}, [](S const&, H const& h) { return h.won && h.guesses == 1; });

        // Streaks
        add("on_a_roll", "On a Roll", "Reach a 3-day streak.", "Streaks",
            "", "3", {110, 220, 110}, [](S const& s, H const&) { return s.maxStreak >= 3; });
        add("dedicated", "Dedicated", "Reach a 7-day streak.", "Streaks",
            "", "7", {70, 170, 255}, [](S const& s, H const&) { return s.maxStreak >= 7; });
        add("unstoppable", "Unstoppable", "Reach a 30-day streak.", "Streaks",
            "", "30", {170, 100, 255}, [](S const& s, H const&) { return s.maxStreak >= 30; });
        add("legendary", "Legendary", "Reach a 100-day streak.", "Streaks",
            "", "100", {255, 170, 30}, [](S const& s, H const&) { return s.maxStreak >= 100; });

        // Special
        add("perfectionist", "Perfectionist", "Win 10 Geodles without losing.", "Special",
            "", "10", {255, 215, 90}, [](S const& s, H const&) { return s.wins >= 10 && s.losses() == 0; });
        add("geometry_master", "Geometry Master", "Complete 50 daily puzzles.", "Special",
            "", "50", {235, 70, 80}, [](S const& s, H const&) { return s.played >= 50; });
        add("veteran", "Veteran", "Complete 100 daily puzzles.", "Special",
            "", "100", {200, 60, 120}, [](S const& s, H const&) { return s.played >= 100; });
        add("century_club", "Century Club", "Win 100 Geodles.", "Special",
            "", "C", {255, 140, 0}, [](S const& s, H const&) { return s.wins >= 100; });
        add("no_hints", "No Hints Needed", "Win 10 consecutive puzzles.", "Special",
            "", "10", {0, 225, 255}, [](S const& s, H const&) { return s.maxStreak >= 10; });
        return v;
    }

    bool frameExists(std::string const& f) {
        return !f.empty() && CCSpriteFrameCache::get()->spriteFrameByName(f.c_str()) != nullptr;
    }
}

std::vector<AchievementDef> const& all() {
    static auto defs = build();
    return defs;
}

AchievementDef const* find(std::string const& id) {
    for (auto const& d : all()) if (d.id == id) return &d;
    return nullptr;
}

std::vector<AchievementDef const*> evaluate(save::Stats const& stats, save::HistoryEntry const& last) {
    std::vector<AchievementDef const*> fresh;
    auto have = save::unlockedAchievements();
    for (auto const& d : all()) {
        if (have.count(d.id) || !d.test(stats, last)) continue;
        save::addUnlocked(d.id);
        fresh.push_back(&d);
    }
    return fresh;
}

CCNode* makeIcon(AchievementDef const& def, bool unlocked, float size) {
    auto node = CCNode::create();
    node->setContentSize({size, size});
    node->setAnchorPoint({0.5f, 0.5f});
    auto c = unlocked ? def.color : ccColor3B{70, 74, 90};
    auto dots = CCDrawNode::create();
    float r = size / 2;
    dots->drawDot({r, r}, r, {0.05f, 0.06f, 0.1f, 1.f});
    dots->drawDot({r, r}, r - 2.f, {c.r / 255.f, c.g / 255.f, c.b / 255.f, 1.f});
    dots->drawDot({r, r}, r - 6.f, {c.r / 255.f * 0.55f, c.g / 255.f * 0.55f, c.b / 255.f * 0.55f, 1.f});
    node->addChild(dots);

    CCNode* glyph = nullptr;
    if (frameExists(def.frame)) {
        auto spr = CCSprite::createWithSpriteFrameName(def.frame.c_str());
        spr->setScale(size * 0.55f / std::max(spr->getContentWidth(), spr->getContentHeight()));
        if (!unlocked) { spr->setColor({150, 150, 160}); spr->setOpacity(130); }
        glyph = spr;
    } else {
        auto l = CCLabelBMFont::create(def.glyph.c_str(), "bigFont.fnt");
        float w = std::max(l->getContentWidth(), 1.f);
        l->setScale(std::min(0.6f * size / 30.f, size * 0.62f / w));
        if (!unlocked) { l->setColor({150, 150, 160}); l->setOpacity(130); }
        glyph = l;
    }
    glyph->setPosition({r, r});
    node->addChild(glyph);
    return node;
}

// ---------------------------------------------------------------------------
// Toast
// ---------------------------------------------------------------------------
namespace {
    std::deque<std::string> s_queue;
    bool s_busy = false;
    void showNext();

    class AchToast : public CCNode {
    public:
        void done() {
            this->removeFromParentAndCleanup(true);
            s_busy = false;
            showNext();
        }
    };

    void showNext() {
        if (s_queue.empty()) { s_busy = false; return; }
        auto scene = CCDirector::get()->getRunningScene();
        if (!scene) return;
        auto def = find(s_queue.front());
        s_queue.pop_front();
        if (!def) { showNext(); return; }
        s_busy = true;

        auto toast = new AchToast();
        toast->init();
        toast->autorelease();
        CCSize size{290.f, 60.f};
        toast->setContentSize(size);
        toast->setAnchorPoint({0.5f, 0.5f});

        auto border = extension::CCScale9Sprite::create("square02_001.png", {0, 0, 80, 80});
        border->ignoreAnchorPointForPosition(false);
        border->setContentSize({size.width + 4, size.height + 4});
        border->setColor(def->color);
        border->setPosition({size.width / 2, size.height / 2});
        toast->addChild(border);
        auto bg = extension::CCScale9Sprite::create("square02_001.png", {0, 0, 80, 80});
        bg->ignoreAnchorPointForPosition(false);
        bg->setContentSize(size);
        bg->setColor({14, 16, 26});
        bg->setPosition({size.width / 2, size.height / 2});
        toast->addChild(bg);

        auto icon = makeIcon(*def, true, 42.f);
        icon->setPosition({34.f, size.height / 2});
        icon->setScale(0.f);
        icon->runAction(CCSequence::create(CCDelayTime::create(0.35f),
            CCEaseElasticOut::create(CCScaleTo::create(0.6f, 1.f), 0.5f), nullptr));
        toast->addChild(icon);

        auto head = CCLabelBMFont::create("Achievement Unlocked!", "goldFont.fnt");
        head->setScale(0.42f);
        head->setAnchorPoint({0.f, 0.5f});
        head->setPosition({64.f, size.height - 18.f});
        toast->addChild(head);
        auto name = CCLabelBMFont::create(def->name.c_str(), "bigFont.fnt");
        name->setAnchorPoint({0.f, 0.5f});
        name->setScale(std::min(0.55f, (size.width - 76.f) / std::max(name->getContentWidth(), 1.f)));
        name->setPosition({64.f, 20.f});
        toast->addChild(name);

        auto win = CCDirector::get()->getWinSize();
        float x = win.width / 2;
        float yShow = win.height - size.height / 2 - 10.f;
        toast->setPosition({x, win.height + size.height});
        scene->addChild(toast, 30000);
        SceneManager::get()->keepAcrossScenes(toast);

        FMODAudioEngine::sharedEngine()->playEffect("achievement_01.ogg");
        toast->runAction(CCSequence::create(
            CCEaseBackOut::create(CCMoveTo::create(0.5f, {x, yShow})),
            CCDelayTime::create(2.8f),
            CCEaseBackIn::create(CCMoveTo::create(0.4f, {x, win.height + size.height})),
            CCCallFunc::create(toast, callfunc_selector(AchToast::done)),
            nullptr));
    }
}

void toast(std::string const& id) {
    s_queue.push_back(id);
    if (!s_busy) showNext();
}

} // namespace geodle::ach
