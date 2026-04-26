#pragma once

#include "../SharedDefs.hpp"
#include "../helpers/Color.hpp"

#include <string>
#include <array>

namespace Notification {
    enum eIconBackend : uint8_t {
        ICONS_BACKEND_NONE = 0,
        ICONS_BACKEND_NF,
        ICONS_BACKEND_FA
    };

    static const std::array<std::array<std::string, ICON_NONE + 1>, 3 /* backends */> ICONS_ARRAY = {
        std::array<std::string, ICON_NONE + 1>{"[!]", "[i]", "[Hint]", "[Err]", "[?]", "[ok]", ""},
        std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", "󰸞", ""}, std::array<std::string, ICON_NONE + 1>{"", "", "", "", "", ""}};
    static const std::array<CHyprColor, ICON_NONE + 1> ICONS_COLORS = {CHyprColor{1.0, 204 / 255.0, 102 / 255.0, 1.0},
                                                                       CHyprColor{128 / 255.0, 255 / 255.0, 255 / 255.0, 1.0},
                                                                       CHyprColor{179 / 255.0, 255 / 255.0, 204 / 255.0, 1.0},
                                                                       CHyprColor{255 / 255.0, 77 / 255.0, 77 / 255.0, 1.0},
                                                                       CHyprColor{255 / 255.0, 204 / 255.0, 153 / 255.0, 1.0},
                                                                       CHyprColor{128 / 255.0, 255 / 255.0, 128 / 255.0, 1.0},
                                                                       CHyprColor{0, 0, 0, 1.0}};
}
