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

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'up' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testFocusWrapping() {
    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // set wrap_focus to true
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_focus = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: a");
    }

    // set wrap_focus to false
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_focus = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: a");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: d");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testSwapcolWrapping() {
    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // set wrap_swapcol to true
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_swapcol = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol l')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // set wrap_swapcol to false
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_swapcol = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol l')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static bool testWindowRule() {
    NLog::log("{}Testing Scrolling Width", Colors::GREEN);

    // inject a new rule.
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', match = { class = 'kitty_scroll' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', scrolling_width = 0.1 })"));

    if (!Tests::spawnKitty("kitty_scroll")) {
        NLog::log("{}Failed to spawn kitty with win class `kitty_scroll`", Colors::RED);
        return false;
    }

    if (!Tests::spawnKitty("kitty_scroll")) {
        NLog::log("{}Failed to spawn kitty with win class `kitty_scroll`", Colors::RED);
        return false;
    }

    EXPECT(Tests::windowCount(), 2);

    // not the greatest test, but as long as res and gaps don't change, we good.
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "size: 174,1036");

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);
    return true;
}

static bool test() {
    NLog::log("{}Testing Scroll layout", Colors::GREEN);

    // setup
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:scroll' })"));
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // test
    NLog::log("{}Testing focus cycling", Colors::GREEN);
    testFocusCycling();

    // test
    NLog::log("{}Testing focus wrap", Colors::GREEN);
    testFocusWrapping();

    // test
    NLog::log("{}Testing swapcol wrap", Colors::GREEN);
    testSwapcolWrapping();

    testWindowRule();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
