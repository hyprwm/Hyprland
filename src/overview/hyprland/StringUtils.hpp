#pragma once

#include <cstdint>
#include <algorithm>
#include <string_view>

#include "../../helpers/memory/Memory.hpp"

namespace Overview::Hyprland::StringUtils {
    inline uint8_t foldASCII(uint8_t character) {
        if (character >= 'A' && character <= 'Z')
            return character + ('a' - 'A');
        return character;
    }

    inline bool matchesName(std::string_view name, std::string_view query) {
        if (query.empty())
            return true;

        return std::ranges::search(name, query, [](char lhs, char rhs) { return foldASCII(sc<unsigned char>(lhs)) == foldASCII(sc<unsigned char>(rhs)); }).begin() != name.end();
    }

    inline bool fullMatchCaseIns(std::string_view a, std::string_view b) {
        return std::ranges::equal(a, b, [](char x, char y) { return foldASCII(x) == foldASCII(y); });
    }
};
