#include "Moderation/Duration.h"

#include <cctype>
#include <utility>
#include <vector>

namespace {
    std::optional<int64_t> unitSeconds(char unit) {
        switch (std::tolower(static_cast<unsigned char>(unit))) {
            case 's':
                return 1;
            case 'm':
                return 60;
            case 'h':
                return 60 * 60;
            case 'd':
                return 24 * 60 * 60;
            case 'w':
                return 7 * 24 * 60 * 60;
            default:
                return std::nullopt;
        }
    }
}

std::optional<int64_t> Duration::parse(const std::string &text) {
    int64_t total = 0;
    size_t index = 0;
    while (index < text.size()) {
        if (std::isspace(static_cast<unsigned char>(text[index]))) {
            ++index;
            continue;
        }

        int64_t value = 0;
        size_t digits = 0;
        while (index < text.size() && std::isdigit(static_cast<unsigned char>(text[index]))) {
            value = value * 10 + (text[index] - '0');
            if (value > 1000000)
                return std::nullopt;
            ++index;
            ++digits;
        }

        if (digits == 0 || index >= text.size())
            return std::nullopt;

        const std::optional<int64_t> unit = unitSeconds(text[index]);
        if (!unit.has_value())
            return std::nullopt;

        total += value * *unit;
        ++index;
    }

    if (total <= 0)
        return std::nullopt;

    return total;
}

std::string Duration::format(int64_t seconds) {
    const std::vector<std::pair<int64_t, const char *>> units = {
        {7 * 24 * 60 * 60, "w"},
        {24 * 60 * 60, "d"},
        {60 * 60, "h"},
        {60, "m"},
        {1, "s"}
    };

    std::string result;
    for (const auto &[size, suffix] : units) {
        if (seconds < size)
            continue;

        if (!result.empty())
            result += ' ';
        result += std::to_string(seconds / size) + suffix;
        seconds %= size;
    }
    return result.empty() ? "0s" : result;
}
