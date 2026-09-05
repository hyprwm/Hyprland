#pragma once

#include <cstdint>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/Numeric.hpp>

namespace StringUtils {
    inline bool truthy(const std::string_view sv) {
        if (sv.starts_with("true") || sv.starts_with("yes") || sv.starts_with("on"))
            return true;

        if (Hyprutils::String::isNumber2(sv)) {
            auto n = Hyprutils::String::strToNumber<int64_t>(sv);

            return n && *n != 0;
        }

        return false;
    }

    inline const char* huParseErrorToString(Hyprutils::String::eNumericParseResult r) {
        switch (r) {
            case Hyprutils::String::NUMERIC_PARSE_OK: return "ok";
            case Hyprutils::String::NUMERIC_PARSE_GARBAGE: return "bad input";
            case Hyprutils::String::NUMERIC_PARSE_BAD: return "bad input";
            case Hyprutils::String::NUMERIC_PARSE_OUT_OF_RANGE: return "out of range";
        }

        return "error";
    }
}
