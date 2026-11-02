#include "GeodleLayer.hpp"
#include "Words.hpp"

#include <algorithm>

using namespace geode::prelude;
namespace logic = geodle::logic;
namespace save = geodle::save;
namespace words = geodle::words;
namespace ach = geodle::ach;
using extension::CCScale9Sprite;

namespace {

const ccColor3B WHITE{255, 255, 255};
const ccColor3B GREEN{50, 205, 80};
const ccColor3B YELLOW{255, 205, 50};
const ccColor3B GRAY{78, 82, 98};
const ccColor3B CYAN{0, 225, 255};
const ccColor3B RED{235, 70, 80};
const ccColor3B PURPLE{160, 90, 255};
const ccColor3B BLUE{70, 140, 255};
const ccColor3B GOLD{255, 196, 40};
const ccColor3B PANEL{14, 16, 26};
const ccColor3B CARD{28, 31, 46};
const ccColor3B TILE_BORDER{60, 64, 82};
const ccColor3B TILE_TYPED{135, 141, 170};
const ccColor3B TILE_FILL{22, 24, 36};
const ccColor3B KEY_IDLE{104, 110, 138};
const ccColor3B KEY_ABSENT{46, 50, 62};
const ccColor3B MUTED{150, 156, 180};

// Design-space size of the panel (scaled to fit any resolution).
constexpr float PW = 420.f, PH = 470.f;
constexpr float TILE = 38.f, PITCH = 43.f, GRID_TOP_Y = 133.f;
constexpr int TAG_SCALE = 901, TAG_BOUNCE = 902;

GeodleLayer* s_instance = nullptr;

CCScale9Sprite* rounded(CCSize size, ccColor3B col, GLubyte op = 255) {
    auto s = CCScale9Sprite::create("square02_001.png", {0, 0, 80, 80});
    s->ignoreAnchorPointForPosition(false);
    s->setAnchorPoint({0.5f, 0.5f});
    s->setContentSize(size);
    s->setColor(col);
    s->setOpacity(op);
    return s;
}

CCLabelBMFont* lbl(std::string const& t, char const* font, float scale, ccColor3B c = WHITE) {
    auto l = CCLabelBMFont::create(t.c_str(), font);
    l->setScale(scale);
    l->setColor(c);
    return l;
}

// Wrapped paragraph, anchored top-left.
CCLabelBMFont* para(CCNode* parent, std::string const& t, CCPoint topLeft, float scale, float width,
                    ccColor3B c = WHITE) {
    auto l = CCLabelBMFont::create(t.c_str(), "chatFont.fnt", width / scale, kCCTextAlignmentLeft, CCPointZero);
    l->setScale(scale);
    l->setColor(c);
    l->setAnchorPoint({0.f, 1.f});
    l->setPosition(topLeft);
    parent->addChild(l);
    return l;
}

void runScale(CCNode* n, float s, float dur = 0.08f) {
    n->stopActionByTag(TAG_SCALE);
    auto a = CCEaseSineOut::create(CCScaleTo::create(dur, s));
    a->setTag(TAG_SCALE);
    n->runAction(a);
}

void miniTile(CCNode* parent, CCPoint pos, float size, char ch, ccColor3B col) {
    auto n = CCNode::create();
    n->setPosition(pos);
    auto b = rounded({size, size}, col);
    n->addChild(b);
    if (ch) {
        auto l = lbl(std::string(1, ch), "bigFont.fnt", size / 38.f * 0.6f);
        l->setPosition({0, 1});
        n->addChild(l);
    }
    parent->addChild(n);
}

} // namespace

// ===========================================================================
// Lifecycle
// ===========================================================================

GeodleLayer* GeodleLayer::get() {
    if (s_instance && !s_instance->isRunning()) s_instance = nullptr;
    return s_instance;
}

void GeodleLayer::toggle() {
    if (auto l = get()) {
        l->close();
        return;
    }
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return;
    // Opened mid-level: pause the level (once) so the player doesn't die in the background.
    if (auto pl = PlayLayer::get()) {
        if (!scene->getChildByType<PauseLayer>(0)) pl->pauseGame(false);
    }
    auto layer = GeodleLayer::create();
    if (!layer) return;
    s_instance = layer;
    scene->addChild(layer, 20000);
}

