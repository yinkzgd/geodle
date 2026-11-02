#include "Words.hpp"
#include <Geode/Geode.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <sstream>
#include <unordered_set>

using namespace geode::prelude;

namespace geodle::words {
namespace {

// ---------------------------------------------------------------------------
// ANSWER POOL - add new Geometry Dash words here (space separated, any case).
// Invalid entries (not exactly 5 letters) are dropped at startup and logged.
// NOTE: adding/removing words changes the daily schedule for everyone, so ship
// pool changes together with a mod update.
// ---------------------------------------------------------------------------
const char* kAnswerData = R"(
DEMON SPIKE CUBES SHARD ROBOT SWING WAVES LEVEL ICONS RATED MUSIC
SHIPS BALLS BLOCK SLOPE TRAIL PULSE SPAWN ALPHA GROUP COLOR SCALE
SHAKE SPEED JUMPS FLIPS CLICK HOLDS BEATS DROPS PAUSE RESET DEATH
CRASH FLASH STARS MOONS COINS DAILY VAULT CHEST TRACK XSTEP CYCLE
ORBIT LASER PIXEL TEMPO SOUND AUDIO SONGS ROLLS FLOAT GHOST SKULL
CHAOS LIGHT BUILD DECOR LAYER GRIDS PLACE TOUCH COUNT TOWER FIRST
CLEAR LOCKS GLOWS BLAST PHASE ARROW SPINS SLIDE POWER SHOCK LEAPS
)";

// Common English words accepted as guesses (answers are always accepted too).
// Drop a bigger list into resources/valid_guesses.txt to widen validation.
const char* kCommonData = R"(
ABOUT ABOVE ABUSE ACTOR ADAPT ADMIT ADOPT ADULT AFTER AGAIN AGENT AGREE AHEAD ALARM ALBUM ALERT ALIKE ALIVE ALLOW ALONE ALONG ALTER AMONG ANGER ANGLE ANGRY APART APPLE APPLY ARENA ARGUE ARISE ASIDE ASSET AVOID AWAKE AWARD AWARE
BADLY BASIC BASIS BEACH BEGAN BEGIN BEING BELOW BENCH BIRTH BLACK BLADE BLAME BLIND BLOOD BOARD BOOST BOUND BRAIN BRAND BREAD BREAK BRING BROAD BROWN BRUSH BUILT BUYER
CABLE CARRY CATCH CAUSE CHAIN CHAIR CHART CHASE CHEAP CHECK CHIEF CHILD CHOSE CIVIL CLAIM CLASS CLEAN CLIMB CLOCK CLOSE CLOUD COACH COAST COULD COURT COVER CRAFT CREAM CRIME CROSS CROWD CROWN CURVE
DANCE DEALT DEBUT DELAY DEPTH DIRTY DOUBT DOZEN DRAFT DRAMA DRAWN DREAM DRESS DRINK DRIVE EARLY EARTH EIGHT ELITE EMPTY ENEMY ENJOY ENTER ENTRY EQUAL ERROR EVENT EVERY EXACT EXIST EXTRA
FAITH FALSE FANCY FAULT FIBER FIELD FIFTH FIFTY FIGHT FINAL FIXED FLOOR FLUID FOCUS FORCE FORTH FORTY FORUM FOUND FRAME FRANK FRAUD FRESH FRONT FRUIT FULLY FUNNY
GIANT GIVEN GLASS GLOBE GOING GRACE GRADE GRAND GRANT GRASS GREAT GREEN GROSS GUARD GUESS GUEST GUIDE HAPPY HARSH HEART HEAVY HENCE HORSE HOTEL HOUSE HUMAN
IDEAL IMAGE INDEX INNER INPUT ISSUE JOINT JUDGE KNOWN LABEL LARGE LATER LAUGH LEARN LEASE LEAST LEAVE LEGAL LIMIT LOCAL LOOSE LOWER LUCKY LUNCH
MAJOR MAKER MARCH MATCH MAYBE MAYOR MEANT MEDIA METAL MIGHT MINOR MIXED MODEL MONEY MONTH MORAL MOTOR MOUNT MOUSE MOUTH MOVIE NEEDS NEVER NIGHT NOISE NORTH NOTED NOVEL NURSE
OCCUR OCEAN OFFER OFTEN ORDER OTHER OUGHT PAINT PANEL PAPER PARTY PEACE PHONE PHOTO PIECE PILOT PITCH PLAIN PLANE PLANT PLATE POINT POUND PRICE PRIDE PRIME PRINT PRIOR PROOF PROUD PROVE
QUEEN QUICK QUIET QUITE RADIO RAISE RANGE RAPID RATIO REACH READY REFER RIGHT RIVAL RIVER ROUGH ROUND ROUTE ROYAL RURAL
SCENE SCOPE SCORE SENSE SERVE SEVEN SHALL SHAPE SHARE SHARP SHEET SHELF SHELL SHIFT SHINE SHIRT SHORT SHOWN SIGHT SINCE SIXTH SIXTY SIZED SKILL SLEEP SMALL SMART SMILE SMOKE SOLID SOLVE SORRY SOUTH SPACE SPARE SPEAK SPEND SPLIT SPORT
STAFF STAGE STAND START STATE STEAM STEEL STICK STILL STOCK STONE STOOD STORE STORM STORY STRIP STUCK STUDY STUFF STYLE SUGAR SUITE SUPER SWEET
TABLE TAKEN TASTE TEACH TEETH THANK THEIR THEME THERE THESE THICK THING THINK THIRD THOSE THREE THREW THROW TIGHT TIMES TIRED TITLE TODAY TOPIC TOTAL TOUGH TRADE TRAIN TREAT TREND TRIAL TRIBE TRICK TRIED TRUCK TRULY TRUST TRUTH TWICE
UNDER UNION UNITY UNTIL UPPER UPSET URBAN USAGE USUAL VALID VALUE VIDEO VIRUS VISIT VITAL VOICE WASTE WATCH WATER WHEEL WHERE WHICH WHILE WHITE WHOLE WHOSE WOMAN WORLD WORRY WORSE WORST WORTH WOULD WRITE WRONG YIELD YOUNG YOUTH
)";

