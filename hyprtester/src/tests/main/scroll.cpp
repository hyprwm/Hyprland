#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

TEST_CASE(scrollFocusCycling) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    for (auto const& win : {"a", "b", "c", "d"}) {
        SPAWN_KITTY(win);
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
        SPAWN_KITTY(win);
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
        SPAWN_KITTY(win);
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
    NLog::yellow("Killing all windows");
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        SPAWN_KITTY(win);
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));
    OK(getFromSocket("/dispatch hl.dsp.layout('swapcol r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: b");
    }

    // clean up
    NLog::yellow("Killing all windows");
    Tests::killAllWindows();

    for (auto const& win : {"a", "b", "c", "d"}) {
        SPAWN_KITTY(win);
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

    NLog::green("Testing Scrolling Width");

    // inject a new rule.
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', match = { class = 'kitty_scroll' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'scrolling-width', scrolling_width = 0.1 })"));

    SPAWN_KITTY("kitty_scroll");
    SPAWN_KITTY("kitty_scroll");

    ASSERT(Tests::windowCount(), 2);

    // not the greatest test, but as long as res and gaps don't change, we good.
    // if this test breaks, it's likely you broke equal sizing
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "size: 179,1036");
}


/*
    Fullscreen Tests
    
    Tests with `Shared test among all default handled FS` comment are duplicated among all layouts to test each layout individually
    
    Scroll has layout handled fullscreen so it will have scrolling-specific FS tests in addition to shared Default Handled Tests

*/

TEST_CASE(scroll_LAYOUT_HANDLED_fullscreen) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::green("Testing Scrolling FS");

    SPAWN_KITTY("kitty_scroll_A");
    SPAWN_KITTY("kitty_scroll_B");
    SPAWN_KITTY("kitty_scroll_C");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = \"class:kitty_scroll_B\" })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1920,1080");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_C");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1920,1080");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
    }
}

TEST_CASE(scroll_LAYOUT_HANDLED_maximized) {
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::green("Testing Scrolling Maximize");

    SPAWN_KITTY("kitty_scroll_A");
    SPAWN_KITTY("kitty_scroll_B");
    SPAWN_KITTY("kitty_scroll_C");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = \"class:kitty_scroll_B\" })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({mode = 'maximized'})"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1866,1036");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
        ASSERT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));
    OK(getFromSocket("/dispatch hl.dsp.layout('focus r')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: kitty_scroll_C");
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1866,1036");
        ASSERT_CONTAINS(str, "class: kitty_scroll_B");
        ASSERT_CONTAINS(str, "fullscreen: 1");
    }
}


TEST_CASE(scroll_DEFAULT_HANDLED_fullscreenMaximiseDispatchers) {

    // Shared test among all default handled FS

    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));


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


TEST_CASE(scroll_DEFAULT_HANDLED_testFsFocusUnderFSWindow) {

    // Shared test among all default handled FS


    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));


    for (auto const& win : {"one", "two", "three"}) {
        if (!Tests::spawnKitty(win)) {
            FAIL_TEST("Could not spawn kitty with win class `{}`", win);
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:one' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));

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




TEST_CASE(scroll_DEFAULT_HANDLED_newWindowTakesOverFullscreen) {

    // Shared test among all default handled FS



    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));


    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("kitty_A");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', layout_aware = false, })"));

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




TEST_CASE(scroll_DEFAULT_HANDLED_exitWindowRetainsFullscreen) {

    // Shared test among all default handled FS


    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = false } })"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', layout_aware = false, })"));

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
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', layout_aware = false, })"));
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