GeodleLayer* GeodleLayer::create() {
    auto ret = new GeodleLayer();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

GeodleLayer::~GeodleLayer() {
    if (s_instance == this) s_instance = nullptr;
}

bool GeodleLayer::init() {
    if (!CCLayerColor::initWithColor(ccc4(0, 0, 0, 0))) return false;

    m_today = logic::today();
    m_answer = words::answerForDay(m_today);
    m_game = save::loadGame(m_today);
    save::normalizeStreak(m_today);

    this->setTouchMode(kCCTouchesOneByOne);
    this->setTouchPriority(-1000);
    this->setTouchEnabled(true);

    m_root = CCNode::create();
    this->addChild(m_root, 1);
    applyLayout(true);

    // backdrop + pop-in
    this->runAction(CCFadeTo::create(0.2f, 175));
    m_root->setScale(m_scale * 0.75f);
    m_root->runAction(CCEaseBackOut::create(CCScaleTo::create(0.28f, m_scale)));

    // panel (glow border + body)
    auto glow = rounded({PW + 8, PH + 8}, CYAN, 90);
    m_root->addChild(glow, 0);
    auto body = rounded({PW, PH}, PANEL, 250);
    m_root->addChild(body, 0);

    this->scheduleUpdate();
    this->schedule(schedule_selector(GeodleLayer::tick), 0.25f);

    if (!save::tutorialSeen()) goTo(Page::Help);
    else showPage(Page::Home);
    return true;
}

void GeodleLayer::applyLayout(bool initial) {
    auto win = CCDirector::get()->getWinSize();
    m_lastWin = win;
    this->setContentSize(win);
    m_scale = std::min(win.width * 0.96f / PW, win.height * 0.97f / PH);
    m_root->setPosition({win.width / 2, win.height / 2});
    if (!initial && !m_closing) m_root->setScale(m_scale);
}

void GeodleLayer::update(float) {
    auto win = CCDirector::get()->getWinSize();
    if (win.width != m_lastWin.width || win.height != m_lastWin.height) applyLayout(false);
    if (m_closing || m_pressed) return;

    // hover effect (polled each frame - works on any platform with a mouse)
    auto mp = geode::cocos::getMousePos();
    CCNode* hit = nullptr;
    if (auto b = hitTest(mp)) hit = b->node;
    if (hit != m_hover) {
        if (m_hover) runScale(m_hover, 1.f);
        if (hit) runScale(hit, 1.07f);
        m_hover = hit;
    }
}

void GeodleLayer::tick(float) {
    if (m_closing) return;
    if (logic::today() != m_today) {
        onNewDay();
        return;
    }
    auto t = "Next Geodle in " + logic::countdown(logic::secondsUntilNextDay());
    if (m_countdownLabel) m_countdownLabel->setString(t.c_str());
    if (m_overlayCountdown) m_overlayCountdown->setString(t.c_str());
}

void GeodleLayer::onNewDay() {
    m_today = logic::today();
    m_answer = words::answerForDay(m_today);
    m_game = save::loadGame(m_today);
    m_current.clear();
    save::normalizeStreak(m_today);
    showPage(m_page);
    if (m_page == Page::Game) showMessage("A new Geodle is available!");
}

void GeodleLayer::close() {
    if (m_closing) return;
    m_closing = true;
    flushToasts();
    this->unscheduleUpdate();
    m_root->stopAllActions();
    m_root->runAction(CCEaseBackIn::create(CCScaleTo::create(0.18f, m_scale * 0.7f)));
    this->stopAllActions();
    this->runAction(CCSequence::create(
        CCFadeTo::create(0.18f, 0),
        CCCallFunc::create(this, callfunc_selector(GeodleLayer::finishClose)),
        nullptr));
}

void GeodleLayer::finishClose() {
    if (s_instance == this) s_instance = nullptr;
    this->removeFromParentAndCleanup(true);
}

bool GeodleLayer::onKey(enumKeyCodes key, bool repeat) {
    if (m_closing) return true;
    if (key == KEY_Escape) {
        if (!repeat) close();
        return true;
    }
    if (m_page != Page::Game || m_overlay) return true;
    int k = static_cast<int>(key);
    if (k >= static_cast<int>(KEY_A) && k <= static_cast<int>(KEY_Z)) {
        if (!repeat) typeLetter(static_cast<char>('A' + (k - static_cast<int>(KEY_A))));
    } else if (key == KEY_Backspace) {
        deleteLetter();
    } else if (key == KEY_Enter) {
        if (!repeat) submit();
    }
    return true;
}

// ===========================================================================
// Touch + buttons
// ===========================================================================

std::vector<GeodleLayer::Btn>& GeodleLayer::activeBtns() {
    return m_overlay ? m_modalBtns : m_btns;
}

GeodleLayer::Btn* GeodleLayer::hitTest(CCPoint world) {
    for (auto& b : activeBtns()) {
        auto parent = b.node->getParent();
        if (!parent) continue;
        auto p = parent->convertToNodeSpace(world);
        auto pos = b.node->getPosition();
        CCRect r{pos.x - b.size.width / 2, pos.y - b.size.height / 2, b.size.width, b.size.height};
        if (r.containsPoint(p)) return &b;
    }
    return nullptr;
}

CCNode* GeodleLayer::addButton(CCNode* parent, CCPoint pos, CCSize size, std::string const& text,
                               ccColor3B col, std::function<void()> cb, float fontScale, bool modal) {
    auto node = CCNode::create();
    node->setContentSize(size);
    node->setAnchorPoint({0.5f, 0.5f});
    node->setPosition(pos);

    CCPoint mid{size.width / 2, size.height / 2};
    auto shadow = rounded(size, {0, 0, 0}, 120);
    shadow->setPosition({mid.x, mid.y - 3.f});
    node->addChild(shadow);
    auto bg = rounded(size, col);
    bg->setPosition(mid);
    bg->setTag(1);
    node->addChild(bg);

    auto l = lbl(text, "bigFont.fnt", fontScale);
    float maxW = size.width - 10.f;
    if (l->getContentWidth() * fontScale > maxW) l->setScale(maxW / l->getContentWidth());
    l->setPosition({mid.x, mid.y + 1.f});
    l->setTag(2);
    node->addChild(l);

    parent->addChild(node, 1);
    (modal ? m_modalBtns : m_btns).push_back(Btn{node, size, std::move(cb)});
    return node;
}

bool GeodleLayer::ccTouchBegan(CCTouch* t, CCEvent*) {
    if (m_closing) return true;
    if (auto b = hitTest(t->getLocation())) {
        m_pressed = b->node;
        runScale(b->node, 0.92f, 0.05f);
    }
    return true; // always swallow: nothing underneath may be clicked
}

void GeodleLayer::ccTouchCancelled(CCTouch*, CCEvent*) {
    if (m_pressed) runScale(m_pressed, 1.f);
    m_pressed = nullptr;
}

void GeodleLayer::ccTouchEnded(CCTouch* t, CCEvent*) {
    auto pressed = m_pressed;
    m_pressed = nullptr;
    if (!pressed || m_closing) return;
    runScale(pressed, 1.f);
    m_hover = nullptr;
    auto b = hitTest(t->getLocation());
    if (b && b->node == pressed) {
        FMODAudioEngine::sharedEngine()->playEffect("playSound_01.ogg");
        auto cb = b->cb; // copy: the callback may rebuild the page and invalidate `b`
        cb();
    }
}

// ===========================================================================
// Pages
// ===========================================================================

void GeodleLayer::goTo(Page p) {
    m_sub = 0;
    showPage(p);
}

void GeodleLayer::showPage(Page p) {
    flushToasts();
    m_page = p;
    m_busy = false;
    if (m_pageNode) m_pageNode->removeFromParentAndCleanup(true);
    m_btns.clear();
    m_modalBtns.clear();
    m_overlay = nullptr;
    m_msg = nullptr;
    m_hover = nullptr;
    m_pressed = nullptr;
    m_countdownLabel = nullptr;
    m_overlayCountdown = nullptr;
    m_tiles = {};
    m_keys.clear();
    m_keyState.clear();

    m_pageNode = CCNode::create();
    m_root->addChild(m_pageNode, 2);

    if (p != Page::Home) buildHeader();
    else addButton(m_pageNode, {187, 207}, {30, 30}, "X", RED, [this] { close(); }, 0.5f);

    switch (p) {
        case Page::Home: buildHome(); break;
        case Page::Game: buildGame(); break;
        case Page::Stats: buildStats(); break;
        case Page::Achievements: buildAchievements(); break;
        case Page::Help: buildHelp(); break;
        case Page::History: buildHistory(); break;
    }
    m_pageNode->setScale(0.96f);
    m_pageNode->runAction(CCEaseBackOut::create(CCScaleTo::create(0.18f, 1.f)));
}

CCNode* GeodleLayer::makeLogo(float scale, bool animate) {
    static const ccColor3B cols[6] = {CYAN, GREEN, YELLOW, RED, PURPLE, GOLD};
    auto node = CCNode::create();
    std::vector<CCLabelBMFont*> letters;
    float total = 0.f;
    char const* word = "GEODLE";
    for (int i = 0; i < 6; ++i) {
        auto l = lbl(std::string(1, word[i]), "bigFont.fnt", scale, cols[i]);
        letters.push_back(l);
        total += l->getContentWidth() * scale + 3.f;
    }
    float x = -total / 2;
    int i = 0;
    for (auto l : letters) {
        float w = l->getContentWidth() * scale;
        l->setPosition({x + w / 2, 0});
        x += w + 3.f;
        node->addChild(l);
        if (animate) {
            l->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create(0.1f * i),
                CCEaseSineInOut::create(CCMoveBy::create(0.45f, {0, 4.f})),
                CCEaseSineInOut::create(CCMoveBy::create(0.45f, {0, -4.f})),
                CCDelayTime::create(0.7f - 0.1f * i * 0.5f),
                nullptr)));
        }
        ++i;
    }
    return node;
}

