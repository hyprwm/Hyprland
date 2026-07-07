#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

TEST_CASE(dwindleFloatClamp) {
    for (auto const& win : {"a", "b", "c"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', reserved = { top = 0, right = 20, bottom = 0, left = 20 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 1200, y = 900, window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'unset', window = 'class:c' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:c' })"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at:");
        EXPECT_CONTAINS(str, "size: 1200,900");
    }

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 0 } })"));
}

TEST_CASE(dwindleIssue13349) {

    // Test if dwindle properly uses a focal point to place a new window.
    // exposed by #13349 as a regression from #12890

    for (auto const& win : {"a", "b", "c"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:c' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 967,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 967,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }
}

TEST_CASE(dwindleSplit) {
    // Test various split methods

    Tests::spawnKitty("a");

    // these must not crash
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('swapsplit')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio 1 exact')"), "ok");

    Tests::spawnKitty("b");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('splitratio -0.2')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 743,1036");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('splitratio 1.6 exact')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1495,1036");
    }

    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio fhne exact')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio exact')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio -....9')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio ..9')"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.layout('splitratio')"), "ok");

    OK(getFromSocket("/dispatch hl.dsp.layout('togglesplit')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,823");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('swapsplit')"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,859");
        EXPECT_CONTAINS(str, "size: 1876,199");
    }
}

TEST_CASE(dwindleRotateSplit) {
    OK(getFromSocket("r/eval hl.config({ general = { gaps_in = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ general = { gaps_out = 0 } })"));
    OK(getFromSocket("r/eval hl.config({ general = { border_size = 0 } })"));

    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    // test 4 repeated rotations by 90 degrees
    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,540");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    // test different angles
    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit 180')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit 270')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,540");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit 360')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    // test negative angles
    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit -90')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('rotatesplit -180')"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }
}

TEST_CASE(dwindleForceSplitOnMoveToWorkspace) {
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    ASSERT(!!Tests::spawnKitty("kitty"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    ASSERT(!!Tests::spawnKitty("kitty"), true);
    std::string posBefore = "at: " + Tests::getAttribute(getFromSocket("/activewindow"), "at");

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move_to_corner({ corner = 3 })")); // top left
    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = '2' })"));

    // Should be moved to the right, so the position should change
    std::string activeWindow = getFromSocket("/activewindow");
    EXPECT(activeWindow.contains(posBefore), false);
}

TEST_CASE(dwindleMoveAcrossToggledSplit) {
    // If we have a split whose orientation has been manually toggled (e.g.
    // vertically stacked, when the split's aspect ratio is such that it would
    // prefer to be horizontally stacked by default), moving a window across
    // the split should NOT revert back to the preferred split orientation

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }
    OK(getFromSocket("/dispatch hl.dsp.layout('togglesplit')"));
    // Window A, now on top, is to be moved

    auto origWinB   = getFromSocket("/activewindow");
    auto expectPos  = "at: " + Tests::getAttribute(origWinB, "at");
    auto expectSize = "size: " + Tests::getAttribute(origWinB, "size");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'down' })"));

    // Window A should be moved down, so position and size should swap with window B
    auto newWinA = getFromSocket("/activewindow");
    EXPECT_CONTAINS(newWinA, std::move(expectPos));
    EXPECT_CONTAINS(newWinA, std::move(expectSize));
}

TEST_CASE(dwindleMoveSmallWindowAcrossSplit) {
    // Small windows (<50% of their parent split's area) should be possible to
    // move across a split. Focal point weirdness has broken this in the past.

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 1 } })"));
    OK(getFromSocket("/eval hl.config({ dwindle = { default_split_ratio = 1.2 } })"));
    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }
    // Window B, on the left, is the smaller one

    auto posBefore = "at: " + Tests::getAttribute(getFromSocket("/activewindow"), "at");

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right' })"));

    // Window B should be moved right, so position should change
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), posBefore);
}

/*
    Fullscreen Tests

    Tests with `Shared test among all default handled FS` comment are duplicated among all layouts to test each layout individually

*/

TEST_CASE(dwindleFullscreenMaximiseDispatchers) {

    // Shared test among all default handled FS

    OK(getFromSocket("/eval hl.config({ general = { layout = 'dwindle' } })"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }
}

TEST_CASE(dwindleTestFsFocusUnderFSWindow) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    for (auto const& win : {"one", "two", "three"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:one' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: one");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    Tests::spawnKitty("four");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: four");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("ignored");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
        EXPECT_CONTAINS(str, "class: four");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    Tests::spawnKitty("erstarrwashere");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: erstarrwashere");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }
}

TEST_CASE(dwindleNewWindowTakesOverFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("kitty_A");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    Tests::spawnKitty("kitty_B");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));

    {
        // should be ignored as per focus_under_fullscreen 0
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    Tests::spawnKitty("kitty_C");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_C");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    Tests::spawnKitty("kitty_D");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "kitty_D");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::killAllWindows();
}

TEST_CASE(dwindleExitWindowRetainsFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = false } })"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    Tests::spawnKitty("kitty_B");
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    Tests::killAllWindows();
}

