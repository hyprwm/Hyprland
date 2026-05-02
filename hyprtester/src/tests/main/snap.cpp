#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/os/Process.hpp>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

using Hyprutils::Math::Vector2D;

static bool spawnFloatingKitty() {
    if (!Tests::spawnKitty()) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }
    bool ok = true;
    ok &= getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })") == "ok";
    ok &= getFromSocket("/dispatch hl.dsp.window.resize({ x = 100, y = 100 })") == "ok";
    return ok;
}

SUBTEST(expectSnapMove, double fromX, double fromY, double toX, double toY) {
    const Vector2D FROM = {fromX, fromY};
    const Vector2D TO   = {toX, toY};
    if (FROM == TO)
        NLog::log("{}Expecting no snap when window is moved to ({},{})", Colors::YELLOW, FROM.x, FROM.y);
    else
        NLog::log("{}Expecting snap to ({},{}) when window is moved to ({},{})", Colors::YELLOW, TO.x, TO.y, FROM.x, FROM.y);

    OK(getFromSocket(std::format("/dispatch hl.dsp.window.move({{ x = {}, y = {} }})", FROM.x, FROM.y)));
    OK(getFromSocket("/eval hl.plugin.test.snapmove()"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("at: {},{}", TO.x, TO.y));
}

SUBTEST(expectNoSnapMove, double x, double y) {
    CALL_SUBTEST(expectSnapMove, x, y, x, y);
}

SUBTEST(testWindowSnap, const bool RESPECTGAPS) {
    const int BORDERSIZE = 2;
    const int WINDOWSIZE = 100;

    const int OTHER     = 500;
    const int WINDOWGAP = 8;
    const int GAPSIN    = 5;
    const int GAP       = (RESPECTGAPS ? 2 * GAPSIN : 0) + (2 * BORDERSIZE);
    const int END       = GAP + WINDOWSIZE;

    int       x = WINDOWGAP + END;
    CALL_SUBTEST(expectNoSnapMove, OTHER + x, OTHER);
    CALL_SUBTEST(expectNoSnapMove, OTHER - x, OTHER);
    CALL_SUBTEST(expectNoSnapMove, OTHER, OTHER + x);
    CALL_SUBTEST(expectNoSnapMove, OTHER, OTHER - x);
    x -= 1;
    CALL_SUBTEST(expectSnapMove, OTHER + x, OTHER, OTHER + END, OTHER);
    CALL_SUBTEST(expectSnapMove, OTHER - x, OTHER, OTHER - END, OTHER);
    CALL_SUBTEST(expectSnapMove, OTHER, OTHER + x, OTHER, OTHER + END);
    CALL_SUBTEST(expectSnapMove, OTHER, OTHER - x, OTHER, OTHER - END);
}

SUBTEST(testMonitorSnap, const bool RESPECTGAPS, const bool OVERLAP) {
    const int BORDERSIZE = 2;
    const int WINDOWSIZE = 100;

    const int MONITORGAP = 10;
    const int GAPSOUT    = 20;
    const int RESP       = (RESPECTGAPS ? GAPSOUT : 0);
    const int GAP        = RESP + (OVERLAP ? 0 : BORDERSIZE);
    const int END        = GAP + WINDOWSIZE;

    int       x;
    Vector2D  predict;

    x = MONITORGAP + GAP;
    CALL_SUBTEST(expectNoSnapMove, x, x);
    x -= 1;
    CALL_SUBTEST(expectSnapMove, x, x, GAP, GAP);

    x = MONITORGAP + END;
    CALL_SUBTEST(expectNoSnapMove, 1920 - x, 1080 - x);
    x -= 1;
    CALL_SUBTEST(expectSnapMove, 1920 - x, 1080 - x, 1920 - END, 1080 - END);

    // test reserved area
    const int RESERVED = 200;
    const int RGAP     = RESERVED + RESP + BORDERSIZE;
    const int REND     = RGAP + WINDOWSIZE;

    x = MONITORGAP + RGAP;
    CALL_SUBTEST(expectNoSnapMove, x, x);
    x -= 1;
    CALL_SUBTEST(expectSnapMove, x, x, RGAP, RGAP);

    x = MONITORGAP + REND;
    CALL_SUBTEST(expectNoSnapMove, 1920 - x, 1080 - x);
    x -= 1;
    CALL_SUBTEST(expectSnapMove, 1920 - x, 1080 - x, 1920 - REND, 1080 - REND);
}

// TODO: decompose this into multiple test cases
TEST_CASE(snap) {
    NLog::log("{}Testing snap", Colors::GREEN);

    // move to monitor HEADLESS-2
    NLog::log("{}Moving to monitor HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    NLog::log("{}Adding reserved monitor area to HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', reserved = { top = 200, right = 200, bottom = 200, left = 200 } })"));

    // test on workspace "snap"
    NLog::log("{}Dispatching workspace `snap`", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:snap' })"));

    // spawn a kitty terminal and move to (500,500)
    NLog::log("{}Spawning kittyProcA", Colors::YELLOW);
    if (!spawnFloatingKitty())
        FAIL_TEST("Could not spawn kitty");

    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 1);

    NLog::log("{}Move the kitty window to (500,500)", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 500, y = 500 })"));

    // spawn a second kitty terminal
    NLog::log("{}Spawning kittyProcB", Colors::YELLOW);
    if (!spawnFloatingKitty())
        FAIL_TEST("Could not spawn kitty");

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 2);

    NLog::log("");
    CALL_SUBTEST(testWindowSnap, false);
    CALL_SUBTEST(testMonitorSnap, false, false);

    NLog::log("\n{}Turning on respect_gaps", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ general = { snap = { respect_gaps = true } } })"));
    CALL_SUBTEST(testWindowSnap, true);
    CALL_SUBTEST(testMonitorSnap, true, false);

    NLog::log("\n{}Turning on border_overlap", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ general = { snap = { respect_gaps = false } } })"));
    OK(getFromSocket("/eval hl.config({ general = { snap = { border_overlap = true } } })"));
    CALL_SUBTEST(testMonitorSnap, false, true);

    NLog::log("\n{}Turning on both border_overlap and respect_gaps", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ general = { snap = { respect_gaps = true } } })"));
    CALL_SUBTEST(testMonitorSnap, true, true);
}
