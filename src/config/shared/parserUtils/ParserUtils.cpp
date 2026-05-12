#include "ParserUtils.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/Numeric.hpp>

#include <format>
#include <algorithm>
#include <cmath>

#include "../../../helpers/memory/Memory.hpp"

using namespace Config;
using namespace Config::ParserUtils;
using namespace Hyprutils::String;

static std::expected<uint64_t, std::string> parseHex(std::string_view value) {
    auto res = value.starts_with("0x") ? strToNumber<uint64_t>(value) : strToNumber<uint64_t>(std::string{"0x"} + value);
    if (!res)
        return std::unexpected(std::format("invalid hex \"{}\"", value));
    return *res;
}

std::expected<int64_t, std::string> ParserUtils::parseColor(std::string_view val) {

    if (val.starts_with("#")) {
        // parse either rgb or rgba
        val = val.substr(1);

        if (val.length() != 6 && val.length() != 8 && val.length() != 3)
            return std::unexpected(std::format("couldn't parse \"{}\" as a color", val));

        if (val.length() == 3) {
            auto r = parseHex(val.substr(0, 1));
            auto g = parseHex(val.substr(1, 1));
            auto b = parseHex(val.substr(2, 1));

            if (!r || !g || !b)
                return std::unexpected(std::format("couldn't parse \"{}\" as a color (bad hex)", val));

            return 0xFF000000 | ((*r | (*r << 4)) << 16) | ((*g | (*g << 4)) << 8) | (*b | (*b << 4));
        }

        if (val.length() == 6) {
            auto r = parseHex(val.substr(0, 2));
            auto g = parseHex(val.substr(2, 2));
            auto b = parseHex(val.substr(4, 2));

            if (!r || !g || !b)
                return std::unexpected(std::format("couldn't parse \"{}\" as a color (bad hex)", val));

            return 0xFF000000 | (*r << 16) | (*g << 8) | *b;
        }

        if (val.length() == 8) {
            auto r = parseHex(val.substr(0, 2));
            auto g = parseHex(val.substr(2, 2));
            auto b = parseHex(val.substr(4, 2));
            auto a = parseHex(val.substr(6, 2));

            if (!r || !g || !b || !a)
                return std::unexpected(std::format("couldn't parse \"{}\" as a color (bad hex)", val));

            return (*a << 24) | (*r << 16) | (*g << 8) | *b;
        }
    }

    if (val.starts_with("0x"))
        return parseHex(val);

    if (val.starts_with("rgba(") && val.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(val.substr(5, val.length() - 6));

        // try doing it the comma way first
        if (std::ranges::count(VALUEWITHOUTFUNC, ',') == 3) {
            // cool
            std::string_view rolling = VALUEWITHOUTFUNC;
            auto             r       = strToNumber<uint8_t>(trim(rolling.substr(0, rolling.find(','))));
            rolling                  = rolling.substr(rolling.find(',') + 1);
            auto g                   = strToNumber<uint8_t>(trim(rolling.substr(0, rolling.find(','))));
            rolling                  = rolling.substr(rolling.find(',') + 1);
            auto b                   = strToNumber<uint8_t>(trim(rolling.substr(0, rolling.find(','))));
            rolling                  = rolling.substr(rolling.find(',') + 1);
            auto a                   = strToNumber<float>(trim(rolling.substr(0, rolling.find(','))));

            if (!r || !g || !b || !a)
                return std::unexpected(std::format("failed parsing \"{}\" as a color", val));

            return (sc<uint64_t>(std::floor(*a * 255.F)) << 24) | (*r << 16) | (*g << 8) | *b;
        } else if (VALUEWITHOUTFUNC.length() == 8) {
            const auto RGBA = parseHex(VALUEWITHOUTFUNC);

            if (!RGBA)
                return RGBA;
            // now we need to RGBA -> ARGB. The config holds ARGB only.
            return (*RGBA >> 8) + (0x1000000 * (*RGBA & 0xFF));
        }

        return std::unexpected("rgba() expects length of 8 characters (4 bytes) or 4 comma separated values");
    }

    if (val.starts_with("rgb(") && val.ends_with(')')) {
        const auto VALUEWITHOUTFUNC = trim(val.substr(4, val.length() - 5));

        // try doing it the comma way first
        if (std::ranges::count(VALUEWITHOUTFUNC, ',') == 2) {
            // cool
            std::string_view rolling = VALUEWITHOUTFUNC;
            auto             r       = strToNumber<uint8_t>(trim(rolling.substr(0, rolling.find(','))));
            rolling                  = rolling.substr(rolling.find(',') + 1);
            auto g                   = strToNumber<uint8_t>(trim(rolling.substr(0, rolling.find(','))));
            rolling                  = rolling.substr(rolling.find(',') + 1);
            auto b                   = strToNumber<uint8_t>(trim(rolling.substr(0, rolling.find(','))));

            if (!r || !g || !b)
                return std::unexpected(std::format("failed parsing \"{}\" as a color", val));

            return 0xFF000000 | (*r << 16) | (*g << 8) | *b;
        } else if (VALUEWITHOUTFUNC.length() == 6) {
            const auto r = parseHex(VALUEWITHOUTFUNC);
            return r ? *r + 0xFF000000 : r;
        }

        return std::unexpected("rgb() expects length of 6 characters (3 bytes) or 3 comma separated values");
    }

    if (isNumber2(val)) {
        if (const auto v = strToNumber<uint32_t>(val); v)
            return *v;
    }

    return std::unexpected(std::format("cannot parse \"{}\" as a color", val));
}

std::expected<int64_t, std::string> ParserUtils::parseInt(std::string_view val) {
    if (val.starts_with("0x"))
        return parseHex(val);

    if (val.starts_with("true") || val.starts_with("on") || val.starts_with("yes"))
        return 1;

    if (val.starts_with("false") || val.starts_with("off") || val.starts_with("no"))
        return 0;

    auto res = strToNumber<int64_t>(val);

    if (!res)
        return std::unexpected(std::format("Failed to parse \"{}\" as an integer", val));

    return *res;
}
