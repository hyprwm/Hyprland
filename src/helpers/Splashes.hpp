#pragma once

#include <vector>
#include <string>

namespace NSplashes {
    inline const std::vector<std::string> SPLASHES = {
        // clang-format off
        "Thanks Brodieeeeeeee!",
        // clang-format on
    };

    inline const std::vector<std::string> SPLASHES_CHRISTMAS = {
        // clang-format off
        "Merry Christmas!",
        "Merry Xmas!",
        "Ho ho ho",
        "Santa was here",
        "Make sure to spend some jolly time with those near and dear to you!",
        "Have you checked for christmas presents yet?",
        // clang-format on
    };

    // ONLY valid near new years.
    inline static int newYear = []() -> int {
        auto tt    = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto local = *localtime(&tt);

        if (local.tm_mon < 8 /* decided with a fair die I promise. */)
            return local.tm_year + 1900;
        return local.tm_year + 1901;
    }();

    inline const std::vector<std::string> SPLASHES_NEWYEAR = {
        // clang-format off
        "Happy new Year!",
        "[New year] will be the year of the Linux desktop!",
        "[New year] will be the year of the Hyprland desktop!",
        std::format("{} will be the year of the Linux desktop!", newYear),
        std::format("{} will be the year of the Hyprland desktop!", newYear),
        std::format("Let's make {} even better than {}!", newYear, newYear - 1),
        // clang-format on
    };
};