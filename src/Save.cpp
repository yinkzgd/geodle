#include "Save.hpp"
#include <Geode/Geode.hpp>
#include <algorithm>
#include <sstream>

using namespace geode::prelude;

namespace geodle::save {
namespace {
    std::vector<std::string> split(std::string const& s, char d) {
        std::vector<std::string> out;
        std::string cur;
        std::istringstream in(s);
        while (std::getline(in, cur, d)) if (!cur.empty()) out.push_back(cur);
        return out;
    }
    int toInt(std::string const& s, int def = 0) {
        try { return std::stoi(s); } catch (...) { return def; }
    }
    template <class T> T get(char const* k, T def) { return Mod::get()->getSavedValue<T>(k, def); }
    template <class T> void set(char const* k, T v) { Mod::get()->setSavedValue<T>(k, v); }
}

Stats loadStats() {
    Stats s;
    s.played = get<int>("stats.played", 0);
    s.wins = get<int>("stats.wins", 0);
    s.streak = get<int>("stats.streak", 0);
    s.maxStreak = get<int>("stats.max-streak", 0);
    s.totalWinGuesses = get<int>("stats.win-guesses", 0);
    s.lastWinDay = get<int>("stats.last-win-day", -1);
    s.lastPlayedDay = get<int>("stats.last-played-day", -1);
    auto d = split(get<std::string>("stats.dist", ""), ',');
    for (size_t i = 0; i < 6 && i < d.size(); ++i) s.dist[i] = toInt(d[i]);
    return s;
}

void storeStats(Stats const& s) {
    set<int>("stats.played", s.played);
    set<int>("stats.wins", s.wins);
    set<int>("stats.streak", s.streak);
    set<int>("stats.max-streak", s.maxStreak);
    set<int>("stats.win-guesses", s.totalWinGuesses);
    set<int>("stats.last-win-day", s.lastWinDay);
    set<int>("stats.last-played-day", s.lastPlayedDay);
    std::string d;
    for (size_t i = 0; i < 6; ++i) d += (i ? "," : "") + std::to_string(s.dist[i]);
    set<std::string>("stats.dist", d);
}

void normalizeStreak(int today) {
    auto s = loadStats();
    if (s.streak > 0 && s.lastWinDay < today - 1) {
        s.streak = 0;
        storeStats(s);
    }
}

std::vector<HistoryEntry> loadHistory() {
    std::vector<HistoryEntry> out;
    for (auto const& rec : split(get<std::string>("history", ""), ';')) {
        auto f = split(rec, ':');
        if (f.size() < 4) continue;
        HistoryEntry e;
        e.day = toInt(f[0]);
        e.won = f[1] == "1";
        e.guesses = toInt(f[2]);
        e.word = f[3];
        out.push_back(e);
    }
    return out;
}

static void storeHistory(std::vector<HistoryEntry> const& h) {
    std::string s;
    size_t start = h.size() > 400 ? h.size() - 400 : 0;
    for (size_t i = start; i < h.size(); ++i) {
        auto const& e = h[i];
        s += std::to_string(e.day) + ":" + (e.won ? "1" : "0") + ":" + std::to_string(e.guesses) + ":" + e.word + ";";
    }
    set<std::string>("history", s);
}

GameState loadGame(int today) {
    GameState g;
    g.day = today;
    if (get<int>("game.day", -1) != today) return g;
    g.guesses = split(get<std::string>("game.guesses", ""), ',');
    g.finished = get<bool>("game.finished", false);
    g.won = get<bool>("game.won", false);
    return g;
}

void storeGame(GameState const& g) {
    set<int>("game.day", g.day);
    std::string s;
    for (size_t i = 0; i < g.guesses.size(); ++i) s += (i ? "," : "") + g.guesses[i];
    set<std::string>("game.guesses", s);
    set<bool>("game.finished", g.finished);
    set<bool>("game.won", g.won);
}

HistoryEntry recordResult(GameState const& g, std::string const& answer) {
    auto hist = loadHistory();
    for (auto const& e : hist) if (e.day == g.day) return e; // already recorded

    HistoryEntry entry;
    entry.day = g.day;
    entry.won = g.won;
    entry.guesses = static_cast<int>(g.guesses.size());
    entry.word = answer;

    auto s = loadStats();
    s.played++;
    if (g.won) {
        s.wins++;
        s.dist[std::clamp(entry.guesses, 1, 6) - 1]++;
        s.totalWinGuesses += entry.guesses;
        s.streak = (s.lastWinDay == g.day - 1) ? s.streak + 1 : 1;
        s.maxStreak = std::max(s.maxStreak, s.streak);
        s.lastWinDay = g.day;
    } else {
        s.streak = 0;
    }
    s.lastPlayedDay = g.day;
    storeStats(s);

    hist.push_back(entry);
    storeHistory(hist);
    return entry;
}

std::set<std::string> unlockedAchievements() {
    auto v = split(get<std::string>("achievements", ""), ',');
    return {v.begin(), v.end()};
}

void addUnlocked(std::string const& id) {
    auto s = unlockedAchievements();
    s.insert(id);
    std::string out;
    for (auto const& x : s) out += x + ",";
    set<std::string>("achievements", out);
}

bool tutorialSeen() { return get<bool>("tutorial-seen", false); }
void setTutorialSeen() { set<bool>("tutorial-seen", true); }

void flush() {
    auto r = Mod::get()->saveData();
    (void)r;
}

} // namespace geodle::save
