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

static std::string getWindowAttribute(const std::string& winInfo, const std::string& attr) {
    auto pos = winInfo.find(attr);
    if (pos == std::string::npos) {
        NLog::log("{}Wrong window attribute", Colors::RED);
        ret = 1;
        return "Wrong window attribute";
    }
    auto pos2 = winInfo.find('\n', pos);
    return winInfo.substr(pos, pos2 - pos);
}

static std::string getWindowAddress(const std::string& winInfo) {
    auto pos  = winInfo.find("Window ");
    auto pos2 = winInfo.find(" -> ");
    if (pos == std::string::npos || pos2 == std::string::npos) {
        NLog::log("{}Wrong window info", Colors::RED);
        ret = 1;
        return "Wrong window info";
    }
    return winInfo.substr(pos + 7, pos2 - pos - 7);
}

static void testSwapWindow() {
    NLog::log("{}Testing swapwindow", Colors::GREEN);

    // test on workspace "swapwindow"
    NLog::log("{}Switching to workspace \"swapwindow\"", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:swapwindow");

    if (!Tests::spawnKitty("kitty_A")) {
        ret = 1;
        return;
    }

    if (!Tests::spawnKitty("kitty_B")) {
        ret = 1;
        return;
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);

    // Test swapwindow by direction
    {
        getFromSocket("/dispatch focuswindow class:kitty_A");
        auto pos = getWindowAttribute(getFromSocket("/activewindow"), "at:");
        NLog::log("{}Testing kitty_A {}, swapwindow with direction 'l'", Colors::YELLOW, pos);

        OK(getFromSocket("/dispatch swapwindow l"));
        OK(getFromSocket("/dispatch focuswindow class:kitty_B"));

        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", pos));
    }

    // Test swapwindow by class
    {
        getFromSocket("/dispatch focuswindow class:kitty_A");
        auto pos = getWindowAttribute(getFromSocket("/activewindow"), "at:");
        NLog::log("{}Testing kitty_A {}, swapwindow with class:kitty_B", Colors::YELLOW, pos);

        OK(getFromSocket("/dispatch swapwindow class:kitty_B"));
        OK(getFromSocket("/dispatch focuswindow class:kitty_B"));

        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", pos));
    }

    // Test swapwindow by address
    {
        getFromSocket("/dispatch focuswindow class:kitty_B");
        auto addr = getWindowAddress(getFromSocket("/activewindow"));
        getFromSocket("/dispatch focuswindow class:kitty_A");
        auto pos = getWindowAttribute(getFromSocket("/activewindow"), "at:");
        NLog::log("{}Testing kitty_A {}, swapwindow with address:0x{}(kitty_B)", Colors::YELLOW, pos, addr);

        OK(getFromSocket(std::format("/dispatch swapwindow address:0x{}", addr)));
        OK(getFromSocket(std::format("/dispatch focuswindow address:0x{}", addr)));

        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", pos));
    }

    NLog::log("{}Testing swapwindow with fullscreen. Expecting to fail", Colors::YELLOW);
    {
        OK(getFromSocket("/dispatch fullscreen"));

        auto str = getFromSocket("/dispatch swapwindow l");
        EXPECT_CONTAINS(str, "Can't swap fullscreen window");

        OK(getFromSocket("/dispatch fullscreen"));
    }

    NLog::log("{}Testing swapwindow with different workspace", Colors::YELLOW);
    {
        getFromSocket("/dispatch focuswindow class:kitty_B");
        auto addr = getWindowAddress(getFromSocket("/activewindow"));
        auto ws   = getWindowAttribute(getFromSocket("/activewindow"), "workspace:");
        NLog::log("{}Sending address:0x{}(kitty_B) to workspace \"swapwindow2\"", Colors::YELLOW, addr);

        OK(getFromSocket("/dispatch movetoworkspacesilent name:swapwindow2"));
        OK(getFromSocket(std::format("/dispatch swapwindow address:0x{}", addr)));
        getFromSocket("/dispatch focuswindow class:kitty_B");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", ws));
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);
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
        const double INITIAL_RATIO = 1.25;
        const int    GAPSIN        = 5;
        const int    GAPSOUT       = 20;
        const int    BORDERSIZE    = 2;
        const int    BORDERS       = BORDERSIZE * 2;
        const int    MONITOR_W     = 1920;
        const int    MONITOR_H     = 1080;

        const float  totalAvailableHeight   = MONITOR_H - (GAPSOUT * 2);
        const int    HEIGHT                 = std::round(totalAvailableHeight) - BORDERS;
        const float  availableWidthForSplit = MONITOR_W - (GAPSOUT * 2) - GAPSIN;

        auto         calculateFinalWidth = [&](double boxWidth, bool isLeftWindow) {
            double gapLeft  = isLeftWindow ? GAPSOUT : GAPSIN;
            double gapRight = isLeftWindow ? GAPSIN : GAPSOUT;
            return std::round(boxWidth - gapLeft - gapRight - BORDERS);
        };

        double       geomBoxWidthA_R1 = (availableWidthForSplit * INITIAL_RATIO / 2.0) + GAPSOUT + (GAPSIN / 2.0);
        double       geomBoxWidthB_R1 = MONITOR_W - geomBoxWidthA_R1;
        const int    WIDTH1           = calculateFinalWidth(geomBoxWidthB_R1, false);

        const double INVERTED_RATIO   = 0.75;
        double       geomBoxWidthA_R2 = (availableWidthForSplit * INVERTED_RATIO / 2.0) + GAPSOUT + (GAPSIN / 2.0);
        double       geomBoxWidthB_R2 = MONITOR_W - geomBoxWidthA_R2;
        const int    WIDTH2           = calculateFinalWidth(geomBoxWidthB_R2, false);
        const int    WIDTH_A_FINAL    = calculateFinalWidth(geomBoxWidthA_R2, true);

        OK(getFromSocket("/keyword dwindle:default_split_ratio 1.25"));

        if (!spawnKitty("kitty_B"))
            return false;

        NLog::log("{}Expecting kitty_B size: {},{}", Colors::YELLOW, WIDTH1, HEIGHT);
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("size: {},{}", WIDTH1, HEIGHT));

        OK(getFromSocket("/dispatch killwindow activewindow"));
        Tests::waitUntilWindowsN(1);

        NLog::log("{}Inverting the split ratio", Colors::YELLOW);
        OK(getFromSocket("/keyword dwindle:default_split_ratio 0.75"));

        if (!spawnKitty("kitty_B"))
            return false;

        NLog::log("{}Expecting kitty_B size: {},{}", Colors::YELLOW, WIDTH2, HEIGHT);
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("size: {},{}", WIDTH2, HEIGHT));

        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        NLog::log("{}Expecting kitty_A size: {},{}", Colors::YELLOW, WIDTH_A_FINAL, HEIGHT);
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("size: {},{}", WIDTH_A_FINAL, HEIGHT));

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

    testSwapWindow();

    getFromSocket("/dispatch workspace 1");

    NLog::log("{}Testing minsize/maxsize rules for tiled windows", Colors::YELLOW);
    {
        // Enable the config for testing, test max/minsize for tiled windows and centering
        OK(getFromSocket("/keyword misc:size_limits_tiled 1"));
        OK(getFromSocket("/keyword windowrule[kitty-max-rule]:match:class kitty_maxsize"));
        OK(getFromSocket("/keyword windowrule[kitty-max-rule]:max_size 1500 500"));
        OK(getFromSocket("r/keyword windowrule[kitty-max-rule]:min_size 1200 500"));
        if (!spawnKitty("kitty_maxsize"))
            return false;

        auto dwindle = getFromSocket("/activewindow");
        EXPECT_CONTAINS(dwindle, "size: 1500,500");
        EXPECT_CONTAINS(dwindle, "at: 210,290");

        if (!spawnKitty("kitty_maxsize"))
            return false;

        EXPECT_CONTAINS(getFromSocket("/activewindow"), "size: 1200,500");

        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);

        OK(getFromSocket("/keyword general:layout master"));

        if (!spawnKitty("kitty_maxsize"))
            return false;

        auto master = getFromSocket("/activewindow");
        EXPECT_CONTAINS(master, "size: 1500,500");
        EXPECT_CONTAINS(master, "at: 210,290");

        if (!spawnKitty("kitty_maxsize"))
            return false;

        OK(getFromSocket("/dispatch focuswindow class:kitty_maxsize"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "size: 1200,500")

        NLog::log("{}Reloading config", Colors::YELLOW);
        OK(getFromSocket("/reload"));
        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);
    }

    NLog::log("{}Testing window rules", Colors::YELLOW);
    if (!spawnKitty("wr_kitty"))
        return false;
    {
        auto      str  = getFromSocket("/activewindow");
        const int SIZE = 200;
        EXPECT_CONTAINS(str, "floating: 1");
        EXPECT_CONTAINS(str, std::format("size: {},{}", SIZE, SIZE));
        EXPECT_NOT_CONTAINS(str, "pinned: 1");
    }

    OK(getFromSocket("/keyword windowrule[wr-kitty-stuff]:opacity 0.5 0.5 override"));

    {
        auto str = getFromSocket("/getprop active opacity");
        EXPECT_CONTAINS(str, "0.5");
    }

    OK(getFromSocket("/keyword windowrule[special-magic-kitty]:match:class magic_kitty"));
    OK(getFromSocket("/keyword windowrule[special-magic-kitty]:workspace special:magic"));

    if (!spawnKitty("magic_kitty"))
        return false;

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "special:magic");
        EXPECT_NOT_CONTAINS(str, "workspace: 9");
    }

    if (auto str = getFromSocket("/monitors"); str.contains("magic)")) {
        OK(getFromSocket("/dispatch togglespecialworkspace magic"));
    }

    Tests::killAllWindows();

    if (!spawnKitty("tag_kitty"))
        return false;

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    // test rules that overlap effects but don't overlap props
    OK(getFromSocket("/keyword windowrule match:class overlap_kitty, border_size 0"));
    OK(getFromSocket("/keyword windowrule match:fullscreen false, border_size 10"));

    if (!spawnKitty("overlap_kitty"))
        return false;

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "10");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    OK(getFromSocket("/dispatch plugin:test:add_rule"));
    OK(getFromSocket("/reload"));

    OK(getFromSocket("/keyword windowrule match:class plugin_kitty, plugin_rule effect"));

    if (!spawnKitty("plugin_kitty"))
        return false;

    OK(getFromSocket("/dispatch plugin:test:check_rule"));

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    OK(getFromSocket("/dispatch plugin:test:add_rule"));
    OK(getFromSocket("/reload"));

    OK(getFromSocket("/keyword windowrule[test-plugin-rule]:match:class plugin_kitty"));
    OK(getFromSocket("/keyword windowrule[test-plugin-rule]:plugin_rule effect"));

    if (!spawnKitty("plugin_kitty"))
        return false;

    OK(getFromSocket("/dispatch plugin:test:check_rule"));

    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
