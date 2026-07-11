#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <string>
#include <vector>
#include "tests.hpp"

/*
    Fullscreen Tests

    Tests with `Shared test among all default handled FS` comment are duplicated among all layouts to test each layout individually

*/

TEST_CASE(monocleFullscreenMaximiseDispatchers) {

    // Shared test among all default handled FS

    OK(getFromSocket("/eval hl.config({ general = { layout = 'monocle' } })"));

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

TEST_CASE(monocleTestFsFocusUnderFSWindow) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'monocle' } })"));

    for (auto const& win : {"one", "two", "three"}) {
        if (!Tests::spawnKitty(win))
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
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

TEST_CASE(monocleNewWindowTakesOverFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'monocle' } })"));

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

TEST_CASE(monocleExitWindowRetainsFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'monocle' } })"));

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

TEST_CASE(monocleFullscreenPinnedWindows) {

    // Shared test among all default handled FS

    /*
    
    allow_pin_fullscreen -> Allow internal FSing a pinned window at all?

    if true: FSed pinned window doesn't behave as pinned while it is FS but continues to behave as pinned when it's unFS 
    if false: doesn't allow FSing it at all (client can be set if de-syncing internal and client)

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'monocle' } })"));

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

TEST_CASE(monocleFullscreenNonInterference) {

    // Shared test among all default handled FS

    /*
    
    When a tiled/floating window is default handled FSed, it must not cause the windows under it to have moved/resized after it is unFSed

    also tests if floating pos/size is properly restored after fS-unfs

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'monocle' } })"));

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
