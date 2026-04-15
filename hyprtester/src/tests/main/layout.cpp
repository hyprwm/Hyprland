#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int  ret = 0;

static void swar() {
    OK(getFromSocket("/keyword layout:single_window_aspect_ratio 1 1"));

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 442,22");
        EXPECT_CONTAINS(str, "size: 1036,1036");
    }

    Tests::spawnKitty();

    OK(getFromSocket("/dispatch killwindow activewindow"));

    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 442,22");
        EXPECT_CONTAINS(str, "size: 1036,1036");
    }

    // don't use swar on maximized
    OK(getFromSocket("/dispatch fullscreen 1"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

// Don't crash when focus after global geometry changes
static void testCrashOnGeomUpdate() {
    Tests::spawnKitty();
    Tests::spawnKitty();
    Tests::spawnKitty();

    // move the layout
    OK(getFromSocket("/keyword monitor HEADLESS-2,1920x1080@60,1000x0,1"));

    // shouldnt crash
    OK(getFromSocket("/dispatch movefocus r"));

    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

// Test if size + pos is preserved after fs cycle
static void testPosPreserve() {
    Tests::spawnKitty();

    OK(getFromSocket("/dispatch setfloating class:kitty"));
    OK(getFromSocket("/dispatch resizewindowpixel exact 1337 69, class:kitty"));
    OK(getFromSocket("/dispatch movewindowpixel exact 420 420, class:kitty"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 420,420");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch fullscreen"));
    OK(getFromSocket("/dispatch fullscreen"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch movewindow r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 581,420");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch fullscreen"));
    OK(getFromSocket("/dispatch fullscreen"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 581,420");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static bool testFocusMRUAfterClose() {
    NLog::log("{}Testing focus after close (MRU order)", Colors::GREEN);

    OK(getFromSocket("/reload"));
    OK(getFromSocket("/keyword dwindle:default_split_ratio 1.25"));
    OK(getFromSocket("/keyword input:focus_on_close 2"));

    EXPECT(!!Tests::spawnKitty("kitty_A"), true);
    EXPECT(!!Tests::spawnKitty("kitty_B"), true);
    EXPECT(!!Tests::spawnKitty("kitty_C"), true);

    OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
    OK(getFromSocket("/dispatch focuswindow class:kitty_B"));
    OK(getFromSocket("/dispatch focuswindow class:kitty_C"));

    OK(getFromSocket("/dispatch killactive"));
    Tests::waitUntilWindowsN(2);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_B"), true);
    }

    OK(getFromSocket("/dispatch killactive"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_A"), true);
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);
    return true;
}

static bool testFocusPreservedLayoutChange() {
    NLog::log("{}Testing focus is preserved on layout change", Colors::GREEN);

    OK(getFromSocket("/keyword general:layout master"));

    EXPECT(!!Tests::spawnKitty("kitty_A"), true);
    EXPECT(!!Tests::spawnKitty("kitty_B"), true);
    EXPECT(!!Tests::spawnKitty("kitty_C"), true);
    EXPECT(!!Tests::spawnKitty("kitty_D"), true);

    OK(getFromSocket("/dispatch focuswindow class:kitty_C"));

    OK(getFromSocket("/keyword general:layout monocle"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_C"), true);
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);
    return true;
}

static bool test() {
    NLog::log("{}Testing layout generic", Colors::GREEN);

    // setup
    OK(getFromSocket("/dispatch workspace 10"));

    // test
    NLog::log("{}Testing `single_window_aspect_ratio`", Colors::GREEN);
    swar();

    testCrashOnGeomUpdate();
    testPosPreserve();
    testFocusMRUAfterClose();
    testFocusPreservedLayoutChange();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
