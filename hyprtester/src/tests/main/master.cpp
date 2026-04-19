#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
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
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "size: 1876");
    }

    // cycle = top, right, bottom, center, left

    // right
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 873,22");
        ASSERT_CONTAINS(str, "size: 1025,1036");
    }

    // bottom
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 22,495");
        ASSERT_CONTAINS(str, "size: 1876");
    }

    // center
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 450,22");
        ASSERT_CONTAINS(str, "size: 1020,1036");
    }

    // left
    OK(getFromSocket("/dispatch hl.dsp.layout('orientationnext')"));
    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "size: 1025,1036");
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
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "size: 1876,1036");
        ASSERT_CONTAINS(str, "class: master");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    Tests::spawnKitty("new_master");

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "size: 1876,1036");
        ASSERT_CONTAINS(str, "class: new_master");
        ASSERT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("ignored");

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "size: 1876,1036");
        ASSERT_CONTAINS(str, "class: new_master");
        ASSERT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    Tests::spawnKitty("vaxwashere");

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: vaxwashere");
        ASSERT_CONTAINS(str, "fullscreen: 0");
    }
}
