#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int  ret = 0;

static void testSingleWindow() {
    if (!Tests::spawnKitty("mono_a")) {
        NLog::log("{}Failed to spawn kitty", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // single window should fill the entire work area (gaps + borders apply)
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: mono_a");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
    }

    Tests::killAllWindows();
}

static void testMultipleWindowsOverlap() {
    for (auto const& win : {"mono_a", "mono_b", "mono_c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // all windows should occupy the same position and size (stacked)
    auto clients = getFromSocket("/clients");
    EXPECT_COUNT_STRING(clients, "at: 22,22", 3);
    EXPECT_COUNT_STRING(clients, "size: 1876,1036", 3);

    // the last spawned window should be focused
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_c");

    Tests::killAllWindows();
}

static void testCycleNext() {
    for (auto const& win : {"mono_a", "mono_b", "mono_c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_c");

    OK(getFromSocket("/dispatch layoutmsg cyclenext"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_a");

    OK(getFromSocket("/dispatch layoutmsg cyclenext"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_b");

    OK(getFromSocket("/dispatch layoutmsg cyclenext"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_c");

    Tests::killAllWindows();
}

static void testCyclePrev() {
    for (auto const& win : {"mono_a", "mono_b", "mono_c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_c");

    OK(getFromSocket("/dispatch layoutmsg cycleprev"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_b");

    OK(getFromSocket("/dispatch layoutmsg cycleprev"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_a");

    // wraps around
    OK(getFromSocket("/dispatch layoutmsg cycleprev"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_c");

    Tests::killAllWindows();
}

static void testFocusWindowSwitch() {
    for (auto const& win : {"mono_a", "mono_b", "mono_c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // focusing a specific window should bring it to the visible slot
    OK(getFromSocket("/dispatch focuswindow class:mono_a"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_a");

    OK(getFromSocket("/dispatch focuswindow class:mono_b"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_b");

    Tests::killAllWindows();
}

static void testWindowRemoval() {
    for (auto const& win : {"mono_a", "mono_b", "mono_c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // focus b, then c, then kill c -- should fall back to b (last focused)
    OK(getFromSocket("/dispatch focuswindow class:mono_b"));
    OK(getFromSocket("/dispatch focuswindow class:mono_c"));
    OK(getFromSocket("/dispatch killactive"));
    Tests::waitUntilWindowsN(2);

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mono_b");

    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing Monocle layout", Colors::GREEN);

    OK(getFromSocket("/dispatch workspace name:monocle"));
    OK(getFromSocket("/keyword general:layout monocle"));

    NLog::log("{}Testing single window", Colors::GREEN);
    testSingleWindow();

    NLog::log("{}Testing multiple windows overlap", Colors::GREEN);
    testMultipleWindowsOverlap();

    NLog::log("{}Testing cyclenext", Colors::GREEN);
    testCycleNext();

    NLog::log("{}Testing cycleprev", Colors::GREEN);
    testCyclePrev();

    NLog::log("{}Testing focus window switch", Colors::GREEN);
    testFocusWindowSwitch();

    NLog::log("{}Testing window removal fallback", Colors::GREEN);
    testWindowRemoval();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
