#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <utility>
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

TEST_CASE(centerMasterColumnResize) {
    // Center master, odd slave count. The fallback-side column holds the extra slave;
    // resizeTarget() used to assume it went right, so with the default 'left' fallback the two left
    // windows of a 1-master/3-slave layout silently refused to resize vertically.

    // focus a window by class and read its {left edge x, height} from /activewindow
    auto geomOf = [&](const std::string& cls) -> std::pair<double, double> {
        getFromSocket("/dispatch hl.dsp.focus({ window = 'class:" + cls + "' })");
        const auto STR = getFromSocket("/activewindow");
        const auto AT  = Tests::getAttribute(STR, "at");   // "x,y"
        const auto SZ  = Tests::getAttribute(STR, "size"); // "w,h"
        return {std::stod(AT.substr(0, AT.find(','))), std::stod(SZ.substr(SZ.find(',') + 1))};
    };
    auto leftOf   = [&](const std::string& cls) { return geomOf(cls).first; };
    auto heightOf = [&](const std::string& cls) { return geomOf(cls).second; };

    // resizeactive-style relative resize of a specific window along y
    auto resizeY = [&](const std::string& cls, int dy) {
        return getFromSocket("/dispatch hl.dsp.window.resize({ x = 0, y = " + std::to_string(dy) + ", relative = true, window = 'class:" + cls + "' })");
    };

    // `top` and `bottom` share one column and must resize vertically (one grows, the other shrinks,
    // total column height preserved); `single` lives in the other column and must stay untouched.
    auto expectColumnResizes = [&](const std::string& top, const std::string& bottom, const std::string& single) {
        const double TX = leftOf(top), BX = leftOf(bottom), SX = leftOf(single);
        ASSERT_MAX_DELTA(BX, TX, 1);           // top & bottom share a column (same left edge)
        ASSERT(std::abs(SX - TX) > 100, true); // single sits in the other column

        const double T0 = heightOf(top), B0 = heightOf(bottom), S0 = heightOf(single);

        OK(resizeY(top, 80)); // grow the upper window of the pair

        const double T1 = heightOf(top), B1 = heightOf(bottom), S1 = heightOf(single);
        EXPECT(T1 > T0, true);                 // upper window grew
        EXPECT(B1 < B0, true);                 // lower window shrank to compensate
        EXPECT_MAX_DELTA(T1 + B1, T0 + B0, 4); // column height preserved
        EXPECT_MAX_DELTA(S1, S0, 2);           // the other column is untouched

        OK(resizeY(top, -80)); // restore the split for later phases
    };

    // 1 master + 3 slaves => slaves slave1, slave2, slave3 in stack order (new_status = master)
    OK(getFromSocket(
        "r/eval hl.config({ general = { layout = 'master' }, master = { orientation = 'center', center_master_fallback = 'left', slave_count_for_center_master = 2 } })"));
    for (auto const& win : {"slave1", "slave2", "slave3", "master"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }

    // default `left` fallback: slave1 (top) + slave3 (bottom) on the left, slave2 alone on the right
    NLog::log("{}center master, left fallback, 3 slaves: left column must resize (smart_resizing on)", Colors::YELLOW);
    expectColumnResizes("slave1", "slave3", "slave2");

    NLog::log("{}center master, left fallback, 3 slaves: left column must resize (smart_resizing off)", Colors::YELLOW);
    OK(getFromSocket("r/eval hl.config({ master = { smart_resizing = false } })"));
    expectColumnResizes("slave1", "slave3", "slave2");
    OK(getFromSocket("r/eval hl.config({ master = { smart_resizing = true } })"));

    // symmetry: `right` fallback puts the pair (slave1, slave3) on the right, slave2 alone on the left
    NLog::log("{}center master, right fallback, 3 slaves: right column must resize", Colors::YELLOW);
    OK(getFromSocket("r/eval hl.config({ master = { center_master_fallback = 'right' } })"));
    expectColumnResizes("slave1", "slave3", "slave2");

    // even count, no regression: a 4th slave makes 2 left / 2 right; the left pair still resizes.
    // new_status = master => `extra` becomes the master and `master` drops to the 4th slave.
    NLog::log("{}center master, left fallback, 4 slaves: columns still resize (no regression)", Colors::YELLOW);
    OK(getFromSocket("r/eval hl.config({ master = { center_master_fallback = 'left' } })"));
    if (!Tests::spawnKitty("extra"))
        FAIL_TEST("Could not spawn kitty with win class `{}`", "extra");
    expectColumnResizes("slave1", "slave3", "slave2");

    // even count, no regression: 2 slaves => 1 per column, so a vertical resize is a no-op
    NLog::log("{}center master, 2 slaves: single-window columns don't resize (no regression)", Colors::YELLOW);
    if (!Tests::killAllWindows())
        FAIL_TEST("Could not kill all windows before the {}-slave phase", 2);
    for (auto const& win : {"slave1", "slave2", "master"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }
    const double H0 = heightOf("slave1");
    OK(resizeY("slave1", 80));
    EXPECT_MAX_DELTA(heightOf("slave1"), H0, 2);
}

// In a centered master layout with three windows w1/w2/w3, return their classes ordered
// visually left-to-right: { left slave, master (center), right slave }.
static std::array<std::string, 3> detectCenterArrangement() {
    std::vector<std::pair<int, std::string>> wins;
    for (auto const& cls : {"w1", "w2", "w3"}) {
        getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:{}' }})", cls));
        const auto at = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        wins.emplace_back(std::stoi(at.substr(0, at.find(','))), cls);
    }
    std::ranges::sort(wins, {}, &std::pair<int, std::string>::first);
    return {wins[0].second, wins[1].second, wins[2].second};
}

// Drag the window currently playing role `pick` (one of "L"/"M"/"R", i.e. the left
// slave, the master, or the right slave) and drop it on the left or right side of the
// screen. Assert the windows end up in the expected left/center/right roles afterwards.
SUBTEST(expectCenterDrop, const std::string& pick, bool dropRight, const std::string& expLeft, const std::string& expCenter, const std::string& expRight) {
    // start from a fresh set of three windows
    if (!Tests::killAllWindows())
        FAIL_TEST("Could not clear windows{}", "");

    for (auto const& win : {"w1", "w2", "w3"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
    }
    Tests::waitUntilWindowsN(3);

    const auto                         INITIAL = detectCenterArrangement();
    std::map<std::string, std::string> role    = {
        {"L", INITIAL[0]},
        {"M", INITIAL[1]},
        {"R", INITIAL[2]},
    };

    const double DROPX = dropRight ? 1800.0 : 100.0;
    NLog::log("{}Picking up {} ({}) and dropping it on the {} side", Colors::YELLOW, pick, role[pick], dropRight ? "right" : "left");

    OK(getFromSocket(std::format("/eval hl.plugin.test.drag_window('{}', {}, {})", role[pick], DROPX, 540)));

    const auto FINAL = detectCenterArrangement();
    EXPECT(FINAL[0], role[expLeft]);
    EXPECT(FINAL[1], role[expCenter]);
    EXPECT(FINAL[2], role[expRight]);
}

TEST_CASE(masterCenterDropAtCursor) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));
    OK(getFromSocket("/eval hl.config({ master = { orientation = 'center', new_status = 'slave', drop_at_cursor = true, slave_count_for_center_master = 0, center_master_fallback "
                     "= 'left' } })"));

    NLog::log("{}Testing center master drop_at_cursor rearrangement", Colors::GREEN);

    CALL_SUBTEST(expectCenterDrop, "L", false, "L", "M", "R");
    CALL_SUBTEST(expectCenterDrop, "L", true, "R", "M", "L");
    CALL_SUBTEST(expectCenterDrop, "M", false, "M", "L", "R");
    CALL_SUBTEST(expectCenterDrop, "M", true, "R", "L", "M");
    CALL_SUBTEST(expectCenterDrop, "R", false, "R", "M", "L");
    CALL_SUBTEST(expectCenterDrop, "R", true, "L", "M", "R");
}
