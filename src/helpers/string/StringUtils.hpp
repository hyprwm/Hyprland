#pragma once

#include <cstdint>
#include <string_view>
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/Numeric.hpp>
#include <aquamarine/backend/Backend.hpp>

namespace StringUtils {
    // Whether haystack contains needle, ignoring ASCII case only; all other bytes must match exactly.
    inline bool containsCaseInsensitive(const std::string_view haystack, const std::string_view needle) {
        const auto lower = [](const unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };

        for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
            size_t j = 0;
            while (j < needle.size() && lower(haystack[i + j]) == lower(needle[j]))
                ++j;

            if (j == needle.size())
                return true;
        }

        return false;
    }

    inline std::string backendStr(Aquamarine::eBackendType t) {
        switch (t) {
            case Aquamarine::AQ_BACKEND_DRM: return "drm";
            case Aquamarine::AQ_BACKEND_HEADLESS: return "headless";
            case Aquamarine::AQ_BACKEND_WAYLAND: return "wayland";
            default: break;
        }
        return "?";
    }

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