void GeodleLayer::buildHeader() {
    auto logo = makeLogo(0.55f, false);
    logo->setPosition({0, 207});
    m_pageNode->addChild(logo);
    auto line = CCLayerColor::create(ccc4(0, 225, 255, 140), 380.f, 2.f);
    line->ignoreAnchorPointForPosition(false);
    line->setAnchorPoint({0.5f, 0.5f});
    line->setPosition({0, 190});
    m_pageNode->addChild(line);

    addButton(m_pageNode, {187, 207}, {30, 30}, "X", RED, [this] { close(); }, 0.5f);
    addButton(m_pageNode, {-187, 207}, {30, 30}, "<", BLUE, [this] {
        goTo(m_page == Page::History ? Page::Stats : Page::Home);
    }, 0.5f);
}

void GeodleLayer::addPager(int pages) {
    if (pages <= 1) return;
    auto txt = lbl(std::to_string(m_sub + 1) + " / " + std::to_string(pages), "bigFont.fnt", 0.4f, MUTED);
    txt->setPosition({0, -212});
    m_pageNode->addChild(txt);
    addButton(m_pageNode, {-60, -212}, {40, 28}, "<", BLUE, [this, pages] {
        m_sub = (m_sub + pages - 1) % pages;
        showPage(m_page);
    }, 0.45f);
    addButton(m_pageNode, {60, -212}, {40, 28}, ">", BLUE, [this, pages] {
        m_sub = (m_sub + 1) % pages;
        showPage(m_page);
    }, 0.45f);
}

