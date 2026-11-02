#pragma once
#include <array>
#include <set>
#include <string>
#include <vector>

namespace geodle::save {

    struct Stats {
        int played = 0;
        int wins = 0;
        int streak = 0;
        int maxStreak = 0;
        int totalWinGuesses = 0;
        int lastWinDay = -1;
        int lastPlayedDay = -1;
        std::array<int, 6> dist{};

        int losses() const { return played - wins; }
        int winPct() const { return played ? (wins * 100 + played / 2) / played : 0; }
        double avgGuesses() const { return wins ? double(totalWinGuesses) / wins : 0.0; }
    };

    struct HistoryEntry {
        int day = 0;
        bool won = false;
        int guesses = 0;
        std::string word;
    };

    struct GameState {
        int day = 0;
        std::vector<std::string> guesses;
        bool finished = false;
        bool won = false;
    };

    Stats loadStats();
    void storeStats(Stats const& s);
    // Resets the streak if a daily puzzle was missed.
    void normalizeStreak(int today);

    std::vector<HistoryEntry> loadHistory();

    // Returns the saved game for `today`, or a fresh one if the saved day differs.
    GameState loadGame(int today);
    void storeGame(GameState const& g);

    // Updates stats + history exactly once per day. Returns the history entry.
    HistoryEntry recordResult(GameState const& g, std::string const& answer);

    std::set<std::string> unlockedAchievements();
    void addUnlocked(std::string const& id);

    bool tutorialSeen();
    void setTutorialSeen();

    void flush(); // write save data to disk now (crash safety)
}
