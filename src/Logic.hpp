#pragma once
#include <array>
#include <string>

namespace geodle::logic {

    enum class Mark : int { Absent = 0, Present = 1, Correct = 2 };
    using Row = std::array<Mark, 5>;

    // Standard Wordle scoring, duplicate letters handled with a two-pass algorithm.
    Row evaluate(std::string const& guess, std::string const& answer);

    // Days since 1970-01-01 (UTC). Everyone on the same UTC day gets the same puzzle.
    int today();
    int secondsUntilNextDay();
    std::string countdown(int seconds);   // HH:MM:SS
    std::string dateString(int day);      // YYYY-MM-DD
    int puzzleNumber(int day);            // 2026-11-01 is #1
}