// ---------------------------------------------------------------- Home
void GeodleLayer::buildHome() {
    auto logo = makeLogo(1.15f, true);
    logo->setPosition({0, 150});
    m_pageNode->addChild(logo);
    auto sub = lbl("The daily Geometry Dash word game", "chatFont.fnt", 0.6f, MUTED);
    sub->setPosition({0, 112});
    m_pageNode->addChild(sub);

    CCSize bs{250, 38};
    addButton(m_pageNode, {0, 72}, bs, "Daily Puzzle", GREEN, [this] { goTo(Page::Game); }, 0.55f);
    addButton(m_pageNode, {0, 24}, bs, "Statistics", CYAN, [this] { goTo(Page::Stats); }, 0.55f);
    addButton(m_pageNode, {0, -24}, bs, "Achievements", GOLD, [this] { goTo(Page::Achievements); }, 0.55f);
    addButton(m_pageNode, {0, -72}, bs, "Help", BLUE, [this] { goTo(Page::Help); }, 0.55f);
    addButton(m_pageNode, {0, -120}, bs, "History", PURPLE, [this] { goTo(Page::History); }, 0.55f);
    addButton(m_pageNode, {0, -166}, {160, 32}, "Close", RED, [this] { close(); }, 0.5f);

    auto stats = save::loadStats();
    std::string status;
    ccColor3B sc = MUTED;
    if (m_game.finished && m_game.won) {
        status = "Today: solved in " + std::to_string(m_game.guesses.size()) + "/6";
        sc = GREEN;
    } else if (m_game.finished) {
        status = "Today: not solved";
        sc = RED;
    } else if (!m_game.guesses.empty()) {
        status = "Today: " + std::to_string(m_game.guesses.size()) + "/6 guesses used";
        sc = YELLOW;
    } else {
        status = "Today's puzzle awaits!";
        sc = CYAN;
    }
    auto sl = lbl(status + "  |  Streak " + std::to_string(stats.streak), "chatFont.fnt", 0.55f, sc);
    sl->setPosition({0, -200});
    m_pageNode->addChild(sl);
    m_countdownLabel = lbl("Next Geodle in " + logic::countdown(logic::secondsUntilNextDay()), "chatFont.fnt", 0.5f, MUTED);
    m_countdownLabel->setPosition({0, -219});
    m_pageNode->addChild(m_countdownLabel);
}

// ---------------------------------------------------------------- Stats
void GeodleLayer::buildStats() {
    auto s = save::loadStats();
    auto title = lbl("STATISTICS", "bigFont.fnt", 0.6f, CYAN);
    title->setPosition({0, 168});
    m_pageNode->addChild(title);

    struct Box { std::string v, cap; };
    std::vector<Box> boxes = {
        {std::to_string(s.played), "Played"},
        {std::to_string(s.winPct()), "Win %"},
        {std::to_string(s.streak), "Streak"},
        {std::to_string(s.maxStreak), "Best"},
        {s.wins ? fmt::format("{:.1f}", s.avgGuesses()) : "-", "Avg"},
    };
    for (size_t i = 0; i < boxes.size(); ++i) {
        float x = -160.f + 80.f * i;
        auto card = rounded({72, 66}, CARD);
        card->setPosition({x, 118});
        m_pageNode->addChild(card);
        auto v = lbl(boxes[i].v, "bigFont.fnt", 0.62f);
        v->setPosition({x, 127});
        m_pageNode->addChild(v);
        auto c = lbl(boxes[i].cap, "chatFont.fnt", 0.5f, MUTED);
        c->setPosition({x, 100});
        m_pageNode->addChild(c);
    }

    auto dh = lbl("GUESS DISTRIBUTION", "bigFont.fnt", 0.42f, WHITE);
    dh->setPosition({0, 66});
    m_pageNode->addChild(dh);

    int maxCount = *std::max_element(s.dist.begin(), s.dist.end());
    int highlight = (m_game.finished && m_game.won) ? static_cast<int>(m_game.guesses.size()) : 0;
    for (int i = 0; i < 6; ++i) {
        float y = 40.f - 29.f * i;
        auto n = lbl(std::to_string(i + 1), "bigFont.fnt", 0.45f, MUTED);
        n->setPosition({-172, y});
        m_pageNode->addChild(n);
        float w = maxCount ? std::max(26.f, 300.f * s.dist[i] / maxCount) : 26.f;
        auto bar = rounded({w, 22}, (i + 1 == highlight) ? GREEN : ccColor3B{70, 90, 125});
        bar->setAnchorPoint({0.f, 0.5f});
        bar->setPosition({-152, y});
        m_pageNode->addChild(bar);
        auto c = lbl(std::to_string(s.dist[i]), "bigFont.fnt", 0.38f);
        c->setAnchorPoint({1.f, 0.5f});
        c->setPosition({-152 + w - 8, y});
        m_pageNode->addChild(c);
    }

    auto wl = lbl("Wins " + std::to_string(s.wins) + "    Losses " + std::to_string(s.losses()) +
                  "    Completed " + std::to_string(s.played), "chatFont.fnt", 0.6f, MUTED);
    wl->setPosition({0, -138});
    m_pageNode->addChild(wl);
    addButton(m_pageNode, {0, -184}, {230, 34}, "Game History", PURPLE, [this] { goTo(Page::History); }, 0.5f);
}

