#pragma once
#include <string>
#include <vector>

namespace geodle::words {
    // Curated Geometry Dash answers: sorted, unique, validated (exactly 5 letters A-Z).
    std::vector<std::string> const& answers();
    // `word` must be upper-case.
    bool isValidGuess(std::string const& word);
    // Deterministic daily answer. Words never repeat until the whole pool has been used.
    std::string answerForDay(int day);
}
