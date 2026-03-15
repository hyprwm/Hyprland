#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int ret = 0;

// reqs 1 master 3 slaves
static void testOrientations() {
    OK(getFromSocket("/keyword master:orientation top"));

    // top
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876");
    }

    // cycle = top, right, bottom, center, left

    // right
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 873,22");
        EXPECT_CONTAINS(str, "size: 1025,1036");
    }

    // bottom
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,495");
        EXPECT_CONTAINS(str, "size: 1876");
    }

    // center
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 450,22");
        EXPECT_CONTAINS(str, "size: 1020,1036");
    }

    // left
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1025,1036");
    }
}

static void focusMasterPrevious() {
    // setup
    NLog::log("{}Spawning 1 master and 3 slave windows", Colors::YELLOW);
    // order of windows set according to new_status = master (set in test.conf)
    for (auto const& win : {"slave1", "slave2", "slave3", "master"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }
    NLog::log("{}Ensuring focus is on master before testing", Colors::YELLOW);
    OK(getFromSocket("/dispatch layoutmsg focusmaster master"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    // test
    NLog::log("{}Testing fallback to focusmaster auto", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: slave1");

    NLog::log("{}Testing focusing from slave to master", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg cyclenext noloop"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: slave2");
    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    NLog::log("{}Testing focusing on previous window", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: slave2");

    NLog::log("{}Testing focusing back to master", Colors::YELLOW);

    OK(getFromSocket("/dispatch layoutmsg focusmaster previous"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    testOrientations();

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testFsBehavior() {
    // Master will re-send data to fullscreen / maximized windows, which can interfere with misc:on_focus_under_fullscreen
    // check that it doesn't.

    for (auto const& win : {"master", "slave1", "slave2"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch focuswindow class:master"));
    OK(getFromSocket("/dispatch fullscreen 1"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: master");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 1"));

    Tests::spawnKitty("new_master");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: new_master");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 0"));

    Tests::spawnKitty("ignored");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: new_master");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 2"));

    Tests::spawnKitty("vaxwashere");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: vaxwashere");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testMfact() {
    OK(getFromSocket("r/keyword general:gaps_in 0"));
    OK(getFromSocket("r/keyword general:gaps_out 0"));
    OK(getFromSocket("r/keyword general:border_size 0"));
    OK(getFromSocket("/keyword master:orientation left"));
    OK(getFromSocket("/keyword master:mfact 0.7"));

    OK(getFromSocket("/dispatch workspace 849"));

    for (auto const& win : {"mf_slave", "mf_master"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // master should take 70% of 1920 = 1344
    OK(getFromSocket("/dispatch focuswindow class:mf_master"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 1344,1080");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    // now test with mfact 0.3 on a fresh workspace
    OK(getFromSocket("/keyword master:mfact 0.3"));
    OK(getFromSocket("/dispatch workspace 848"));

    for (auto const& win : {"mf2_slave", "mf2_master"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch focuswindow class:mf2_master"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 576,1080");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

static void testNewStatusSlave() {
    // use a fresh workspace to avoid stale layout state
    OK(getFromSocket("/dispatch workspace 850"));
    OK(getFromSocket("/keyword general:layout master"));
    OK(getFromSocket("/keyword master:new_status slave"));

    if (!Tests::spawnKitty("nss_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // first window always becomes master regardless of new_status
    OK(getFromSocket("/dispatch layoutmsg focusmaster master"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: nss_a");

    for (auto const& win : {"nss_b", "nss_c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // nss_a should still be the master
    OK(getFromSocket("/dispatch layoutmsg focusmaster master"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: nss_a");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/keyword master:new_status master"));
}

static void testCenterMaster() {
    // use fresh workspace to avoid stale layout state
    OK(getFromSocket("/dispatch workspace 851"));
    OK(getFromSocket("/keyword general:layout master"));
    OK(getFromSocket("r/keyword general:gaps_in 0"));
    OK(getFromSocket("r/keyword general:gaps_out 0"));
    OK(getFromSocket("r/keyword general:border_size 0"));
    OK(getFromSocket("/keyword master:orientation center"));
    OK(getFromSocket("/keyword master:slave_count_for_center_master 2"));

    // with new_status=master, last spawned is master
    for (auto const& win : {"cm_slave1", "cm_slave2", "cm_master"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // with center orientation and 2 slaves, master should be in the center
    OK(getFromSocket("/dispatch layoutmsg focusmaster master"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cm_master");
        // master should not be at x=0 (left edge), it should be centered
        EXPECT_NOT_CONTAINS(str, "at: 0,0");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

static bool test() {
    NLog::log("{}Testing Master layout", Colors::GREEN);

    // setup
    OK(getFromSocket("/dispatch workspace name:master"));
    OK(getFromSocket("/keyword general:layout master"));

    // test
    NLog::log("{}Testing `focusmaster previous` layoutmsg", Colors::GREEN);
    focusMasterPrevious();

    NLog::log("{}Testing fs behavior", Colors::GREEN);
    testFsBehavior();

    NLog::log("{}Testing mfact", Colors::GREEN);
    testMfact();

    NLog::log("{}Testing new_status slave", Colors::GREEN);
    testNewStatusSlave();

    NLog::log("{}Testing center_master orientation", Colors::GREEN);
    testCenterMaster();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