// ---------------------------------------------------------------- Achievements
void GeodleLayer::buildAchievements() {
    auto const& defs = ach::all();
    auto have = save::unlockedAchievements();
    int unlockedCount = 0;
    for (auto const& d : defs) unlockedCount += have.count(d.id) ? 1 : 0;

    auto title = lbl("ACHIEVEMENTS", "bigFont.fnt", 0.6f, GOLD);
    title->setPosition({0, 170});
    m_pageNode->addChild(title);
    auto cnt = lbl(std::to_string(unlockedCount) + " / " + std::to_string(defs.size()) + " unlocked",
                   "chatFont.fnt", 0.55f, MUTED);
    cnt->setPosition({0, 150});
    m_pageNode->addChild(cnt);

    constexpr int PER = 12;
    int pages = std::max(1, (static_cast<int>(defs.size()) + PER - 1) / PER);
    m_sub = std::clamp(m_sub, 0, pages - 1);
    for (int i = 0; i < PER; ++i) {
        int idx = m_sub * PER + i;
        if (idx >= static_cast<int>(defs.size())) break;
        auto const& d = defs[idx];
        bool un = have.count(d.id) > 0;
        float x = (i % 2 == 0) ? -98.f : 98.f;
        float y = 116.f - 56.f * (i / 2);

        auto card = rounded({192, 52}, un ? ccColor3B{36, 42, 62} : ccColor3B{24, 26, 38});
        card->setPosition({x, y});
        m_pageNode->addChild(card);
        if (un) {
            auto edge = rounded({192, 52}, d.color, 70);
            edge->setPosition({x, y});
            m_pageNode->addChild(edge);
        }
        auto icon = ach::makeIcon(d, un, 36.f);
        icon->setPosition({x - 78, y});
        m_pageNode->addChild(icon);

        auto n = lbl(d.name, "bigFont.fnt", 0.34f, un ? WHITE : ccColor3B{130, 134, 150});
        n->setAnchorPoint({0.f, 0.5f});
        float maxW = 128.f;
        if (n->getContentWidth() * 0.34f > maxW) n->setScale(maxW / n->getContentWidth());
        n->setPosition({x - 54, y + 14});
        m_pageNode->addChild(n);
        para(m_pageNode, d.description, {x - 54, y + 5}, 0.34f, 128.f, un ? ccColor3B{190, 196, 220} : ccColor3B{105, 108, 125});
    }
    addPager(pages);
}

// ---------------------------------------------------------------- Help
void GeodleLayer::buildHelp() {
    save::setTutorialSeen();
    auto title = lbl("HOW TO PLAY", "bigFont.fnt", 0.6f, BLUE);
    title->setPosition({0, 170});
    m_pageNode->addChild(title);

    float L = -190.f, W = 380.f;
    para(m_pageNode, "Guess the hidden Geometry Dash word in 6 tries. Every guess must be a valid 5-letter word. Press Enter to submit.", {L, 155}, 0.5f, W);
    para(m_pageNode, "After each guess the tiles change color:", {L, 108}, 0.5f, W, CYAN);

    struct Ex { char const* word; int idx; ccColor3B col; char const* text; };
    Ex ex[3] = {
        {"DEMON", 0, GREEN, "D is in the word and in the correct spot."},
        {"SPIKE", 2, YELLOW, "I is in the word, but in the wrong spot."},
        {"WAVES", 0, GRAY, "W is not in the word at all."},
    };
    for (int e = 0; e < 3; ++e) {
        float y = 72.f - 44.f * e;
        for (int i = 0; i < 5; ++i)
            miniTile(m_pageNode, {-165.f + 31.f * i, y}, 28.f, ex[e].word[i], i == ex[e].idx ? ex[e].col : ccColor3B{40, 44, 60});
        para(m_pageNode, ex[e].text, {-78, y + 10}, 0.48f, 265.f);
    }
    para(m_pageNode, "A new word appears every day at 00:00 UTC and everyone gets the same one. Words don't repeat until the whole list has been used.", {L, -62}, 0.48f, W);
    para(m_pageNode, "Type with your keyboard or click the keys. Backspace deletes, Esc or F8 closes Geodle.", {L, -108}, 0.48f, W);
    para(m_pageNode, "Win to build a daily streak and unlock achievements!", {L, -142}, 0.48f, W, GOLD);

    addButton(m_pageNode, {0, -200}, {200, 36}, "Got it!", GREEN, [this] { goTo(Page::Game); }, 0.55f);
}

// ---------------------------------------------------------------- History
void GeodleLayer::buildHistory() {
    auto title = lbl("GAME HISTORY", "bigFont.fnt", 0.6f, PURPLE);
    title->setPosition({0, 170});
    m_pageNode->addChild(title);

    auto hist = save::loadHistory();
    std::reverse(hist.begin(), hist.end());
    if (hist.empty()) {
        auto l = lbl("No games yet. Play today's Geodle!", "chatFont.fnt", 0.6f, MUTED);
        l->setPosition({0, 40});
        m_pageNode->addChild(l);
        auto l2 = lbl("Future answers are never revealed.", "chatFont.fnt", 0.5f, MUTED);
        l2->setPosition({0, 15});
        m_pageNode->addChild(l2);
        return;
    }
    constexpr int PER = 8;
    int pages = (static_cast<int>(hist.size()) + PER - 1) / PER;
    m_sub = std::clamp(m_sub, 0, pages - 1);
    for (int i = 0; i < PER; ++i) {
        int idx = m_sub * PER + i;
        if (idx >= static_cast<int>(hist.size())) break;
        auto const& e = hist[idx];
        float y = 140.f - 38.f * i;
        auto card = rounded({380, 32}, CARD);
        card->setPosition({0, y});
        m_pageNode->addChild(card);
        auto stripe = rounded({8, 32}, e.won ? GREEN : RED);
        stripe->setPosition({-186, y});
        m_pageNode->addChild(stripe);

        auto d = lbl(logic::dateString(e.day), "chatFont.fnt", 0.55f, MUTED);
        d->setAnchorPoint({0.f, 0.5f});
        d->setPosition({-170, y});
        m_pageNode->addChild(d);
        auto num = lbl("#" + std::to_string(logic::puzzleNumber(e.day)), "chatFont.fnt", 0.5f, MUTED);
        num->setPosition({-62, y});
        m_pageNode->addChild(num);
        auto w = lbl(e.word, "bigFont.fnt", 0.42f);
        w->setPosition({30, y});
        m_pageNode->addChild(w);
        auto r = lbl((e.won ? std::to_string(e.guesses) : std::string("X")) + "/6", "bigFont.fnt", 0.45f, e.won ? GREEN : RED);
        r->setAnchorPoint({1.f, 0.5f});
        r->setPosition({178, y});
        m_pageNode->addChild(r);
    }
    addPager(pages);
}

