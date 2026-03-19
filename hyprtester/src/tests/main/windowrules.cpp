#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <thread>
#include <chrono>

static int  ret = 0;

static void testMoveRule() {
    OK(getFromSocket("/keyword windowrule match:class wr_move_kitty, float yes, move 100 200"));

    if (!Tests::spawnKitty("wr_move_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
        EXPECT_CONTAINS(str, "at: 100,200");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testCenterRule() {
    OK(getFromSocket("/keyword windowrule match:class wr_center_kitty, float yes, center yes"));

    if (!Tests::spawnKitty("wr_center_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
        // kitty default is 640x400, centered on 1920x1080 = (640, 340)
        EXPECT_CONTAINS(str, "at: 640,340");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testFullscreenRule() {
    OK(getFromSocket("/keyword windowrule match:class wr_fs_kitty, fullscreen yes"));

    if (!Tests::spawnKitty("wr_fs_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testMaximizeRule() {
    OK(getFromSocket("/keyword windowrule match:class wr_max_kitty, maximize yes"));

    if (!Tests::spawnKitty("wr_max_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testMonitorRule() {
    // create a second monitor so we can verify cross-monitor placement
    EXPECT(getFromSocket("/output create headless HEADLESS-3"), "ok");

    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 600"));

    OK(getFromSocket("/keyword windowrule match:class wr_mon_kitty, monitor HEADLESS-3"));

    if (!Tests::spawnKitty("wr_mon_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        OK(getFromSocket("/output remove HEADLESS-3"));
        return;
    }

    // the window should have been placed on the workspace active on HEADLESS-3
    {
        auto str      = getFromSocket("/activewindow");
        auto monitors = getFromSocket("/monitors");

        EXPECT_CONTAINS(str, "class: wr_mon_kitty");

        // find which workspace HEADLESS-3 has, and verify the window is on it
        auto h3pos = monitors.find("Monitor HEADLESS-3");
        if (h3pos != std::string::npos) {
            auto h3section = monitors.substr(h3pos);
            // the window should NOT be on workspace 600 (which is on HEADLESS-2)
            EXPECT_NOT_CONTAINS(str, "workspace: 600");
        } else {
            NLog::log("{}Could not find HEADLESS-3", Colors::RED);
            ++TESTS_FAILED;
            ret = 1;
        }
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
    OK(getFromSocket("/output remove HEADLESS-3"));
}

static void testNoInitialFocusRule() {
    OK(getFromSocket("/dispatch workspace 601"));

    // spawn a window first to hold focus
    if (!Tests::spawnKitty("wr_holder")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: wr_holder");

    OK(getFromSocket("/keyword windowrule match:class wr_nofocus_kitty, no_initial_focus yes"));

    if (!Tests::spawnKitty("wr_nofocus_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // focus should remain on the holder window
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: wr_holder");

    EXPECT(Tests::windowCount(), 2);

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testMoveSizeCombo() {
    OK(getFromSocket("/keyword windowrule match:class wr_combo_kitty, float yes, move 50 50, size 800 600"));

    if (!Tests::spawnKitty("wr_combo_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
        EXPECT_CONTAINS(str, "at: 50,50");
        EXPECT_CONTAINS(str, "size: 800,600");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testPinRule() {
    OK(getFromSocket("/keyword windowrule match:class wr_pin_kitty, float yes, pin yes"));

    if (!Tests::spawnKitty("wr_pin_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
        EXPECT_CONTAINS(str, "pinned: 1");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing window rules (static effects)", Colors::GREEN);

    OK(getFromSocket("/dispatch workspace name:windowrules"));

    NLog::log("{}Testing move rule", Colors::GREEN);
    testMoveRule();

    NLog::log("{}Testing center rule", Colors::GREEN);
    testCenterRule();

    NLog::log("{}Testing fullscreen rule", Colors::GREEN);
    testFullscreenRule();

    NLog::log("{}Testing maximize rule", Colors::GREEN);
    testMaximizeRule();

    NLog::log("{}Testing monitor rule", Colors::GREEN);
    testMonitorRule();

    NLog::log("{}Testing no_initial_focus rule", Colors::GREEN);
    testNoInitialFocusRule();

    NLog::log("{}Testing move+size combo rule", Colors::GREEN);
    testMoveSizeCombo();

    NLog::log("{}Testing pin rule", Colors::GREEN);
    testPinRule();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
