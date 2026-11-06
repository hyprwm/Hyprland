#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace Config {
    class CCornersData;

    // Raw radii are clockwise from top-left, before power or geometry limits.
    namespace Rounding {
        std::expected<CCornersData, std::string> parse(std::string_view raw, bool clampNegative = false);
        std::expected<CCornersData, std::string> parseProp(std::string_view raw, const CCornersData& current);
    }
}