// ---------------------------------------------------------------- Game
CCPoint GeodleLayer::tilePos(int r, int c) const {
    return {(c - 2) * PITCH, GRID_TOP_Y - r * PITCH};
}

void GeodleLayer::styleTile(int r, int c, TileState st, char ch) {
    auto& t = m_tiles[r][c];
    if (!t.node) return;
    ccColor3B border = TILE_BORDER, fill = TILE_FILL;
    switch (st) {
        case TileState::Empty: break;
        case TileState::Typed: border = TILE_TYPED; break;
        case TileState::Absent: border = fill = GRAY; break;
        case TileState::Present: border = fill = YELLOW; break;
        case TileState::Correct: border = fill = GREEN; break;
    }
    t.border->setColor(border);
    t.fill->setColor(fill);
    t.lbl->setString(ch ? std::string(1, ch).c_str() : "");
}

void GeodleLayer::buildGame() {
    auto title = lbl("DAILY GEODLE #" + std::to_string(logic::puzzleNumber(m_today)), "bigFont.fnt", 0.45f, CYAN);
    title->setPosition({0, 181});
    m_pageNode->addChild(title);
    m_countdownLabel = lbl("Next Geodle in " + logic::countdown(logic::secondsUntilNextDay()), "chatFont.fnt", 0.5f, MUTED);
    m_countdownLabel->setPosition({0, 164});
    m_pageNode->addChild(m_countdownLabel);

    // grid
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 5; ++c) {
            auto& t = m_tiles[r][c];
            t.node = CCNode::create();
            t.node->setContentSize({TILE, TILE});
            t.node->setAnchorPoint({0.5f, 0.5f});
            t.node->setPosition(tilePos(r, c));
            t.node->setTag(r * 10 + c);
            t.border = rounded({TILE, TILE}, TILE_BORDER);
            t.border->setPosition({TILE / 2, TILE / 2});
            t.node->addChild(t.border);
            t.fill = rounded({TILE - 4, TILE - 4}, TILE_FILL);
            t.fill->setPosition({TILE / 2, TILE / 2});
            t.node->addChild(t.fill);
            t.lbl = lbl("", "bigFont.fnt", 0.6f);
            t.lbl->setPosition({TILE / 2, TILE / 2 + 1});
            t.node->addChild(t.lbl);
            m_pageNode->addChild(t.node, 1);
        }
    }

    // on-screen keyboard
    auto makeKey = [this](char ch, float x, float y) {
        auto n = addButton(m_pageNode, {x, y}, {34, 34}, std::string(1, ch), KEY_IDLE,
                           [this, ch] { typeLetter(ch); }, 0.42f);
        m_keys[ch] = Key{n, static_cast<CCScale9Sprite*>(n->getChildByTag(1))};
    };
    char const* r1 = "QWERTYUIOP";
    char const* r2 = "ASDFGHJKL";
    char const* r3 = "ZXCVBNM";
    for (int i = 0; i < 10; ++i) makeKey(r1[i], -192.5f + 17.f + 39.f * i, -125.f);
    for (int i = 0; i < 9; ++i) makeKey(r2[i], -173.f + 17.f + 39.f * i, -164.f);
    for (int i = 0; i < 7; ++i) makeKey(r3[i], -117.f + 39.f * i, -203.f);
    addButton(m_pageNode, {-167, -203}, {56, 34}, "ENTER", ccColor3B{40, 150, 170}, [this] { submit(); }, 0.32f);
    addButton(m_pageNode, {167, -203}, {56, 34}, "DEL", ccColor3B{150, 60, 80}, [this] { deleteLetter(); }, 0.38f);

    // restore saved progress without animation
    for (size_t r = 0; r < m_game.guesses.size() && r < 6; ++r) {
        auto const& g = m_game.guesses[r];
        auto marks = logic::evaluate(g, m_answer);
        for (int c = 0; c < 5; ++c) {
            auto st = marks[c] == logic::Mark::Correct ? TileState::Correct
                    : marks[c] == logic::Mark::Present ? TileState::Present : TileState::Absent;
            styleTile(static_cast<int>(r), c, st, g[c]);
        }
        applyKeyStates(g, marks, false);
    }
    refreshCurrentRow();

    if (m_game.finished) {
        addButton(m_pageNode, {152, 164}, {84, 22}, "Results", PURPLE, [this] { showResult(); }, 0.32f);
        this->scheduleOnce(schedule_selector(GeodleLayer::showResultCb), 0.35f);
    }
}

void GeodleLayer::refreshCurrentRow() {
    if (m_page != Page::Game || m_game.finished) return;
    int row = static_cast<int>(m_game.guesses.size());
    if (row >= 6) return;
    for (int c = 0; c < 5; ++c) {
        if (c < static_cast<int>(m_current.size())) styleTile(row, c, TileState::Typed, m_current[c]);
        else styleTile(row, c, TileState::Empty, 0);
    }
}