bool clean(std::string& w) {
    if (w.size() != 5) return false;
    for (auto& ch : w) {
        if (!std::isalpha(static_cast<unsigned char>(ch))) return false;
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return true;
}

void tokenize(std::istream& in, std::vector<std::string>& out, bool warn) {
    std::string w;
    while (in >> w) {
        std::string orig = w;
        if (clean(w)) out.push_back(w);
        else if (warn) log::warn("Geodle: dropped invalid word '{}' (must be 5 letters)", orig);
    }
}

struct Data {
    std::vector<std::string> answers;
    std::unordered_set<std::string> valid;
};

Data build() {
    Data d;
    std::istringstream a(kAnswerData);
    tokenize(a, d.answers, true);
    std::sort(d.answers.begin(), d.answers.end());
    d.answers.erase(std::unique(d.answers.begin(), d.answers.end()), d.answers.end());

    d.valid.insert(d.answers.begin(), d.answers.end());

    std::vector<std::string> extra;
    std::istringstream c(kCommonData);
    tokenize(c, extra, true);

    std::ifstream file(Mod::get()->getResourcesDir() / "valid_guesses.txt");
    if (file) tokenize(file, extra, false);
    d.valid.insert(extra.begin(), extra.end());

    log::info("Geodle: {} answers, {} valid guesses", d.answers.size(), d.valid.size());
    return d;
}

Data& data() {
    static Data d = build();
    return d;
}

// splitmix64: identical on every platform/compiler (unlike std::shuffle).
uint64_t splitmix(uint64_t& s) {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

std::vector<size_t> permutation(int cycle, size_t n) {
    std::vector<size_t> p(n);
    std::iota(p.begin(), p.end(), size_t{0});
    uint64_t s = static_cast<uint64_t>(cycle) * 0xD1B54A32D192ED03ull ^ 0xC0FFEEull;
    for (size_t i = n - 1; i > 0; --i) {
        size_t j = static_cast<size_t>(splitmix(s) % (i + 1));
        std::swap(p[i], p[j]);
    }
    return p;
}

} // namespace

std::vector<std::string> const& answers() { return data().answers; }

bool isValidGuess(std::string const& word) {
    return word.size() == 5 && data().valid.count(word) > 0;
}

std::string answerForDay(int day) {
    auto const& pool = data().answers;
    if (pool.empty()) return "DEMON";
    size_t n = pool.size();
    int cycle = day / static_cast<int>(n);
    size_t idx = static_cast<size_t>(day % static_cast<int>(n));
    auto p = permutation(cycle, n);
    // avoid the same word on the last day of one cycle and the first of the next
    if (cycle > 0 && n > 2 && p[0] == permutation(cycle - 1, n).back()) std::swap(p[0], p[1]);
    return pool[p[idx]];
}

} // namespace geodle::words