TEST_CASE(scroll_DEFAULT_HANDLED_FullscreenPinnedWindows) {


    // Shared test among all default handled FS

    /*
    
    allow_pin_fullscreen -> Allow FSing a pinned window at all?

    if true: FSed pinned window doesn't behave as pinned while it is FS but continues to behave as pinned when it's unFS 
    if false: doesn't allow FSing it at all

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

    


    // set to false, try to FS; expect the cake to be a lie (fail)
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = false } })"));
    
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:cake'})"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', layout_aware = false, })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
    }


    // try with maximised

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
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
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 2");
    }



    // Move back to primary testing workspace, assumed it'll follow since the last test passed
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));


    // While syncing FS state, is not supposed to set either mode, setting only client is supposed to work
    // should suppress internal setting but allow client setting to go through
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set', window = 'activewindow', layout_aware = false, })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 1");
    }


    // try with maximised too
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 1, client = 1, action = 'set', window = 'activewindow', layout_aware = false, })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 1");
    }




    // re-set its FS values for the next test
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', window = 'activewindow', layout_aware = false, })"));











    // set to true, try to FS; expect the cake to be real (succeed - happy birthday)
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = true } })"));
    
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:cake'})"));

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', layout_aware = false, })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        // When it's pinned FS, this shows as 0
        // TODO add the hyprctl output values "m_pinFullscreened", it already exists as a CWindow attribute
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "at: 0,0");
        ASSERT_CONTAINS(str, "size: 1920,1080");
    }


    // try with maximised

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        // When it's pinned FS, this shows as 0
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "at: 2,2");
        EXPECT_CONTAINS(str, "size: 1916,1076");
    }


    
    
    
    // unFs it, move to another workspace - expect it to follow
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', window = 'activewindow', layout_aware = false, })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "class: cake");
        // After the FSed pinned window is unFSed, expect its pinned value to come back
        EXPECT_CONTAINS(str, "pinned: 1");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "size: 200,200");
        EXPECT_CONTAINS(str, "workspace: 2");
    }



    // set the variable to false, unpin it and expect it to be FS-able
    OK(getFromSocket("r/eval hl.config({ binds = { allow_pin_fullscreen = false } })"));
    OK(getFromSocket("r/dispatch hl.dsp.window.pin({ window = 'class:cake' })"));
    

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', layout_aware = false, })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "at: 0,0");
        ASSERT_CONTAINS(str, "size: 1920,1080");
    }


    // try with maximised

    // Try with fullscreen
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: cake");
        EXPECT_CONTAINS(str, "pinned: 0");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
        EXPECT_CONTAINS(str, "at: 2,2");
        EXPECT_CONTAINS(str, "size: 1916,1076");
    }
}




TEST_CASE(scroll_DEFAULT_HANDLED_FullscreenNonInterference) {

    // Shared test among all default handled FS

    /*
    
    When a tiled/floating window is default handled FSed, it must not cause the windows under it to have moved/resized after it is unFSed

    also tests if floating pos/size is properly restored after fS-unfs

    */


    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));
    

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
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));

        // red
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   redPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), redSize);

        // crimson
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   crimsonPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), crimsonSize);

        // blue
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   bluePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), blueSize);

        // cyan
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   cyanPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), cyanSize);

        // azure
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   azurePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), azureSize);

        // green
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   greenPos);
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
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', layout_aware = false, })"));

        // red
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:red' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   redPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), redSize);

        // crimson
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:crimson' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   crimsonPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), crimsonSize);

        // blue
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:blue' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   bluePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), blueSize);

        // cyan
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:cyan' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   cyanPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), cyanSize);

        // azure
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:azure' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   azurePos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), azureSize);

        // green
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:green' })"));
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "at"),   greenPos);
        EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "size"), greenSize);
    }


}















































































/* Scroll viewport tests */

