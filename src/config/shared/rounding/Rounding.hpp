#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace Config {
    class CCornersData;

    // Raw radii are clockwise from top-left, before power or geometry limits.
    namespace Rounding {
        constexpr int64_t                        MAX_RADIUS = 100;

        std::expected<CCornersData, std::string> parse(std::string_view raw, std::optional<int64_t> max = std::nullopt);
        std::expected<CCornersData, std::string> parseProp(std::string_view raw, const CCornersData& current);
    }
}
