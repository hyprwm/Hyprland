#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

TEST_CASE(single_window_aspect_ratio) {
    OK(getFromSocket("/eval hl.config({ layout = { single_window_aspect_ratio = '1 1' } })"));

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 442,22");
        ASSERT_CONTAINS(str, "size: 1036,1036");
    }

    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));

    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 442,22");
        ASSERT_CONTAINS(str, "size: 1036,1036");
    }

    // don't use swar on maximized
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "size: 1876,1036");
    }
}

// Don't crash when focus after global geometry changes
TEST_CASE(crashOnGeomUpdate) {
    Tests::spawnKitty();
    Tests::spawnKitty();
    Tests::spawnKitty();

    // move the layout
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '1000x0', scale = '1' })"));

    // shouldnt crash
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));
}

// Test if size + pos is preserved after fs cycle
TEST_CASE(posPreserve) {
    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 1337, y = 69, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 420, y = 420, window = 'class:kitty' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 420,420");
        ASSERT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 581,420");
        ASSERT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "at: 581,420");
        ASSERT_CONTAINS(str, "size: 1337,69");
    }
}

TEST_CASE(focusMRUAfterClose) {
    NLog::log("{}Testing focus after close (MRU order)", Colors::GREEN);

    OK(getFromSocket("/reload"));
    OK(getFromSocket("/eval hl.config({ dwindle = { default_split_ratio = 1.25 } })"));
    OK(getFromSocket("/eval hl.config({ input = { focus_on_close = 2 } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);
    ASSERT(!!Tests::spawnKitty("kitty_C"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_C' })"));

    OK(getFromSocket("/dispatch hl.dsp.window.kill()"));
    Tests::waitUntilWindowsN(2);

    {
        auto str = getFromSocket("/activewindow");
        ASSERT(str.contains("class: kitty_B"), true);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.kill()"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        ASSERT(str.contains("class: kitty_A"), true);
    }
}

TEST_CASE(focusPreservedLayoutChange) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);
    ASSERT(!!Tests::spawnKitty("kitty_C"), true);
    ASSERT(!!Tests::spawnKitty("kitty_D"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_C' })"));

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'monocle' } })"));

    {
        auto str = getFromSocket("/activewindow");
        ASSERT(str.contains("class: kitty_C"), true);
    }
}