TEST_CASE(testScrollingViewBehaviourDispatchFocusWindowFollowFocusFalse) {

    /*
     focuswindow DOES NOT move the scrolling view when follow_focus = false
     ---------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: focuswindow dispatch SHOULD NOT move scrolling view when follow_focus = false");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // if the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));

    if (posAx >= 0)
        FAIL_TEST("Expected the x coordinate of window of class \"a\" to be < 0, got {}.", posAx);
}

TEST_CASE(testScrollingViewBehaviourDispatchFocusWindowFollowFocustrue) {

    /*
     focuswindow DOES move the view when follow_focus = true
     --------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: focuswindow dispatch SHOULD move scrolling view when follow_focus = true");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // If the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport.
    // If it is not, the view moved, which is what we expect to happen.
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));

    if (posAx < 0)
        FAIL_TEST("Expected the x coordinate of window of class \"a\" to be >= 0, got {}.", posAx);
}

TEST_CASE(testScrollingViewBehaviourFocusFallback) {

    /*
     Focus fallback from killing a floating window onto a tiled window must NOT move scrolling view, regardless of follow_focus
     --------------------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: focus fallback from floating window to a tiled window should not move scrolling view");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");
    SPAWN_KITTY("c");

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

    if (std::stoi(currentWindowPosX) >= 0)
        FAIL_TEST("Expected the x coordinate of window of class \"a\" to be < 0, got {}.", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourFocusFallbackWithGroups) {

    // same idea as testScrollingViewBehaviourFocusFallback, but with window of class "a" being grouped.

    NLog::green("Testing scrolling view behaviour: focus fallback from floating window to a grouped tiled should not move scrolling view");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // to correctly set up windows for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    // only one tiled window will be grouped for the test
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    SPAWN_KITTY("a");

    // make it a grouped. There need not be any other windows in the group for this test
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");
    SPAWN_KITTY("c");

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

    if (std::stoi(currentWindowPosX) >= 0)
        FAIL_TEST("Expected the x coordinate of window of class \"a\" to be < 0, got {}.", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourWorkspaceChange) {

    /*
     When you change to a scrolling workspace, the focused window in that workspace must not be pulled into view, regardless of follow_focus
     ---------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: changing to a scrolling workspace should not move scrolling view");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // switch to workspace 1 for this test
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '1'})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

    // does not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    // change to workspace 2, then back to workspace 1 again
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '2'})"));
    OK(getFromSocket("/dispatch hl.dsp.focus({workspace = '1'})"));

    // If the scrolling view did not move, the x value for `at:` of the currently focused window, class:a, must be <0 (must be left of the viewport)
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourSpecialWorkspaceChange) {

    /*
     When you change to a special scrolling workspace from a normal workspace, the focused window in that workspace must not be pulled into view, regardless of follow_focus
     -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: changing to a special scrolling workspace from a normal workspace should not move scrolling view");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // We'll test in this special workspace
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

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

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourSpecialToSpecialWorkspaceChange) {

    /*
    We also test switching between 2 special workspaces
    This follows the same idea and dependencies as the test testScrollingViewBehaviourSpecialWorkspaceChange()
    */

    NLog::green("Testing scrolling view behaviour: changing to a special scrolling workspace from another special workspace should not move scrolling view");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    // We'll test in this special workspace
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('name:scroll_S')"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

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

    if (std::stoi(currentWindowPosSPECIALX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosSPECIALX);
}

TEST_CASE(testScrollingViewBehaviourCloseWindowInGroup) {

    /*
     When you change close a window inside a group (NOT destroying the group!), it should not cause scrolling view to shift to pull that group into view, regardless of follow_focus
     -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: closing a window in a group (> 1 window in group) should not move scrolling view");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    // We need 2 windows to be grouped, the third one not.
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    SPAWN_KITTY("b");

    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    SPAWN_KITTY("c");

    // switch focus to group. This will not move view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // kill window class:b. we expect that this should cause not difference in the position of the group
    OK(getFromSocket("/dispatch hl.dsp.window.kill({window = 'class:b'})"));
    Tests::waitUntilWindowsN(2);

    // If the scrolling view did not move, the x value for `at:` of the currently focused windows, class:a, must be <0 (must be left of the viewport)

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourMoveWindowIntoGroupFollowFocusFalse) {

    /*
     when a window is moved inside a group, scrolling view should not move to fit that group when follow_focus = false
     -----------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: moving a window into a group SHOULD NOT move scrolling view if follow_focus = 0");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    SPAWN_KITTY("b");
    SPAWN_KITTY("c");

    // focus class:b
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // the focus now should still be on class:b window. If the view did not move, its x coordinate for its `at:` value should be <0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'b' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourMoveWindowInGroupFollowFocusTrue) {

    /*
    when a window is moved inside a group, scrolling view should move to fit that group when follow_focus = true
    ------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: moving a window in a group SHOULD move scrolling view if follow_focus = true");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));

    SPAWN_KITTY("b");
    SPAWN_KITTY("c");

    // focus class:b
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // the focus now should still be on class:b window. If the scrolling view did move, its x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) < 0)
        FAIL_TEST("window of class 'b' does not have x coordinates >= 0 for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourNewLayer) {

    /*
     Starting a program on a different layer shouldn't cause scrolling view to move to fit the window that was focused when this program was started, regardless of follow_focus
     ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: new program occupying another layer shouldn't move scrolling view");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test - this is to avoid unwanted view shifts when setting up the windows
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

    // focus class:a - this does not move scrolling view when follow_focus = 0
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})"));

    NLog::yellow("Spawning kitty layer");
    SPAWN_LAYER_KITTY("myLayer");

    // If the scrolling view did not move, class:a window's x coordinate for its `at:` value should be <0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourMoveFocusFollowFocusFalse) {

    /*
     dispatching movefocus when follow_focus = false should not cause scrolling view to move
     ---------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: movefocus does not cause scrolling view to move if follow_focus = false");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

    // we expect that after dispatching this, scrolling view must not have moved
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // If the scrolling view did not move, class:a window's x coordinate for its `at:` value should be < 0.
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourMoveFocusFollowFocusTrue) {

    /*
     dispatching movefocus when follow_focus = true should cause scrolling view to move
     ----------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: movefocus does cause scrolling view to move if follow_focus = true");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

    // we expect that after dispatching this, scrolling view must have moved since follow_focus = true
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // If the scrolling view moved, class:a window's x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) < 0)
        FAIL_TEST("window of class 'a' does not have x coordinates >= 0 for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourMoveFocusInGroupFollowFocusFalse) {

    /*
     When movefocus is dispatched within groups to move focus from one group member to another, scrolling view must not move if follow_focus = false
     -----------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: movefocus within groups does not cause scrolling view to move if follow_focus = false");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // necessary to make sure movefocus first cycles through tabs in a group
    OK(getFromSocket("/eval hl.config({ binds = {movefocus_cycles_groupfirst = true}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");
    SPAWN_KITTY("c");

    // focus class:b. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // we move from one window of a group to another (from class:b to class:a) via movefocus
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // the focus now should still be on class:a window. If the scrolling view did not move, its x coordinate for its `at:` value should be < 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourMoveFocusInGroupFollowFocusTrue) {

    /*
     When movefocus is dispatched within groups to move focus from one group member to another, scrolling view must move if follow_focus = true
     ------------------------------------------------------------------------------------------------------------------------------------------
    */

    NLog::green("Testing scrolling view behaviour: movefocus within groups does causes scrolling view to move if follow_focus = true");

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    // necessary to make sure movefocus first cycles through tabs in a group
    OK(getFromSocket("/eval hl.config({ binds = {movefocus_cycles_groupfirst = true}})"));
    OK(getFromSocket("/eval hl.config({group = {auto_group = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.group.toggle({window = 'class:a'})"));
    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");
    SPAWN_KITTY("c");

    // focus class:b. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})"));

    // move it into the group where class:a is. This does not cause scrolling view to move when follow_focus = false
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    // we move from one window of a group to another (from class:b to class:a) via movefocus
    OK(getFromSocket("/dispatch hl.dsp.focus({direction = 'left'})"));

    // the focus now should still be on class:a window. If the scrolling view moved, its x coordinate for its `at:` value should be >= 0

    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) < 0)
        FAIL_TEST("window of class 'a' does not have x coordinates >= 0 for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollingViewBehaviourScheduledPropRefresh) {

    /*
    Test that hl.exec_scheduled_prop_refresh_immediately() should immediately execute prop refresh. This is tested via inhibiting scrollin during helper functs dispatch; if it works, the viewport
    should not move when a new workspace rule is created. If it doesn't, dispatch will miss because the refresh will be executed as another event 
    --------------------------------------------------------------------------------------------------------------------------------------
    */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test

    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");

    // since follow_focus = false, viewport does not move
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    // setting a workspace rule queues a doLater() call in the Event Loop Manager
    OK(getFromSocket("/eval hl.dispatch(hl.dsp.layout('inhibit_scroll true')); hl.workspace_rule({workspace = hl.get_active_workspace().id,gaps_in = 0}); "
                     "hl.exec_scheduled_prop_refresh_immediately(); hl.dispatch(hl.dsp.layout('inhibit_scroll false'));"));

    // Check that the workspace rule is set
    ASSERT_CONTAINS(getFromSocket("/workspacerules"), "gapsIn: 0 0 0 0");

    // The viewport must not have moved: left corner cords of window should be < 0
    const std::string currentWindowPos  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const std::string currentWindowPosX = currentWindowPos.substr(0, currentWindowPos.find(','));

    if (std::stoi(currentWindowPosX) > 0)
        FAIL_TEST("window of class 'a' does not have negative x coordinates for its position: {}", currentWindowPosX);
}

