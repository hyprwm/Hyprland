#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <algorithm>
#include <vector>
#include <thread>
#include "tests.hpp"

TEST_CASE(focusMasterPrevious) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

    // setup
    NLog::log("{}Spawning 1 master and 3 slave windows", Colors::YELLOW);
    // order of windows set according to new_status = master (set in test.lua)
    for (auto const& win : {"slave1", "slave2", "slave3", "master"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }
    NLog::log("{}Ensuring focus is on master before testing", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.layout('focusmaster master')"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    // test
    NLog::log("{}Testing fallback to focusmaster auto", Colors::YELLOW);

    OK(getFromSocket("/dispatch hl.dsp.layout('focusmaster previous')"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: slave1");

    NLog::log("{}Testing focusing from slave to master", Colors::YELLOW);

    OK(getFromSocket("/dispatch hl.dsp.layout('cyclenext noloop')"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: slave2");
    OK(getFromSocket("/dispatch hl.dsp.layout('focusmaster previous')"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    NLog::log("{}Testing focusing on previous window", Colors::YELLOW);

    OK(getFromSocket("/dispatch hl.dsp.layout('focusmaster previous')"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: slave2");

    NLog::log("{}Testing focusing back to master", Colors::YELLOW);

    OK(getFromSocket("/dispatch hl.dsp.layout('focusmaster previous')"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    OK(getFromSocket("r/eval hl.config({ master = { orientation = 'top' } })"));

    // top
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876");
    }

    // cycle = top, right, bottom, center, left

    // right
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 873,22");
        EXPECT_CONTAINS(str, "size: 1025,1036");
    }

    // bottom
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,495");
        EXPECT_CONTAINS(str, "size: 1876");
    }

    // center
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 450,22");
        EXPECT_CONTAINS(str, "size: 1020,1036");
    }

    // left
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1025,1036");
    }
}

TEST_CASE(fsBehavior) {
    // Master will re-send data to fullscreen / maximized windows, which can interfere with misc:on_focus_under_fullscreen
    // check that it doesn't.

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

    for (auto const& win : {"master", "slave1", "slave2"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:master' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: master");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    Tests::spawnKitty("new_master");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: new_master");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("ignored");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: new_master");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    Tests::spawnKitty("vaxwashere");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: vaxwashere");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }
}

TEST_CASE(rollFocus) {
    // test rollnext/rollprev dispatchers

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

    // set up windows
    std::vector<std::string> windows = {"slave1", "slave2", "slave3", "master"};

    // helper lambda thing
    auto roll = [&](const std::string& dir) {
        auto pivot = (dir == "rollnext") ? windows.begin() + 1 : windows.end() - 1;

        // rotate the windows vector along with the actual windows
        // the rolling behavior of the window focus should follow the
        // rotating behavior of std::ranges::rotate
        OK(getFromSocket("/dispatch hl.dsp.layout('" + dir + "')"));
        std::ranges::rotate(windows.begin(), pivot, windows.end());
        ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: " + windows.back());
    };

    for (auto const& win : windows) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    // focus master
    OK(getFromSocket("/dispatch hl.dsp.layout('focusmaster master')"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    // put the windows in the washing machine
    NLog::log("{}Testing rollnext", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        roll("rollnext");
    }

    NLog::log("{}Testing rollprev", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        roll("rollprev");
    }

    NLog::log("{}Testing rollnext with rollprev", Colors::YELLOW);
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 5; ++j) {
            roll("rollnext");
        }
        roll("rollprev");
    }

    NLog::log("{}Testing rollnext/rollprev alternation", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        if (i % 2 == 0) {
            roll("rollnext");
        } else {
            roll("rollprev");
        }
    }

    NLog::log("{}Testing rollnext/rollprev burst calls", Colors::YELLOW);
    for (int i = 0; i < 20; ++i) {
        if (i / 5 % 2 == 0) {
            roll("rollnext");
        } else {
            roll("rollprev");
        }
    }
}

TEST_CASE(focusMasterClose) {
    //Test behaviour of master:focus_master_on_close
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' }, master = { focus_master_on_close = true } })"));

    std::vector<pid_t> pids;
    for (auto const& win : {"slave1", "slave2", "slave3", "master"}) {
        auto p = Tests::spawnKitty(win);
        if (!p)
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        pids.push_back(p->pid());
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:slave1' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.close({ window = 'class:slave1' })"));
    while (Tests::processAlive(pids[0]))
        std::this_thread::sleep_for(std::chrono::milliseconds(25l));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:slave2' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.close({ window = 'class:slave2' })"));
    while (Tests::processAlive(pids[1]))
        std::this_thread::sleep_for(std::chrono::milliseconds(25l));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: master");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:slave3' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.close({ window = 'class:slave3' })"));
    while (Tests::processAlive(pids[2]))
        std::this_thread::sleep_for(std::chrono::milliseconds(25l));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: master");
}
