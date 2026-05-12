#pragma once

#include <vector>
#include <string>

namespace NSplashes {
    inline const std::vector<std::string> SPLASHES = {
        // clang-format off
        "Woo, animations!",
        "It's like Hypr, but better.",
        "Release 1.0 when?",
        "It's not awesome, it's Hyprland!",
        "\"I commit too often, people can't catch up lmao\" - Vaxry",
        "This text is random.",
        "\"There are reasons to not use rust.\" - Boga",
        "Read the wiki.",
        "\"Hello everyone this is YOUR daily dose of ‘read the wiki’\" - Vaxry",
        "h",
        "\"‘why no work’, bro I haven't hacked your pc to get live feeds yet\" - Vaxry",
        "Compile, wait for 20 minutes, notice a new commit, compile again.",
        "To rice, or not to rice, that is the question.",
        "Now available on Fedora!",
        "\"Hyprland is so good it starts with a capital letter\" - Hazel",
        "\"please make this message a splash\" - eriedaberrie",
        "\"the only wayland compositor powered by fried chicken\" - raf",
        "\"This will never get into Hyprland\" - Flafy",
        "\"Hyprland only gives you up on -git\" - fazzi",
        "Segmentation fault (core dumped)",
        "\"disabling hyprland logo is a war crime\" - vaxry",
        "some basic startup code",
        "\"I think I am addicted to hyprland\" - mathisbuilder",
        "\"hyprland is the most important package in the arch repos\" - jacekpoz",
        "Thanks Brodie!",
        "Thanks fufexan!",
        "Thanks raf!",
        "You can't use --splash to change this message :)",
        "Hyprland will overtake Gnome in popularity by [insert year]",
        "Designed in California - Assembled in China",
        "\"something <time here> and still no new splash\" - snowman",
        "My name is Land. Hypr Land. One red bull, shaken not stirred.",
        "\"Glory To The Emperor\" - raf",
        "Help I forgot to install kitty",
        "Go to settings to activate Hyprland",
        "Why is there code??? Make a damn .exe file and give it to me.",
        "Hyprland is not a window manager!",
        "Can we get a version without anime girls?",
        "Check out quickshell!",
        "A day without Hyprland is a day wasted",
        "By dt, do you mean damage tracking or distrotube?",
        "Made in Poland",
        "\"I use Arch, btw\" - John Cena",
        R"("Hyper".replace("e", ""))",
        "\"my win11 install runs hyprland that is true\" - raf",
        "\"stop playing league loser\" - hyprBot",
        "\"If it ain't broke, don't fix it\" - Lucascito_03",
        "\"@vaxry how do i learn c++\" - flicko",
        "Join the discord server!",
        "Thanks ThatOneCalculator!",
        "The AUR packages always work, except for the times they don't.",
        "Funny animation compositor woo",
        "4 years!",
        "Beauty will save the world", // 4th ricing comp winner - zacoons' choice
        "Configration",
        "RIP hyprlang",
        "better call vaxry",
        "i hypr therefore i land",
        "five. hundred. config errors.",
        "bundled with anime girls for your convenience",
        "now with 200% more hypr and land",
        "daily dose of rice",
        "Removed Herobrine",
        "You should try quickshell!",
        "He was an X11 boy, she was a Wayland girl...",
        "How do I exit vim????",
        "Now with lua, it might as well be awesome.",
        "Have you ran your daily fastfetch yet?"
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