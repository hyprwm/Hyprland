#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/os/Process.hpp>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

using Hyprutils::Math::Vector2D;

static int  ret = 0;

static bool spawnFloatingKitty() {
    if (!Tests::spawnKitty()) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }
    OK(getFromSocket("/dispatch setfloating active"));
    OK(getFromSocket("/dispatch resizeactive exact 100 100"));
    return true;
}

static void expectSocket(const std::string& CMD) {
    if (const auto RESULT = getFromSocket(CMD); RESULT != "ok") {
        NLog::log("{}Failed: {}getFromSocket({}), expected ok, got {}. Source: {}@{}.", Colors::RED, Colors::RESET, CMD, RESULT, __FILE__, __LINE__);
        ret = 1;
        TESTS_FAILED++;
    } else {
        NLog::log("{}Passed: {}getFromSocket({}). Got ok", Colors::GREEN, Colors::RESET, CMD);
        TESTS_PASSED++;
    }
}

static void expectSnapMove(const Vector2D FROM, const Vector2D* TO) {
    const Vector2D& A = FROM;
    const Vector2D& B = TO ? *TO : FROM;
    if (TO)
        NLog::log("{}Expecting snap to ({},{}) when window is moved to ({},{})", Colors::YELLOW, B.x, B.y, A.x, A.y);
    else
        NLog::log("{}Expecting no snap when window is moved to ({},{})", Colors::YELLOW, A.x, A.y);

    expectSocket(std::format("/dispatch moveactive exact {} {}", A.x, A.y));
    expectSocket("/dispatch plugin:test:snapmove");
    EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("at: {},{}", B.x, B.y));
}

static void testWindowSnap(const bool RESPECTGAPS) {
    const int BORDERSIZE = 2;
    const int WINDOWSIZE = 100;

    const int OTHER     = 500;
    const int WINDOWGAP = 8;
    const int GAPSIN    = 5;
    const int GAP       = (RESPECTGAPS ? 2 * GAPSIN : 0) + (2 * BORDERSIZE);
    const int END       = GAP + WINDOWSIZE;

    int       x;
    Vector2D  predict;

    x = WINDOWGAP + END;
    expectSnapMove({OTHER + x, OTHER}, nullptr);
    expectSnapMove({OTHER - x, OTHER}, nullptr);
    expectSnapMove({OTHER, OTHER + x}, nullptr);
    expectSnapMove({OTHER, OTHER - x}, nullptr);
    x -= 1;
    expectSnapMove({OTHER + x, OTHER}, &(predict = {OTHER + END, OTHER}));
    expectSnapMove({OTHER - x, OTHER}, &(predict = {OTHER - END, OTHER}));
    expectSnapMove({OTHER, OTHER + x}, &(predict = {OTHER, OTHER + END}));
    expectSnapMove({OTHER, OTHER - x}, &(predict = {OTHER, OTHER - END}));
}

static void testMonitorSnap(const bool RESPECTGAPS, const bool OVERLAP) {
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
    expectSnapMove({x, x}, nullptr);
    x -= 1;
    expectSnapMove({x, x}, &(predict = {GAP, GAP}));

    x = MONITORGAP + END;
    expectSnapMove({1920 - x, 1080 - x}, nullptr);
    x -= 1;
    expectSnapMove({1920 - x, 1080 - x}, &(predict = {1920 - END, 1080 - END}));

    // test reserved area
    const int RESERVED = 200;
    const int RGAP     = RESERVED + RESP + BORDERSIZE;
    const int REND     = RGAP + WINDOWSIZE;

    x = MONITORGAP + RGAP;
    expectSnapMove({x, x}, nullptr);
    x -= 1;
    expectSnapMove({x, x}, &(predict = {RGAP, RGAP}));

    x = MONITORGAP + REND;
    expectSnapMove({1920 - x, 1080 - x}, nullptr);
    x -= 1;
    expectSnapMove({1920 - x, 1080 - x}, &(predict = {1920 - REND, 1080 - REND}));
}

static bool test() {
    NLog::log("{}Testing snap", Colors::GREEN);

    // move to monitor HEADLESS-2
    NLog::log("{}Moving to monitor HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    NLog::log("{}Adding reserved monitor area to HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/keyword monitor HEADLESS-2,addreserved,200,200,200,200"));

    // test on workspace "snap"
    NLog::log("{}Dispatching workspace `snap`", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace name:snap"));

    // spawn a kitty terminal and move to (500,500)
    NLog::log("{}Spawning kittyProcA", Colors::YELLOW);
    if (!spawnFloatingKitty())
        return false;

    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 1);

    NLog::log("{}Move the kitty window to (500,500)", Colors::YELLOW);
    OK(getFromSocket("/dispatch moveactive exact 500 500"));

    // spawn a second kitty terminal
    NLog::log("{}Spawning kittyProcB", Colors::YELLOW);
    if (!spawnFloatingKitty())
        return false;

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);

    NLog::log("");
    testWindowSnap(false);
    testMonitorSnap(false, false);

    NLog::log("\n{}Turning on respect_gaps", Colors::YELLOW);
    OK(getFromSocket("/keyword general:snap:respect_gaps true"));
    testWindowSnap(true);
    testMonitorSnap(true, false);

    NLog::log("\n{}Turning on border_overlap", Colors::YELLOW);
    OK(getFromSocket("/keyword general:snap:respect_gaps false"));
    OK(getFromSocket("/keyword general:snap:border_overlap true"));
    testMonitorSnap(false, true);

    NLog::log("\n{}Turning on both border_overlap and respect_gaps", Colors::YELLOW);
    OK(getFromSocket("/keyword general:snap:respect_gaps true"));
    testMonitorSnap(true, true);

    // kill all
    NLog::log("\n{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    NLog::log("{}Reloading the config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test)
