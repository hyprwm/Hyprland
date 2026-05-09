#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

TEST_CASE(scrollFocusCycling) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'up' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }
}

TEST_CASE(scrollFocusWrapping) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    // set wrap_focus to true
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_focus = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: a");
    }

    // set wrap_focus to false
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_focus = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: a");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: d");
    }
}

TEST_CASE(scrollSwapcolWrapping) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    // set wrap_swapcol to true
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_swapcol = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol l')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: b");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    // set wrap_swapcol to false
    OK(getFromSocket("/eval hl.config({ scrolling = { wrap_swapcol = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol l')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: c");
    }
}

TEST_CASE(scrollWindowRule) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::log("{}Testing Scrolling Width", Colors::GREEN);

    // inject a new rule.
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', match = { class = 'kitty_scroll' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', scrolling_width = 0.1 })"));

    if (!Tests::spawnKitty("kitty_scroll")) {
        FAIL_TEST("Could not spawn kitty with win class `kitty_scroll`");
    }

    if (!Tests::spawnKitty("kitty_scroll")) {
        FAIL_TEST("Could not spawn kitty with win class `kitty_scroll`");
    }

    ASSERT(Tests::windowCount(), 2);

    // not the greatest test, but as long as res and gaps don't change, we good.
    // if this test breaks, it's likely you broke equal sizing
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "size: 179,1036");
}

TEST_CASE(scrollFullscreen) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::log("{}Testing Scrolling FS", Colors::GREEN);

    ASSERT(!!Tests::spawnKitty("kitty_scroll_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_scroll_B"), true);
    ASSERT(!!Tests::spawnKitty("kitty_scroll_C"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = \"class:kitty_scroll_B\" })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1920,1080");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = \"left\" })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = \"right\" })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = \"right\" })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_C");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = \"left\" })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1920,1080");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
    }
}