void GeodleLayer::typeLetter(char ch) {
    if (m_page != Page::Game || m_overlay || m_busy || m_game.finished || m_current.size() >= 5) return;
    m_current.push_back(ch);
    refreshCurrentRow();
    int row = static_cast<int>(m_game.guesses.size());
    auto node = m_tiles[row][m_current.size() - 1].node;
    node->stopAllActions();
    node->setScale(1.f);
    node->runAction(CCSequence::create(CCScaleTo::create(0.06f, 1.12f), CCScaleTo::create(0.06f, 1.f), nullptr));
}

void GeodleLayer::deleteLetter() {
    if (m_page != Page::Game || m_overlay || m_busy || m_game.finished || m_current.empty()) return;
    m_current.pop_back();
    refreshCurrentRow();
}

void GeodleLayer::shakeRow() {
    int row = static_cast<int>(m_game.guesses.size());
    for (int c = 0; c < 5; ++c) {
        auto n = m_tiles[row][c].node;
        n->stopAllActions();
        n->setPosition(tilePos(row, c));
        n->runAction(CCSequence::create(
            CCMoveBy::create(0.05f, {8, 0}), CCMoveBy::create(0.05f, {-16, 0}),
            CCMoveBy::create(0.05f, {16, 0}), CCMoveBy::create(0.05f, {-16, 0}),
            CCMoveBy::create(0.05f, {8, 0}), nullptr));
    }
}

void GeodleLayer::submit() {
    if (m_page != Page::Game || m_overlay || m_busy || m_game.finished) return;
    if (m_current.size() < 5) {
        shakeRow();
        showMessage("Not enough letters");
        return;
    }
    if (!words::isValidGuess(m_current)) {
        shakeRow();
        showMessage("Not in word list");
        return;
    }

    int row = static_cast<int>(m_game.guesses.size());
    std::string guess = m_current;
    m_current.clear();
    m_game.guesses.push_back(guess);
    m_revealMarks = logic::evaluate(guess, m_answer);

    bool won = guess == m_answer;
    bool lost = !won && m_game.guesses.size() >= 6;
    if (won || lost) {
        m_game.finished = true;
        m_game.won = won;
    }
    // Persist immediately: closing Geodle or quitting mid-animation never loses a guess.
    save::storeGame(m_game);
    if (m_game.finished) {
        auto entry = save::recordResult(m_game, m_answer);
        for (auto def : ach::evaluate(save::loadStats(), entry)) m_pendingToasts.push_back(def->id);
    }
    save::flush();

    m_busy = true;
    for (int c = 0; c < 5; ++c) {
        auto n = m_tiles[row][c].node;
        n->stopAllActions();
        n->setScale(1.f);
        auto seq = CCArray::create();
        seq->addObject(CCDelayTime::create(0.28f * c));
        seq->addObject(CCScaleTo::create(0.14f, 1.f, 0.f));
        seq->addObject(CCCallFuncN::create(this, callfuncN_selector(GeodleLayer::onFlipMid)));
        seq->addObject(CCScaleTo::create(0.14f, 1.f, 1.f));
        if (c == 4) seq->addObject(CCCallFunc::create(this, callfunc_selector(GeodleLayer::onRevealDone)));
        n->runAction(CCSequence::create(seq));
    }
}

void GeodleLayer::onFlipMid(CCNode* n) {
    int tag = n->getTag();
    int r = tag / 10, c = tag % 10;
    if (r < 0 || r >= 6 || c < 0 || c >= 5 || r >= static_cast<int>(m_game.guesses.size())) return;
    auto st = m_revealMarks[c] == logic::Mark::Correct ? TileState::Correct
            : m_revealMarks[c] == logic::Mark::Present ? TileState::Present : TileState::Absent;
    styleTile(r, c, st, m_game.guesses[r][c]);
}

void GeodleLayer::onRevealDone() {
    m_busy = false;
    if (m_game.guesses.empty()) return;
    applyKeyStates(m_game.guesses.back(), m_revealMarks, true);

    if (!m_game.finished) return;
    int row = static_cast<int>(m_game.guesses.size()) - 1;
    if (m_game.won) {
        for (int c = 0; c < 5; ++c) {
            m_tiles[row][c].node->runAction(CCSequence::create(
                CCDelayTime::create(0.08f * c), CCJumpBy::create(0.45f, {0, 0}, 14.f, 1), nullptr));
        }
        FMODAudioEngine::sharedEngine()->playEffect("gold01.ogg");
    }
    flushToasts();
    addButton(m_pageNode, {152, 164}, {84, 22}, "Results", PURPLE, [this] { showResult(); }, 0.32f);
    this->scheduleOnce(schedule_selector(GeodleLayer::showResultCb), m_game.won ? 1.1f : 0.6f);
}

void GeodleLayer::refreshKey(char ch, bool bounce) {
    auto it = m_keys.find(ch);
    if (it == m_keys.end()) return;
    int st = m_keyState[ch];
    ccColor3B col = st == 3 ? GREEN : st == 2 ? YELLOW : st == 1 ? KEY_ABSENT : KEY_IDLE;
    it->second.bg->setColor(col);
    if (bounce) {
        auto n = it->second.node;
        n->stopActionByTag(TAG_BOUNCE);
        auto a = CCSequence::create(CCScaleTo::create(0.08f, 1.2f), CCScaleTo::create(0.1f, 1.f), nullptr);
        a->setTag(TAG_BOUNCE);
        n->runAction(a);
    }
}

