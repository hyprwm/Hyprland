#pragma once

#include <expected>
#include <cstdint>
#include <string>

namespace Config::ParserUtils {
    std::expected<int64_t, std::string> parseColor(std::string_view val);
    std::expected<int64_t, std::string> parseInt(std::string_view val);
};