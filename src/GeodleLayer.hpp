#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Achievements.hpp"
#include "Logic.hpp"
#include "Save.hpp"

class GeodleLayer : public cocos2d::CCLayerColor {
public:
    static GeodleLayer* get();   // the open instance, or nullptr
    static void toggle();        // F8
    bool onKey(cocos2d::enumKeyCodes key, bool repeat); // called from the keyboard hook (key-down only)

protected:
    enum class Page { Home, Game, Stats, Achievements, Help, History };
    enum class TileState { Empty, Typed, Absent, Present, Correct };

    struct Btn {
        geode::Ref<cocos2d::CCNode> node;
        cocos2d::CCSize size;
        std::function<void()> cb;
    };
    struct Tile {
        cocos2d::CCNode* node = nullptr;
        cocos2d::extension::CCScale9Sprite* border = nullptr;
        cocos2d::extension::CCScale9Sprite* fill = nullptr;
        cocos2d::CCLabelBMFont* lbl = nullptr;
    };
    struct Key {
        cocos2d::CCNode* node = nullptr;
        cocos2d::extension::CCScale9Sprite* bg = nullptr;
    };

    static GeodleLayer* create();
    bool init() override;
    ~GeodleLayer() override;

    // lifecycle / layout
    void applyLayout(bool initial);
    void update(float dt) override;
    void tick(float dt);
    void close();
    void finishClose();
    void onNewDay();

    // touch
    bool ccTouchBegan(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override;
    void ccTouchEnded(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override;
    void ccTouchCancelled(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override;
    Btn* hitTest(cocos2d::CCPoint world);
    std::vector<Btn>& activeBtns();
    cocos2d::CCNode* addButton(cocos2d::CCNode* parent, cocos2d::CCPoint pos, cocos2d::CCSize size,
                               std::string const& text, cocos2d::ccColor3B col,
                               std::function<void()> cb, float fontScale = 0.5f, bool modal = false);

    // pages
    void goTo(Page p);
    void showPage(Page p);
    void buildHeader();
    cocos2d::CCNode* makeLogo(float scale, bool animate);
    void buildHome();
    void buildGame();
    void buildStats();
    void buildAchievements();
    void buildHelp();
    void buildHistory();
    void addPager(int pages);

    // game
    void styleTile(int r, int c, TileState st, char ch);
    cocos2d::CCPoint tilePos(int r, int c) const;
    void refreshCurrentRow();
    void typeLetter(char ch);
    void deleteLetter();
    void submit();
    void shakeRow();
    void onFlipMid(cocos2d::CCNode* tile);
    void onRevealDone();
    void refreshKey(char ch, bool bounce);
    void applyKeyStates(std::string const& guess, geodle::logic::Row const& marks, bool bounce);
    void showMessage(std::string const& msg);
    void hideMessage(float);
    void showResult();
    void showResultCb(float) { showResult(); }
    void closeResult();
    void flushToasts();
    std::string shareText() const;

    // state
    Page m_page = Page::Home;
    int m_sub = 0;
    int m_today = 0;
    std::string m_answer;
    geodle::save::GameState m_game;
    std::string m_current;
    bool m_busy = false;
    bool m_closing = false;
    float m_scale = 1.f;
    cocos2d::CCSize m_lastWin{0, 0};
    geodle::logic::Row m_revealMarks{};
    std::vector<std::string> m_pendingToasts;

    // nodes (owned by the scene graph; reset in showPage)
    cocos2d::CCNode* m_root = nullptr;
    cocos2d::CCNode* m_pageNode = nullptr;
    cocos2d::CCNode* m_overlay = nullptr;
    cocos2d::CCNode* m_msg = nullptr;
    cocos2d::CCLabelBMFont* m_countdownLabel = nullptr;
    cocos2d::CCLabelBMFont* m_overlayCountdown = nullptr;
    std::array<std::array<Tile, 5>, 6> m_tiles{};
    std::map<char, Key> m_keys;
    std::map<char, int> m_keyState; // 0 unknown, 1 absent, 2 present, 3 correct

    std::vector<Btn> m_btns;
    std::vector<Btn> m_modalBtns;
    geode::Ref<cocos2d::CCNode> m_pressed;
    cocos2d::CCNode* m_hover = nullptr;
};