void GeodleLayer::applyKeyStates(std::string const& guess, logic::Row const& marks, bool bounce) {
    for (int i = 0; i < 5; ++i) {
        int st = static_cast<int>(marks[i]) + 1;
        char ch = guess[i];
        if (st > m_keyState[ch]) m_keyState[ch] = st;
    }
    for (int i = 0; i < 5; ++i) refreshKey(guess[i], bounce);
}

void GeodleLayer::showMessage(std::string const& msg) {
    if (m_msg) m_msg->removeFromParentAndCleanup(true);
    this->unschedule(schedule_selector(GeodleLayer::hideMessage));
    auto n = CCNode::create();
    auto l = lbl(msg, "bigFont.fnt", 0.42f, ccColor3B{20, 22, 32});
    float w = l->getContentWidth() * l->getScale() + 28.f;
    auto bg = rounded({w, 30}, WHITE);
    n->addChild(bg);
    n->addChild(l);
    n->setPosition({0, 150});
    n->setScale(0.7f);
    n->runAction(CCEaseBackOut::create(CCScaleTo::create(0.12f, 1.f)));
    m_pageNode->addChild(n, 200);
    m_msg = n;
    this->scheduleOnce(schedule_selector(GeodleLayer::hideMessage), 1.3f);
}

void GeodleLayer::hideMessage(float) {
    if (m_msg) m_msg->removeFromParentAndCleanup(true);
    m_msg = nullptr;
}

void GeodleLayer::flushToasts() {
    for (auto const& id : m_pendingToasts) ach::toast(id);
    m_pendingToasts.clear();
}

std::string GeodleLayer::shareText() const {
    std::string out = "Geodle #" + std::to_string(logic::puzzleNumber(m_today)) + " " +
                      (m_game.won ? std::to_string(m_game.guesses.size()) : std::string("X")) + "/6\n";
    for (auto const& g : m_game.guesses) {
        out += "\n";
        for (auto m : logic::evaluate(g, m_answer)) {
            out += m == logic::Mark::Correct ? "\xF0\x9F\x9F\xA9"      // green square
                 : m == logic::Mark::Present ? "\xF0\x9F\x9F\xA8"      // yellow square
                                             : "\xE2\xAC\x9B";         // black square
        }
    }
    return out;
}

void GeodleLayer::showResult() {
    if (m_overlay || m_page != Page::Game || !m_game.finished || m_closing) return;
    auto ov = CCNode::create();
    m_pageNode->addChild(ov, 100);
    m_overlay = ov;
    m_hover = nullptr;

    bool won = m_game.won;
    ccColor3B accent = won ? GREEN : RED;

    auto dim = rounded({PW - 8, PH - 8}, {0, 0, 0}, 150);
    ov->addChild(dim);
    auto edge = rounded({344, 264}, accent);
    edge->setPosition({0, 10});
    ov->addChild(edge);
    auto card = rounded({340, 260}, {20, 23, 36});
    card->setPosition({0, 10});
    ov->addChild(card);

    static char const* winTitles[6] = {"EXTREME DEMON!", "INSANE!", "HARDER!", "HARD!", "NORMAL!", "PHEW! CLOSE ONE"};
    int n = static_cast<int>(m_game.guesses.size());
    auto head = lbl(won ? winTitles[std::clamp(n, 1, 6) - 1] : "GAME OVER", "bigFont.fnt", 0.62f, accent);
    head->setPosition({0, 108});
    ov->addChild(head);
    auto sub = lbl(won ? "Solved in " + std::to_string(n) + "/6 guesses" : "The word was...",
                   "chatFont.fnt", 0.6f, MUTED);
    sub->setPosition({0, 80});
    ov->addChild(sub);

    for (int i = 0; i < 5; ++i) {
        float x = -76.f + 38.f * i;
        miniTile(ov, {x, 44}, 34.f, m_answer[i], won ? GREEN : ccColor3B{200, 60, 70});
    }

    auto stats = save::loadStats();
    auto st = lbl("Streak " + std::to_string(stats.streak) + "   |   Best " + std::to_string(stats.maxStreak) +
                  "   |   Win " + std::to_string(stats.winPct()) + "%", "chatFont.fnt", 0.55f, WHITE);
    st->setPosition({0, 6});
    ov->addChild(st);
    m_overlayCountdown = lbl("Next Geodle in " + logic::countdown(logic::secondsUntilNextDay()), "chatFont.fnt", 0.55f, CYAN);
    m_overlayCountdown->setPosition({0, -18});
    ov->addChild(m_overlayCountdown);

    addButton(ov, {-108, -70}, {100, 34}, "Stats", CYAN, [this] { goTo(Page::Stats); }, 0.42f, true);
    addButton(ov, {0, -70}, {100, 34}, "Share", PURPLE, [this] {
        geode::utils::clipboard::write(shareText());
        showMessage("Copied to clipboard!");
    }, 0.42f, true);
    addButton(ov, {108, -70}, {100, 34}, "Board", BLUE, [this] { closeResult(); }, 0.42f, true);
    addButton(ov, {0, -112}, {160, 28}, "Menu", GRAY, [this] { goTo(Page::Home); }, 0.38f, true);

    ov->setScale(0.8f);
    ov->runAction(CCEaseBackOut::create(CCScaleTo::create(0.25f, 1.f)));
}

void GeodleLayer::closeResult() {
    if (!m_overlay) return;
    m_overlay->removeFromParentAndCleanup(true);
    m_overlay = nullptr;
    m_overlayCountdown = nullptr;
    m_modalBtns.clear();
    m_hover = nullptr;
}
