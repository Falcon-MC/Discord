#pragma once

#include <cstdint>
#include <optional>
#include <string>

class Duration {
public:
    static std::optional<int64_t> parse(const std::string &text);
    static std::string format(int64_t seconds);
};