TEST_CASE(testScrollInhibitor) {

    /*
        scroll inhibitor prevent the scrolling view from moving
        ---------------------------------------------------------------------------------
    */

    // set current layout to scrolling
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    NLog::green("Testing inhibit_scroll");

    SPAWN_KITTY("a");

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    SPAWN_KITTY("b");
    // Currently, we are focused on window class:b

    // enable scroll inhibitor
    OK(getFromSocket("/dispatch hl.dsp.layout('inhibit_scroll 1')"));

    // dispatching `layoutmsg focus l` will move scrolling view regardless of follow_focus if inhibitor is not working
    OK(getFromSocket("/dispatch hl.dsp.layout('focus l')"));

    // the focus must have moved regardless of the state of the inhibitor (it only prevents the scrolling view from moving). We are now focused on window class:a

    // if the view does not move, we expect the x coordinate of the window of class "a" to be negative, as it would be to the left of the viewport
    const std::string posA  = Tests::getAttribute(getFromSocket("/activewindow"), "at");
    const int         posAx = std::stoi(posA.substr(0, posA.find(',')));

    if (posAx >= 0)
        FAIL_TEST("Expected the x coordinate of window of class \"a\" to be < 0, got {}.", posAx);
}

TEST_CASE(layoutmsg_fit_into_view) {

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    // ensure variables are correctly set for the test
    OK(getFromSocket("/eval hl.config({scrolling = {follow_focus = false}})"));

    if (!Tests::spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty with win class `a`");
        return;
    }

    OK(getFromSocket("/dispatch hl.dsp.layout('colresize 0.8')"));

    if (!Tests::spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty with win class `b`");
        return;
    }

    // class:a column is now off screen to the left

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));

    // fit class:a window into view

    OK(getFromSocket("/dispatch hl.dsp.layout('fit_into_view')"));

    // If it worked, class:a window must now have at: ~= 0,0 -- 0,0 + gaps, border = 22,22.

    ASSERT_CONTAINS(Tests::getAttribute(getFromSocket("/activewindow"), "at"), "22,22");
}