TEST_CASE(dwindleFullscreenPinnedWindows) {

    // Shared test among all default handled FS

    /*
    
    allow_pin_fullscreen -> Allow internal FSing a pinned window at all?

    if true: FSed pinned window doesn't behave as pinned while it is FS but continues to behave as pinned when it's unFS 
    if false: doesn't allow FSing it at all (client can be set if de-syncing internal and client)

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    Tests::spawnKitty("cake");

    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:cake'})"));

    // resize to expected floating value: 200 x 200
    OK(getFromSocket("/dispatch hl.dsp.window.resize({x = 200, y = 200, relative = false, window = 'class:cake'})"));

    // Workspace we are testing on: 1
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    // Pin the window
    OK(getFromSocket("r/dispatch hl.dsp.window.pin({ window = 'class:cake' })"));

    // set to false, try to FS; expect the cake to be a lie
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = false } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:cake'})"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
    }

    // Move to another workspace, expect it to follow
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 2");
    }

    // Move back to primary testing workspace, assumed it'll follow since the last test passed
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    // While syncing FS state, is not supposed to set either mode. If internal and client are decoupled, client is expected to go through
    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set', window = 'activewindow' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 1");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 1, client = 1, action = 'set', window = 'activewindow' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 1");
    }

    // re-set its FS values for the next test
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', window = 'activewindow' })"));

    // set to true, try to FS; expect the cake to be real
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = true } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:cake'})"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 1");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "at: 0,0");
        ASSERT_CONTAINS(str, "size: 1920,1080");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 1");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "at: 2,2");
        EXPECT_CONTAINS(str, "size: 1916,1076");
    }

    // unFs it, move to another workspace - expect it to follow
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', window = 'activewindow' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        // After the FSed pinned window is unFSed, expect its pinned value to come back
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 2");
    }

    // set the variable to false, unpin it and expect it to be FS-able
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = false } })"));
    OK(getFromSocket("r/dispatch hl.dsp.window.pin({ window = 'class:cake' })"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "at: 0,0");
        ASSERT_CONTAINS(str, "size: 1920,1080");
    }

    // Try with maximised
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "pinFullscreened: 0");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "at: 2,2");
        EXPECT_CONTAINS(str, "size: 1916,1076");
    }
}

TEST_CASE(dwindleFullscreenNonInterference) {

    // Shared test among all default handled FS

    /*
    
    When a tiled/floating window is default handled FSed, it must not cause the windows under it to have moved/resized after it is unFSed

    also tests if floating pos/size is properly restored after fS-unfs

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'dwindle' } })"));

    Tests::spawnKitty("red");
    Tests::spawnKitty("crimson");
    Tests::spawnKitty("blue");
    Tests::spawnKitty("cyan");
    Tests::spawnKitty("azure");
    Tests::spawnKitty("green");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));

    // Testing tiled first
    {

        // save all pos/size inc red

        // save all pos/size
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        auto redPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto redSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        auto crimsonPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto crimsonSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        auto bluePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto blueSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        auto cyanPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto cyanSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        auto azurePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto azureSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        auto greenPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto greenSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        // FS and unFS red, then check all positions are unchanged
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

        // red
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), redPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), redSize);

        // crimson
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), crimsonPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), crimsonSize);

        // blue
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), bluePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), blueSize);

        // cyan
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), cyanPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), cyanSize);

        // azure
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), azurePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), azureSize);

        // green
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), greenPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), greenSize);
    }

    // test floating

    {

        // float red
        OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:red' })"));

        // save all pos/size (red's size will be its floating size)
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        auto redPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto redSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        auto crimsonPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto crimsonSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        auto bluePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto blueSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        auto cyanPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto cyanSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        auto azurePos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto azureSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        auto greenPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
        auto greenSize = Tests::getAttribute(getFromSocket("/activewindow"), "size");

        // FS and unFS red, then check all positions are unchanged
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

        // red
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), redPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), redSize);

        // crimson
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), crimsonPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), crimsonSize);

        // blue
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), bluePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), blueSize);

        // cyan
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), cyanPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), cyanSize);

        // azure
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), azurePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), azureSize);

        // green
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"), greenPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), greenSize);
    }
}
