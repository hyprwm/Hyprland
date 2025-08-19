#include <cmath>
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

static int  ret = 0;

static bool spawnKitty(const std::string& class_) {
    NLog::log("{}Spawning {}", Colors::YELLOW, class_);
    if (!Tests::spawnKitty(class_)) {
        NLog::log("{}Error: {} did not spawn", Colors::RED, class_);
        return false;
    }
    return true;
}

static bool test() {
    NLog::log("{}Testing windows", Colors::GREEN);

    // test on workspace "window"
    NLog::log("{}Switching to workspace `window`", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:window");

    if (!spawnKitty("kitty_A"))
        return false;

    // check kitty properties. One kitty should take the entire screen, as this is smart gaps
    NLog::log("{}Expecting kitty_A to take up the whole screen", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 0,0"), true);
        EXPECT(str.contains("size: 1920,1080"), true);
        EXPECT(str.contains("fullscreen: 0"), true);
    }

    NLog::log("{}Testing window split ratios", Colors::YELLOW);
    {
        const double RATIO   = 1.25;
        const double PERCENT = RATIO / 2.0 * 100.0;
        const int    GAPSIN  = 5;
        const int    GAPSOUT = 20;
        const int    BORDERS = 2 * 2;
        const int    WTRIM   = BORDERS + GAPSIN + GAPSOUT;
        const int    HEIGHT  = 1080 - (BORDERS + (GAPSOUT * 2));
        const int    WIDTH1  = std::round(1920.0 / 2.0 * (2 - RATIO)) - WTRIM;
        const int    WIDTH2  = std::round(1920.0 / 2.0 * RATIO) - WTRIM;

        OK(getFromSocket("/keyword dwindle:default_split_ratio 1.25"));

        if (!spawnKitty("kitty_B"))
            return false;

        NLog::log("{}Expecting kitty_B to take up roughly {}% of screen width", Colors::YELLOW, 100 - PERCENT);
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("size: {},{}", WIDTH1, HEIGHT));

        OK(getFromSocket("/dispatch killwindow activewindow"));
        Tests::waitUntilWindowsN(1);

        NLog::log("{}Inverting the split ratio", Colors::YELLOW);
        OK(getFromSocket("/keyword dwindle:default_split_ratio 0.75"));

        if (!spawnKitty("kitty_B"))
            return false;

        NLog::log("{}Expecting kitty_B to take up roughly {}% of screen width", Colors::YELLOW, PERCENT);
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("size: {},{}", WIDTH2, HEIGHT));

        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        NLog::log("{}Expecting kitty_A to have the same width as the previous kitty_B", Colors::YELLOW);
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("size: {},{}", WIDTH1, HEIGHT));

        OK(getFromSocket("/keyword dwindle:default_split_ratio 1"));
    }

    // open xeyes
    NLog::log("{}Spawning xeyes", Colors::YELLOW);
    getFromSocket("/dispatch exec xeyes");

    NLog::log("{}Keep checking if xeyes spawned", Colors::YELLOW);
    int counter = 0;
    while (Tests::windowCount() != 3) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            EXPECT(Tests::windowCount(), 3);
            return !ret;
        }
    }

    NLog::log("{}Expecting 3 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 3);

    NLog::log("{}Checking props of xeyes", Colors::YELLOW);
    // check some window props of xeyes, try to tile them
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "floating: 1");
        getFromSocket("/dispatch settiled class:XEyes");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        str = getFromSocket("/clients");
        EXPECT_NOT_CONTAINS(str, "floating: 1");
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