TEST_CASE(layoutRuleExpand) {
    // set current layout to scrolling
    OK(getFromSocket(
        "r/eval hl.config({ general = { layout = 'scrolling', gaps_in = 0, border_size = 0, gaps_out = 0 }, scrolling = {column_width = 0.5, fullscreen_on_one_column = true} })"));

    SPAWN_KITTY("a");

    const std::string sizeSingle  = Tests::getAttribute(getFromSocket("/activewindow"), "size");
    const int         sizeSingleX = std::stoi(sizeSingle.substr(0, sizeSingle.find(',')));

    for (auto const& win : {"b", "c"}) {
        SPAWN_KITTY(win);
    }

    OK(getFromSocket("dispatch hl.dsp.window.resize({x = 100, y = 500, window = 'class:a'})"));
    OK(getFromSocket("dispatch hl.dsp.window.resize({x = 100, y = 500, window = 'class:c'})"));

    OK(getFromSocket("dispatch hl.dsp.focus({window = 'class:b'})"));

    // const std::string sizeBefore  = Tests::getAttribute(getFromSocket("/activewindow"), "size");
    // const int         sizeBeforeX = std::stoi(sizeBefore.substr(0, sizeBefore.find(',')));

    OK(getFromSocket("/dispatch hl.dsp.layout('fit expand')"));

    const std::string sizeAfter  = Tests::getAttribute(getFromSocket("/activewindow"), "size");
    const int         sizeAfterX = std::stoi(sizeAfter.substr(0, sizeAfter.find(',')));

    if (sizeAfterX < sizeSingleX - 200)
        FAIL_TEST("Expected the width of window of class \"b\" to take up all remaining space {}, got {}.", sizeSingleX - 200, sizeAfterX);
}
TEST_CASE(scrollTapeOnClickOutOfWindow) {
    /*
     * Do not move tape on click in the direction, but out of the window  
     */

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));
    OK(getFromSocket("r/eval hl.config({ general = { gaps_out = 100 } })"));
    OK(getFromSocket("r/eval hl.config({ scrolling = { follow_min_visible = 1.0, column_width = 0.6 } })"));
    OK(getFromSocket("r/eval hl.config({ input = { follow_mouse = 1 } })"));

    SPAWN_KITTY("A"); // A should be at x negative
    SPAWN_KITTY("B");

    OK(getFromSocket("/eval hl.plugin.test.window_soft_focus('A')"));     // soft focus A
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 0, y = 20 })")); // move cursor to the gap zone

    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));

    const auto active = getFromSocket("/activewindow");
    ASSERT_CONTAINS(active, "class: A");

    const auto posA  = Tests::getAttribute(active, "at");
    const auto posAx = std::stoi(posA.substr(0, posA.find(',')));

    if (posAx >= 0)
        FAIL_TEST("Expected the x coordinate of window of class \"A\" to be < 0, got {}.", posAx);
}

