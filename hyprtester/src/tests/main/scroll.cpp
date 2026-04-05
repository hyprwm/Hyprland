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
    OK(getFromSocket("/keyword scrolling:wrap_focus true"));

    OK(getFromSocket("/dispatch focuswindow class:a"));

    OK(getFromSocket("/dispatch layoutmsg focus l"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch layoutmsg focus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: a");
    }

    // set wrap_focus to false
    OK(getFromSocket("/keyword scrolling:wrap_focus false"));

    OK(getFromSocket("/dispatch focuswindow class:a"));

    OK(getFromSocket("/dispatch layoutmsg focus l"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: a");
    }

    OK(getFromSocket("/dispatch focuswindow class:d"));

    OK(getFromSocket("/dispatch layoutmsg focus r"));

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
    OK(getFromSocket("/keyword scrolling:wrap_swapcol true"));

    OK(getFromSocket("/dispatch focuswindow class:a"));

    OK(getFromSocket("/dispatch layoutmsg swapcol l"));
    OK(getFromSocket("/dispatch layoutmsg focus l"));

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

    OK(getFromSocket("/dispatch focuswindow class:d"));
    OK(getFromSocket("/dispatch layoutmsg swapcol r"));
    OK(getFromSocket("/dispatch layoutmsg focus r"));

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
    OK(getFromSocket("/keyword scrolling:wrap_swapcol false"));

    OK(getFromSocket("/dispatch focuswindow class:a"));

    OK(getFromSocket("/dispatch layoutmsg swapcol l"));
    OK(getFromSocket("/dispatch layoutmsg focus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch focuswindow class:d"));

    OK(getFromSocket("/dispatch layoutmsg swapcol r"));
    OK(getFromSocket("/dispatch layoutmsg focus l"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testScrollInhibitor() {

    // setup so borders don't contribute to window positioning
    OK(getFromSocket("/keyword general:border_size 0"));
    OK(getFromSocket("/keyword general:gaps_in 0"));
    OK(getFromSocket("/keyword general:gaps_out 0"));

    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
        // set each window's column size to take up the entire screen
        OK(getFromSocket("/dispatch layoutmsg colresize 1"));
    }

    // must be focused on the leftmost window
    OK(getFromSocket("/dispatch focuswindow class:a"));

    std::string posA   = Tests::getWindowAttribute(getFromSocket("/activewindow"), "at:");
    std::string posA_x = posA.substr(4, posA.find(',') - 4);

    std::string sizeA   = Tests::getWindowAttribute(getFromSocket("/activewindow"), "size:");
    std::string sizeA_x = sizeA.substr(6, sizeA.find(',') - 6);

    OK(getFromSocket("/dispatch layoutmsg inhibit_scroll 1"));

    // if it were not inhibited, it'd move the view to show the rightmost window
    OK(getFromSocket("/dispatch layoutmsg focus r"));

    // if the left window's "at: ..." attribute's x coordinate is = right window's at_x + size_x, the inhibition worked.
    {
        std::string posB   = Tests::getWindowAttribute(getFromSocket("/activewindow"), "at:");
        std::string posB_x = posB.substr(4, posB.find(',') - 4);

        std::string expectedRightWindowPos = std::to_string(std::stoi(posA_x) + std::stoi(sizeA_x));

        // This way prevents the check from breaking if resolution for tests were to be changed
        // NOLINTBEGIN(performance-unnecessary-copy-initialization)
        EXPECT(posB_x, expectedRightWindowPos);
        // NOLINTEND(performance-unnecessary-copy-initialization)
    }

    // clean up

    // to revert the changes made to border_size, gaps_in, gaps_out
    NLog::log("{}Restoring config state", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    // kill all windows
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);
}

static bool testWindowRule() {
    NLog::log("{}Testing Scrolling Width", Colors::GREEN);

    // inject a new rule.
    OK(getFromSocket("/keyword windowrule[scrolling-width]:match:class kitty_scroll"));
    OK(getFromSocket("/keyword windowrule[scrolling-width]:scrolling_width 0.1"));

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
    OK(getFromSocket("/dispatch workspace name:scroll"));
    OK(getFromSocket("/keyword general:layout scrolling"));

    // test
    NLog::log("{}Testing focus cycling", Colors::GREEN);
    testFocusCycling();

    // test
    NLog::log("{}Testing focus wrap", Colors::GREEN);
    testFocusWrapping();

    // test
    NLog::log("{}Testing swapcol wrap", Colors::GREEN);
    testSwapcolWrapping();

    // test
    NLog::log("{}Testing scroll inhibitor", Colors::GREEN);
    testScrollInhibitor();

    testWindowRule();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
