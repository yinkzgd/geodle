#include "Logic.hpp"
#include <ctime>
#include <algorithm>
#include <cstdio>

namespace geodle::logic {

    static constexpr int kLaunchDay = 20758; // 2026-11-01

    Row evaluate(std::string const& guess, std::string const& answer) {
        Row r;
        r.fill(Mark::Absent);
        int remaining[26] = {};
        for (int i = 0; i < 5; ++i) {
            if (guess[i] == answer[i]) r[i] = Mark::Correct;
            else remaining[answer[i] - 'A']++;
        }
        for (int i = 0; i < 5; ++i) {
            if (r[i] == Mark::Correct) continue;
            int& n = remaining[guess[i] - 'A'];
            if (n > 0) { r[i] = Mark::Present; --n; }
        }
        return r;
    }

    int today() {
        return static_cast<int>(static_cast<long long>(std::time(nullptr)) / 86400);
    }

    int secondsUntilNextDay() {
        long long t = static_cast<long long>(std::time(nullptr));
        return static_cast<int>(86400 - (t % 86400));
    }

    std::string countdown(int s) {
        s = std::max(0, s);
        char buf[16];
        std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
        return buf;
    }

    std::string dateString(int day) {
        // Howard Hinnant's civil_from_days
        long long z = static_cast<long long>(day) + 719468;
        long long era = (z >= 0 ? z : z - 146096) / 146097;
        long long doe = z - era * 146097;
        long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        long long y = yoe + era * 400;
        long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        long long mp = (5 * doy + 2) / 153;
        long long d = doy - (153 * mp + 2) / 5 + 1;
        long long m = mp < 10 ? mp + 3 : mp - 9;
        if (m <= 2) ++y;
        char buf[48];
        std::snprintf(buf, sizeof buf, "%04lld-%02lld-%02lld", y, m, d);
        return buf;
    }

    int puzzleNumber(int day) {
        return std::max(1, day - kLaunchDay + 1);
    }
}