TEST_CASE(properFocusBehvaior) {
    // test that focus history does not fuck with proper workspace preference

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'scrolling' } })"));

    if (getFromSocket("/monitors all").contains("HEADLESS-3"))
        OK(getFromSocket("/output remove HEADLESS-3"));

    OK(getFromSocket("/output create headless HEADLESS-3"));
    CScopeGuard x([&] {
        if (getFromSocket("/monitors all").contains("HEADLESS-3"))
            OK(getFromSocket("/output remove HEADLESS-3"));
    });

    auto        test = [&] {
        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));

        SPAWN_KITTY("a");
        Tests::waitUntilWindowsN(1);

        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));

        SPAWN_KITTY("b");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:b' })"));
        SPAWN_KITTY("c");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:c' })"));
        SPAWN_KITTY("d");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:d' })"));

        Tests::waitUntilWindowsN(4);

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: d");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: b");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: a");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: b");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: d");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        // now we have a situation of:
        // HEADLESS-2: a
        // HEADLESS-3: b | c d | -> b is offscreen

        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: a");
        }

        OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: c");
        }

        // now we have a history of a being more recent than b, but if we move left, we should still focus b.

        OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

        {
            const auto str = getFromSocket("/activewindow");
            EXPECT_CONTAINS(str, "class: b");
        }

        Tests::killAllWindows();
        Tests::waitUntilWindowsN(0);
    };

    OK(getFromSocket("/eval hl.config({ binds = { focus_preferred_method = 0 } })")); // set history mode, default
    test();

    OK(getFromSocket("/eval hl.config({ binds = { focus_preferred_method = 1 } })")); // set length mode
    test();
}
