#pragma once

// Plain date helpers with no Geode dependency, so they can be tested on their own.

#include <array>
#include <chrono>
#include <cstdio>
#include <optional>
#include <string>

namespace date_utils {

// Parses "2026-10-23T08:13:51.349Z" (UTC) into a time point.
inline std::optional<std::chrono::sys_seconds> parseIso8601(std::string const& s) {
    using namespace std::chrono;
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    int got = std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &se);
    if (got < 3) return std::nullopt;

    year_month_day ymd{ year{ y }, month{ static_cast<unsigned>(mo) }, day{ static_cast<unsigned>(d) } };
    if (!ymd.ok()) return std::nullopt;

    return sys_days{ ymd } + hours{ h } + minutes{ mi } + seconds{ se };
}

// Whole days elapsed, never negative.
inline int daysBetween(std::chrono::sys_seconds from, std::chrono::system_clock::time_point to) {
    using namespace std::chrono;
    auto diff = floor<seconds>(to) - from;
    if (diff.count() < 0) return 0;
    return static_cast<int>(floor<days>(diff).count());
}

inline std::string withCommas(int n) {
    std::string digits = std::to_string(n);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count > 0 && count % 3 == 0) out.insert(out.begin(), ',');
        out.insert(out.begin(), *it);
        ++count;
    }
    return out;
}

inline std::string daysAgoText(int days) {
    if (days <= 0) return "today";
    if (days == 1) return "1 day ago";
    return withCommas(days) + " days ago";
}

// format is one of the values offered in mod.json's "date-format" setting.
inline std::string formatDate(std::chrono::sys_seconds t, std::string const& format) {
    using namespace std::chrono;
    static constexpr std::array<char const*, 12> names = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    year_month_day ymd{ floor<days>(t) };
    int y = static_cast<int>(ymd.year());
    unsigned m = static_cast<unsigned>(ymd.month());
    unsigned d = static_cast<unsigned>(ymd.day());

    char buf[32];
    if (format == "YYYY-MM-DD") {
        std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", y, m, d);
    } else if (format == "MM/DD/YYYY") {
        std::snprintf(buf, sizeof buf, "%02u/%02u/%04d", m, d, y);
    } else if (format == "DD/MM/YYYY") {
        std::snprintf(buf, sizeof buf, "%02u/%02u/%04d", d, m, y);
    } else { // "DD Mon YYYY"
        std::snprintf(buf, sizeof buf, "%u %s %d", d, names[m - 1], y);
    }
    return buf;
}

} // namespace date_utils