TEST_CASE(testScrollingViewBehaviourDispatchFocusWindowFollowFocusFalse) {

    /*
     focuswindow DOES NOT move the scrolling view when follow_focus = false
     ---------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: focuswindow dispatch SHOULD NOT move scrolling view when follow_focus = false", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // if the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));
    if (posAx < 0) {
        NLog::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::GREEN, Colors::RESET, posAx);
    } else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::RED, Colors::RESET, posAx);
    }
}

TEST_CASE(testScrollingViewBehaviourDispatchFocusWindowFollowFocustrue) {

    /*
     focuswindow DOES move the view when follow_focus = true
     --------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: focuswindow dispatch SHOULD move scrolling view when follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // If the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport.
    // If it is not, the view moved, which is what we expect to happen.
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));
    if (posAx < 0) {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be >= 0, got {}.", Colors::RED, Colors::RESET, posAx);
    } else {
        NLog::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be >= 0, got {}.", Colors::GREEN, Colors::RESET, posAx);
    }
}

TEST_CASE(testScrollingViewBehaviourFocusFallback) {

    /*
     Focus fallback from killing a floating window onto a tiled window must NOT move scrolling view, regardless of follow_focus
     --------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: focus fallback from floating window to a tiled window should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with class `c`", Colors::RED);
    }

    // make it (window of class:c) float - the view now mush have shifted to fit window class:b
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:c'})"));

    // establish focus history
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));

    // kill the floating window
    // Expect the focus to fall back to the left tiled window
    OK(getFromSocket("/dispatch hl.dsp.window.kill({window = 'class:c'})"));
    Tests::waitUntilWindowsN(2);

    // The focus now must have fallen back to tiled window of class "a".

    // If the view did not move, we expect currently focused window's (class:a) to have "at: " x coordinat value <0 (must be left of the viewport)

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourFocusFallbackWithGroups) {

    // same idea as testScrollingViewBehaviourFocusFallback, but with window of class "a" being grouped.

    NLog::log("{}Testing scrolling view behaviour: focus fallback from floating window to a grouped tiled should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // to correctly set up windows for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    // only one tiled window will be grouped for the test
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    // make it a grouped. There need not be any other windows in the group for this test
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with class `c`", Colors::RED);
    }

    // make it float - the view now mush have shifted to fit window class:b
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:c'})"));

    // establish focus history
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})"));

    // kill the floating window
    OK(getFromSocket("/dispatch hl.dsp.window.kill({window = 'class:c'})"));
    Tests::waitUntilWindowsN(2);

    // The focus now must have fallen back to tiled window of class "a".

    // If the view did not move, we expect currently focused window's (class:a) to have "at: " x coordinat value <0 (must be left of the viewport)

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}Expected the x coordinate of window of class \"a\" to be < 0, got {}.", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourWorkspaceChange) {

    /*
     When you change to a scrolling workspace, the focused window in that workspace must not be pulled into view, regardless of follow_focus
     ---------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: changing to a scrolling workspace should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // switch to workspace 1 for this test
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '1'})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // change to workspace 2, then back to workspace 1 again
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '2'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '1'})"));

    // If the scrolling view did not move, the x value for `at:` of the currently focused window, class:a, must be <0 (must be left of the viewport)
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourSpecialWorkspaceChange) {

    /*
     When you change to a special scrolling workspace from a normal workspace, the focused window in that workspace must not be pulled into view, regardless of follow_focus
     -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: changing to a special scrolling workspace from a normal workspace should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // We'll test in this special workspace
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // change to workspace 2, then back to special "scroll_S" workspace again
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '2'})"));
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    // Reestablish focus since it is finnicky in hyprtester - Harmless and does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // If the scrolling view did not move, the x value for `at:` of the currently focused windows, class:c, must be <0 (must be left of the viewport)
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourSpecialToSpecialWorkspaceChange) {

    /*
    We also test switching between 2 special workspaces
    This follows the same idea and dependencies as the test testScrollingViewBehaviourSpecialWorkspaceChange()
    */

    NLog::log("{}Testing scrolling view behaviour: changing to a special scrolling workspace from another special workspace should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // We'll test in this special workspace
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // change to special workspace "scroll_F", then back to special "scroll_S" workspace again
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_F')"));
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    // Reestablish focus since it is finnicky in hyprtester - Harmless and does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // If the scrolling view did not move, the x value for `at:` of the currently focused windows, class:c, must be <0 (must be left of the viewport)

    const std::string currentWindowPosSPECIAL  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosSPECIALX = currentWindowPosSPECIAL.substr(0, currentWindowPosSPECIAL.find(','));
    // test pass
    if (std::stoi(currentWindowPosSPECIALX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosSPECIALX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosSPECIALX);
    }
}

TEST_CASE(testScrollingViewBehaviourCloseWindowInGroup) {

    /*
     When you change close a window inside a group (NOT destroying the group!), it should not cause scrolling view to shift to pull that group into view, regardless of follow_focus
     -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: closing a window in a group (> 1 window in group) should not move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    // We need 2 windows to be grouped, the third one not.
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // switch focus to group. This will not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // kill window class:b. we expect that this should cause not difference in the position of the group
    OK(getFromSocket("/dispatch hl.dsp.window.kill({window = 'class:b'})"));
    Tests::waitUntilWindowsN(2);

    // If the scrolling view did not move, the x value for `at:` of the currently focused windows, class:a, must be <0 (must be left of the viewport)

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveWindowIntoGroupFollowFocusFalse) {

    /*
     when a window is moved inside a group, scrolling view should not move to fit that group when follow_focus = false
     -----------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: moving a window into a group SHOULD NOT move scrolling view if follow_focus = 0", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // the focus now should still be on class:b window. If the view did not move, its x coordinate for its `at:` value should be <0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'b' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'b' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveWindowInGroupFollowFocusTrue) {

    /*
    when a window is moved inside a group, scrolling view should move to fit that group when follow_focus = true
    ------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: moving a window in a group SHOULD move scrolling view if follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // the focus now should still be on class:b window. If the scrolling view did move, its x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test fail
    if (std::stoi(currentWindowPosX) < 0) {
        FAIL_TEST("{}window of class 'b' does not have x coordinates >= 0 for its position: {}", Colors::RED, currentWindowPosX);
    }
    // test pass
    else {
        NLog ::log("{}Passed: {}window of class 'b' has x coordinates >= 0 for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourNewLayer) {

    /*
     Starting a program on a different layer shouldn't cause scrolling view to move to fit the window that was focused when this program was started, regardless of follow_focus
     ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: new program occupying another layer shouldn't move scrolling view", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // focus class:a - this does not move scrolling view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    NLog::log("{}Spawning kitty layer {}", Colors::YELLOW, "myLayer");
    if (!Tests::spawnLayerKitty("myLayer")) {
        FAIL_TEST("{}Error: {} layer did not spawn", Colors::RED, "myLayer");
    }

    // If the scrolling view did not move, class:a window's x coordinate for its `at:` value should be <0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }

    // TEST_CASE's own cleanup functions fail to kill all layers with this test. Manually do it

    // kill all layers
    NLog::log("{}Killing all layers", Colors::YELLOW);
    Tests::killAllLayers();
    ASSERT(Tests::layerCount(), 0);

    // kill all windows
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}

TEST_CASE(testScrollingViewBehaviourMoveFocusFollowFocusFalse) {

    /*
     dispatching movefocus when follow_focus = false should not cause scrolling view to move
     ---------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus does not cause scrolling view to move if follow_focus = false", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // we expect that after dispatching this, scrolling view must not have moved
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // If the scrolling view did not move, class:a window's x coordinate for its `at:` value should be < 0.
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveFocusFollowFocusTrue) {

    /*
     dispatching movefocus when follow_focus = true should cause scrolling view to move
     ----------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus does cause scrolling view to move if follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    // we expect that after dispatching this, scrolling view must have moved since follow_focus = true
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // If the scrolling view moved, class:a window's x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test fail
    if (std::stoi(currentWindowPosX) < 0) {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have x coordinates >= 0 for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
    // test pass
    else {
        NLog ::log("{}Passed: {}window of class 'a' has x coordinates >= 0 for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveFocusInGroupFollowFocusFalse) {

    /*
     When movefocus is dispatched within groups to move focus from one group member to another, scrolling view must not move if follow_focus = false
     -----------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus within groups does not cause scrolling view to move if follow_focus = false", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // necessary to make sure movefocus first cycles through tabs in a group
    OK(getFromSocket("/eval hl.config({ binds = {movefocus_cycles_groupfirst = true}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // we move from one window of a group to another (from class:b to class:a) via movefocus
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // the focus now should still be on class:a window. If the scrolling view did not move, its x coordinate for its `at:` value should be < 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test pass
    if (std::stoi(currentWindowPosX) < 0) {
        NLog ::log("{}Passed: {}window of class 'a' has negative x coordinates for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
    // test fail
    else {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have negative x coordinates for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
}

TEST_CASE(testScrollingViewBehaviourMoveFocusInGroupFollowFocusTrue) {

    /*
     When movefocus is dispatched within groups to move focus from one group member to another, scrolling view must move if follow_focus = true
     ------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::log("{}Testing scrolling view behaviour: movefocus within groups does causes scrolling view to move if follow_focus = true", Colors::GREEN);

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // necessary to make sure movefocus first cycles through tabs in a group
    OK(getFromSocket("/eval hl.config({ binds = {movefocus_cycles_groupfirst = true}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `a`", Colors::RED);
    }

    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `b`", Colors::RED);
    }

    if (!Tests::spawnKitty("c")) {
        FAIL_TEST("{}Failed to spawn kitty with win class `c`", Colors::RED);
    }

    // focus class:b. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // we move from one window of a group to another (from class:b to class:a) via movefocus
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // the focus now should still be on class:a window. If the scrolling view moved, its x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));
    // test fail
    if (std::stoi(currentWindowPosX) < 0) {
        FAIL_TEST("{}Failed: {}window of class 'a' does not have x coordinates >= 0 for its position: {}", Colors::RED, Colors::RESET, currentWindowPosX);
    }
    // test pass
    else {
        NLog ::log("{}Passed: {}window of class 'a' has x coordinates >= 0 for its position: {}", Colors ::GREEN, Colors::RESET, currentWindowPosX);
    }
}
