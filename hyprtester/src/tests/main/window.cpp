#include <unistd.h>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/string/VarList2.hpp>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

static int  ret = 0;

static bool spawnKitty(const std::string& class_, const std::vector<std::string>& args = {}) {
    NLog::log("{}Spawning {}", Colors::YELLOW, class_);
    if (!Tests::spawnKitty(class_, args)) {
        NLog::log("{}Error: {} did not spawn", Colors::RED, class_);
        return false;
    }
    return true;
}

/// Spawns a kitty and creates a file and returns its name. The removal of the file triggers
/// activation of the spawned kitty window.
///
/// On failure, returns an empty string, possibly leaving a temporary file.
static std::string spawnKittyActivating(const std::string& class_ = "kitty_activating") {
    // `XXXXXX` is what `mkstemp` expects to find in the string
    std::string tmpFilename = (std::filesystem::temp_directory_path() / "XXXXXX").string();
    int         fd          = mkstemp(tmpFilename.data());
    if (fd < 0) {
        NLog::log("{}Error: could not create tmp file: errno {}", Colors::RED, errno);
        return "";
    }
    (void)close(fd);
    bool ok =
        spawnKitty(class_, {"-o", "allow_remote_control=yes", "--", "/bin/sh", "-c", "while [ -f \"" + tmpFilename + "\" ]; do :; done; kitten @ focus-window; sleep infinity"});
    if (!ok) {
        NLog::log("{}Error: failed to spawn kitty", Colors::RED);
        return "";
    }
    return tmpFilename;
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

static void testGroupRules() {
    NLog::log("{}Testing group window rules", Colors::YELLOW);

    OK(getFromSocket("/keyword general:border_size 8"));
    OK(getFromSocket("/keyword workspace w[tv1], bordersize:0"));
    OK(getFromSocket("/keyword workspace f[1], bordersize:0"));
    OK(getFromSocket("/keyword windowrule match:workspace w[tv1], border_size 0"));
    OK(getFromSocket("/keyword windowrule match:workspace f[1], border_size 0"));

    if (!Tests::spawnKitty("kitty_A")) {
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    if (!Tests::spawnKitty("kitty_B")) {
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "8");
    }

    OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
    OK(getFromSocket("/dispatch togglegroup"));
    OK(getFromSocket("/dispatch focuswindow class:kitty_B"));
    OK(getFromSocket("/dispatch moveintogroup l"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    OK(getFromSocket("/dispatch changegroupactive f"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    if (!Tests::spawnKitty("kitty_C")) {
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch moveoutofgroup r"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "8");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static bool isActiveWindow(const std::string& class_, char fullscreen = '0', bool log = true) {
    std::string activeWin     = getFromSocket("/activewindow");
    auto        winClass      = getWindowAttribute(activeWin, "class:");
    auto        winFullscreen = getWindowAttribute(activeWin, "fullscreen:").back();
    if (winClass.substr(strlen("class: ")) == class_ && winFullscreen == fullscreen)
        return true;
    else {
        if (log)
            NLog::log("{}Wrong active window: expected class {} fullscreen '{}', found class {}, fullscreen '{}'", Colors::RED, class_, fullscreen, winClass, winFullscreen);
        return false;
    }
}

static bool waitForActiveWindow(const std::string& class_, char fullscreen = '0', bool logLastCheck = true, int maxTries = 50) {
    int cnt = 0;
    while (!isActiveWindow(class_, fullscreen, false)) {
        ++cnt;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (cnt > maxTries) {
            return isActiveWindow(class_, fullscreen, logLastCheck);
        }
    }
    return true;
}

/// Tests behavior of a window being focused when on that window's workspace
/// another fullscreen window exists.
static bool testWindowFocusOnFullscreenConflict() {
    if (!spawnKitty("kitty_A"))
        return false;
    if (!spawnKitty("kitty_B"))
        return false;

    OK(getFromSocket("/keyword misc:focus_on_activate true"));

    // Unfullscreen on conflict
    {
        OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 2"));

        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        OK(getFromSocket("/dispatch fullscreen 0 set"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus the same window
        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus a different window
        OK(getFromSocket("/dispatch focuswindow class:kitty_B"));
        EXPECT(isActiveWindow("kitty_B", '0'), true);

        // Make a window that will request focus
        const std::string removeToActivate = spawnKittyActivating();
        if (removeToActivate.empty())
            return false;
        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        OK(getFromSocket("/dispatch fullscreen 0 set"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);
        std::filesystem::remove(removeToActivate);
        EXPECT(waitForActiveWindow("kitty_activating", '0'), true);
        OK(getFromSocket("/dispatch forcekillactive"));
        Tests::waitUntilWindowsN(2);
    }

    // Take over on conflict
    {
        OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 1"));

        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        OK(getFromSocket("/dispatch fullscreen 0 set"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus the same window
        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus a different window
        OK(getFromSocket("/dispatch focuswindow class:kitty_B"));
        EXPECT(isActiveWindow("kitty_B", '2'), true);
        OK(getFromSocket("/dispatch fullscreenstate 0 0"));

        // Make a window that will request focus
        const std::string removeToActivate = spawnKittyActivating();
        if (removeToActivate.empty())
            return false;
        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        OK(getFromSocket("/dispatch fullscreen 0 set"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);
        std::filesystem::remove(removeToActivate);
        EXPECT(waitForActiveWindow("kitty_activating", '2'), true);
        OK(getFromSocket("/dispatch forcekillactive"));
        Tests::waitUntilWindowsN(2);
    }

    // Keep the old focus on conflict
    {
        OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 0"));

        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        OK(getFromSocket("/dispatch fullscreen 0 set"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus the same window
        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Make a window that will request focus - the setting is treated normally
        const std::string removeToActivate = spawnKittyActivating();
        if (removeToActivate.empty())
            return false;
        OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
        OK(getFromSocket("/dispatch fullscreen 0 set"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);
        std::filesystem::remove(removeToActivate);
        EXPECT(waitForActiveWindow("kitty_A", '2'), true);
    }

    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return true;
}

static void testMaximizeSize() {
    NLog::log("{}Testing maximize size", Colors::GREEN);

    EXPECT(spawnKitty("kitty_A"), true);

    // check kitty properties. Maximizing shouldnt change its size
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,22"), true);
        EXPECT(str.contains("size: 1876,1036"), true);
        EXPECT(str.contains("fullscreen: 0"), true);
    }

    OK(getFromSocket("/dispatch fullscreen 1"));

    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,22"), true);
        EXPECT(str.contains("size: 1876,1036"), true);
        EXPECT(str.contains("fullscreen: 1"), true);
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);
}

static void testFloatingFocusOnFullscreen() {
    NLog::log("{}Testing floating focus on fullscreen", Colors::GREEN);

    EXPECT(spawnKitty("kitty_A"), true);
    OK(getFromSocket("/dispatch togglefloating"));

    EXPECT(spawnKitty("kitty_B"), true);
    OK(getFromSocket("/dispatch fullscreen 1"));

    OK(getFromSocket("/dispatch cyclenext"));

    OK(getFromSocket("/dispatch plugin:test:floating_focus_on_fullscreen"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);
}

static void testGroupFallbackFocus() {
    NLog::log("{}Testing group fallback focus", Colors::GREEN);

    EXPECT(spawnKitty("kitty_A"), true);

    OK(getFromSocket("/dispatch togglegroup"));

    EXPECT(spawnKitty("kitty_B"), true);
    EXPECT(spawnKitty("kitty_C"), true);
    EXPECT(spawnKitty("kitty_D"), true);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_D"), true);
    }

    OK(getFromSocket("/dispatch focuswindow class:kitty_B"));
    OK(getFromSocket("/dispatch focuswindow class:kitty_D"));
    OK(getFromSocket("/dispatch killactive"));

    Tests::waitUntilWindowsN(3);

    // Focus must return to the last focus, in this case B.
    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_B"), true);
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);
}

static void testBringActiveToTopMouseMovement() {
    NLog::log("{}Testing bringactivetotop mouse movement", Colors::GREEN);

    Tests::killAllWindows();
    OK(getFromSocket("/keyword input:follow_mouse 2"));
    OK(getFromSocket("/keyword input:float_switch_override_focus 0"));

    EXPECT(spawnKitty("a"), true);
    OK(getFromSocket("/dispatch setfloating"));
    OK(getFromSocket("/dispatch movewindowpixel exact 500 300,activewindow"));
    OK(getFromSocket("/dispatch resizewindowpixel exact 400 400,activewindow"));

    EXPECT(spawnKitty("b"), true);
    OK(getFromSocket("/dispatch setfloating"));
    OK(getFromSocket("/dispatch movewindowpixel exact 500 300,activewindow"));
    OK(getFromSocket("/dispatch resizewindowpixel exact 400 400,activewindow"));

    auto getTopWindow = []() -> std::string {
        auto clients = getFromSocket("/clients");
        return (clients.rfind("class: a") > clients.rfind("class: b")) ? "a" : "b";
    };

    EXPECT(getTopWindow(), std::string("b"));
    OK(getFromSocket("/dispatch movecursor 700 500"));

    OK(getFromSocket("/dispatch focuswindow class:a"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: a");

    OK(getFromSocket("/dispatch bringactivetotop"));
    EXPECT(getTopWindow(), std::string("a"));

    OK(getFromSocket("/dispatch plugin:test:click 272,1"));
    OK(getFromSocket("/dispatch plugin:test:click 272,0"));

    EXPECT(getTopWindow(), std::string("a"));

    Tests::killAllWindows();
}

static void testInitialFloatSize() {
    NLog::log("{}Testing initial float size", Colors::GREEN);

    Tests::killAllWindows();
    OK(getFromSocket("/keyword windowrule match:class kitty, float yes"));
    OK(getFromSocket("/keyword input:float_switch_override_focus 0"));

    EXPECT(spawnKitty("kitty"), true);

    {
        // Kitty by default opens as 640x400, if this changes this test will break
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("size: 640,400"), true);
    }

    OK(getFromSocket("/reload"));

    Tests::killAllWindows();

    OK(getFromSocket("/dispatch exec [float yes]kitty"));

    Tests::waitUntilWindowsN(1);

    {
        // Kitty by default opens as 640x400, if this changes this test will break
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("size: 640,400"), true);
        EXPECT(str.contains("floating: 1"), true);
    }

    Tests::killAllWindows();
}

/// Tests that the `focus_on_activate` effect of window rules always overrides
/// the `misc:focus_on_activate` variable.
static bool testWindowRuleFocusOnActivate() {
    OK(getFromSocket("/reload"));

    if (!spawnKitty("kitty_default")) {
        NLog::log("{}Error: failed to spawn kitty", Colors::RED);
        return false;
    }

    // Do not focus anyone automatically
    ///////////OK(getFromSocket("/keyword windowrule match:class .*, no_initial_focus true"));

    // `focus_on_activate off` takes over
    {
        OK(getFromSocket("/keyword misc:focus_on_activate true"));
        OK(getFromSocket("/keyword windowrule match:class kitty_antifocus, focus_on_activate off"));

        const std::string removeToActivate = spawnKittyActivating("kitty_antifocus");
        if (removeToActivate.empty()) {
            return false;
        }
        EXPECT(waitForActiveWindow("kitty_antifocus"), true);
        OK(getFromSocket("/dispatch focuswindow class:kitty_default"));
        EXPECT(isActiveWindow("kitty_default"), true);

        std::filesystem::remove(removeToActivate);
        // The focus should NOT transition, since the window rule explicitly forbids that
        EXPECT(waitForActiveWindow("kitty_antifocus", '0', false), false);
    }

    // `focus_on_activate on` takes over
    {
        OK(getFromSocket("/keyword misc:focus_on_activate false"));
        OK(getFromSocket("/keyword windowrule match:class kitty_superfocus, focus_on_activate on"));

        const std::string removeToActivate = spawnKittyActivating("kitty_superfocus");
        if (removeToActivate.empty()) {
            return false;
        }
        EXPECT(waitForActiveWindow("kitty_superfocus"), true);
        OK(getFromSocket("/dispatch focuswindow class:kitty_default"));
        EXPECT(isActiveWindow("kitty_default"), true);

        std::filesystem::remove(removeToActivate);
        // Now that we requested activation, the focus SHOULD transition to kitty_superfocus, according to the window rule
        EXPECT(waitForActiveWindow("kitty_superfocus"), true);
    }

    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

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
        const double INITIAL_RATIO = 1.25;
        const int    GAPSIN        = 5;
        const int    GAPSOUT       = 20;
        const int    BORDERSIZE    = 2;
        const int    BORDERS       = BORDERSIZE * 2;
        const int    MONITOR_W     = 1920;
        const int    MONITOR_H     = 1080;

        const float  totalAvailableHeight   = MONITOR_H - (GAPSOUT * 2);
        const int    HEIGHT                 = std::floor(totalAvailableHeight) - BORDERS;
        const float  availableWidthForSplit = MONITOR_W - (GAPSOUT * 2) - GAPSIN;

        auto         calculateFinalWidth = [&](double boxWidth, bool isLeftWindow) {
            double gapLeft  = isLeftWindow ? GAPSOUT : GAPSIN;
            double gapRight = isLeftWindow ? GAPSIN : GAPSOUT;
            return std::floor(boxWidth - gapLeft - gapRight - BORDERS);
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

        try {
            NLog::log("{}Expecting kitty_B size: {},{}", Colors::YELLOW, WIDTH2, HEIGHT);

            {
                auto data = getFromSocket("/activewindow");
                data      = data.substr(data.find("size:") + 5);
                data      = data.substr(0, data.find('\n'));

                Hyprutils::String::CVarList2 sizes(std::move(data), 0, ',');

                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[0]}), WIDTH2, 2);
                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[1]}), HEIGHT, 2);
            }

            OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
            NLog::log("{}Expecting kitty_A size: {},{}", Colors::YELLOW, WIDTH_A_FINAL, HEIGHT);

            {
                auto data = getFromSocket("/activewindow");
                data      = data.substr(data.find("size:") + 5);
                data      = data.substr(0, data.find('\n'));

                Hyprutils::String::CVarList2 sizes(std::move(data), 0, ',');

                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[0]}), WIDTH_A_FINAL, 2);
                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[1]}), HEIGHT, 2);
            }

        } catch (...) {
            NLog::log("{}Exception thrown", Colors::RED);
            EXPECT(false, true);
        }

        OK(getFromSocket("/keyword dwindle:default_split_ratio 1"));
    }

    // open xeyes
    NLog::log("{}Spawning xeyes", Colors::YELLOW);
    getFromSocket("/dispatch exec xeyes");

    NLog::log("{}Keep checking if xeyes spawned", Colors::YELLOW);
    Tests::waitUntilWindowsN(3);

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

    if (!testWindowFocusOnFullscreenConflict()) {
        ret = 1;
        return false;
    }

    NLog::log("{}Testing spawning a floating window over a fullscreen window", Colors::YELLOW);
    {
        if (!spawnKitty("kitty_A"))
            return false;
        OK(getFromSocket("/dispatch fullscreen 0 set"));
        EXPECT(Tests::windowCount(), 1);

        OK(getFromSocket("/dispatch exec [float] kitty"));
        Tests::waitUntilWindowsN(2);

        OK(getFromSocket("/dispatch focuswindow class:^kitty$"));
        const auto focused1 = getFromSocket("/activewindow");
        EXPECT_CONTAINS(focused1, "class: kitty\n");

        OK(getFromSocket("/dispatch killwindow activewindow"));
        Tests::waitUntilWindowsN(1);

        // The old window should be focused again
        const auto focused2 = getFromSocket("/activewindow");
        EXPECT_CONTAINS(focused2, "class: kitty_A\n");

        NLog::log("{}Killing all windows", Colors::YELLOW);
        Tests::killAllWindows();
    }

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

        // Fuck this test, it's fucking stupid - vax
        // if (!spawnKitty("kitty_maxsize"))
        //     return false;
        // EXPECT_CONTAINS(getFromSocket("/activewindow"), "size: 1200,500");

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

        // FIXME: I can't be arsed.
        OK(getFromSocket("/dispatch focuswindow class:kitty_maxsize"));
        //        EXPECT_CONTAINS(getFromSocket("/activewindow"), "size: 1200,500")

        NLog::log("{}Reloading config", Colors::YELLOW);
        OK(getFromSocket("/reload"));
        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);
    }

    NLog::log("{}Testing minsize/maxsize rules", Colors::YELLOW);
    {
        // Disable size limits tiled and check if props are working and not getting skipped
        OK(getFromSocket("/keyword misc:size_limits_tiled 0"));
        OK(getFromSocket("/keyword windowrule[kitty-max-rule]:match:class kitty_maxsize"));
        OK(getFromSocket("/keyword windowrule[kitty-max-rule]:max_size 1500 500"));
        OK(getFromSocket("r/keyword windowrule[kitty-max-rule]:min_size 1200 500"));
        if (!spawnKitty("kitty_maxsize"))
            return false;

        {
            auto res = getFromSocket("/getprop active max_size");
            EXPECT_CONTAINS(res, "1500");
            EXPECT_CONTAINS(res, "500");
        }

        {
            auto res = getFromSocket("/getprop active min_size");
            EXPECT_CONTAINS(res, "1200");
            EXPECT_CONTAINS(res, "500");
        }

        NLog::log("{}Reloading config", Colors::YELLOW);
        OK(getFromSocket("/reload"));
        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);
    }

    {
        // Set float
        OK(getFromSocket("/keyword windowrule[kitty-max-rule]:match:class kitty_maxsize"));
        OK(getFromSocket("/keyword windowrule[kitty-max-rule]:max_size 1200 500"));
        OK(getFromSocket("r/keyword windowrule[kitty-max-rule]:min_size 1200 500"));
        OK(getFromSocket("r/keyword windowrule[kitty-max-rule]:float yes"));
        if (!spawnKitty("kitty_maxsize"))
            return false;

        {
            auto res = getFromSocket("/getprop active max_size");
            EXPECT_CONTAINS(res, "1200");
            EXPECT_CONTAINS(res, "500");
        }

        {
            auto res = getFromSocket("/getprop active min_size");
            EXPECT_CONTAINS(res, "1200");
            EXPECT_CONTAINS(res, "500");
        }

        {
            auto res = getFromSocket("/activewindow");
            EXPECT_CONTAINS(res, "size: 1200,500");
        }

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

    OK(getFromSocket("/keyword windowrule[border-magic-kitty]:match:class border_kitty"));
    OK(getFromSocket("/keyword windowrule[border-magic-kitty]:border_color rgba(c6ff00ff) rgba(ff0000ee) 45deg"));

    if (!spawnKitty("border_kitty"))
        return false;

    OK(getFromSocket("/dispatch focuswindow class:border_kitty"));

    {
        auto str = getFromSocket("/getprop active active_border_color");
        EXPECT_CONTAINS(str, "ffc6ff00");
        EXPECT_CONTAINS(str, "eeff0000");
        EXPECT_CONTAINS(str, "45deg");
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

    // test persistent_size between floating window launches
    OK(getFromSocket("/keyword windowrule match:class persistent_size_kitty, persistent_size true, float true"));

    if (!spawnKitty("persistent_size_kitty"))
        return false;

    OK(getFromSocket("/dispatch resizeactive exact 600 400"))

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 600,400");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    Tests::killAllWindows();

    if (!spawnKitty("persistent_size_kitty"))
        return false;

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 600,400");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    OK(getFromSocket("/keyword general:border_size 0"));
    OK(getFromSocket("/keyword windowrule match:float true, border_size 10"));

    if (!spawnKitty("border_kitty"))
        return false;

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    OK(getFromSocket("/dispatch togglefloating"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "10");
    }

    OK(getFromSocket("/dispatch togglefloating"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    // test expression rules
    OK(getFromSocket("/keyword windowrule match:class expr_kitty, float yes, size monitor_w*0.5 monitor_h*0.5, min_size monitor_w*0.25 monitor_h*0.25, "
                     "max_size monitor_w*0.75 monitor_h*0.75, move 20+(monitor_w*0.1) monitor_h*0.5"));

    if (!spawnKitty("expr_kitty"))
        return false;

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
        EXPECT_CONTAINS(str, "at: 212,540");
        EXPECT_CONTAINS(str, "size: 960,540");

        auto min = getFromSocket("/getprop active min_size");
        EXPECT_CONTAINS(min, "480");
        EXPECT_CONTAINS(min, "270");

        auto max = getFromSocket("/getprop active max_size");
        EXPECT_CONTAINS(max, "1440");
        EXPECT_CONTAINS(max, "810");
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

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    testGroupRules();
    testMaximizeSize();
    testFloatingFocusOnFullscreen();
    testBringActiveToTopMouseMovement();
    testGroupFallbackFocus();
    testInitialFloatSize();
    testWindowRuleFocusOnActivate();

    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
