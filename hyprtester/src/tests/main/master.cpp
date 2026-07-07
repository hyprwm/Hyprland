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

/*
    Fullscreen Tests

    Tests with `Shared test among all default handled FS` comment are duplicated among all layouts to test each layout individually

*/

TEST_CASE(masterTestFsFocusUnderFSWindow) {

    // Shared test among all default handled FS

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

TEST_CASE(masterFullscreenMaximiseDispatchers) {

    // Shared test among all default handled FS

    OK(getFromSocket("/eval hl.config({ general = { layout = 'master' } })"));

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

TEST_CASE(masterNewWindowTakesOverFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

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

TEST_CASE(masterExitWindowRetainsFullscreen) {

    // Shared test among all default handled FS

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

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

TEST_CASE(masterFullscreenPinnedWindows) {

    // Shared test among all default handled FS

    /*
    
    allow_pin_fullscreen -> Allow internal FSing a pinned window at all?

    if true: FSed pinned window doesn't behave as pinned while it is FS but continues to behave as pinned when it's unFS 
    if false: doesn't allow FSing it at all (client can be set if de-syncing internal and client)

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

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

TEST_CASE(masterFullscreenNonInterference) {

    // Shared test among all default handled FS

    /*
    
    When a tiled/floating window is default handled FSed, it must not cause the windows under it to have moved/resized after it is unFSed

    also tests if floating pos/size is properly restored after fS-unfs

    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

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
