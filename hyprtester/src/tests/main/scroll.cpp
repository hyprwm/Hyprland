#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int  ret = 0;

static void testFocusCycling() {
    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch focuswindow class:a"));

    OK(getFromSocket("/dispatch movefocus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch movefocus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    OK(getFromSocket("/dispatch movefocus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch movewindow l"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch movefocus u"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing Scroll layout", Colors::GREEN);

    // setup
    OK(getFromSocket("/dispatch workspace name:scroll"));
    OK(getFromSocket("/keyword general:layout scrolling"));

    // test
    NLog::log("{}Testing focus cycling", Colors::GREEN);
    testFocusCycling();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
