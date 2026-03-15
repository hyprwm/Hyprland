#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int  ret = 0;

static void testFloatClamp() {
    for (auto const& win : {"a", "b", "c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/keyword dwindle:force_split 2"));
    OK(getFromSocket("/keyword monitor HEADLESS-2, addreserved, 0, 20, 0, 20"));
    OK(getFromSocket("/dispatch focuswindow class:c"));
    OK(getFromSocket("/dispatch setfloating class:c"));
    OK(getFromSocket("/dispatch resizewindowpixel exact 1200 900,class:c"));
    OK(getFromSocket("/dispatch settiled class:c"));
    OK(getFromSocket("/dispatch setfloating class:c"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 698,158");
        EXPECT_CONTAINS(str, "size: 1200,900");
    }

    OK(getFromSocket("/keyword dwindle:force_split 0"));

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    OK(getFromSocket("/reload"));
}

static void test13349() {

    // Test if dwindle properly uses a focal point to place a new window.
    // exposed by #13349 as a regression from #12890

    for (auto const& win : {"a", "b", "c"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch focuswindow class:c"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 967,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }

    OK(getFromSocket("/dispatch movewindow l"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }

    OK(getFromSocket("/dispatch movewindow r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 967,547");
        EXPECT_CONTAINS(str, "size: 931,511");
    }

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testSplit() {
    // Test various split methods

    Tests::spawnKitty("a");

    // these must not crash
    EXPECT_NOT(getFromSocket("/dispatch layoutmsg swapsplit"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch layoutmsg splitratio 1 exact"), "ok");

    Tests::spawnKitty("b");

    OK(getFromSocket("/dispatch focuswindow class:a"));
    OK(getFromSocket("/dispatch layoutmsg splitratio -0.2"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 743,1036");
    }

    OK(getFromSocket("/dispatch layoutmsg splitratio 1.6 exact"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1495,1036");
    }

    EXPECT_NOT(getFromSocket("/dispatch layoutmsg splitratio fhne exact"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch layoutmsg splitratio exact"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch layoutmsg splitratio -....9"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch layoutmsg splitratio ..9"), "ok");
    EXPECT_NOT(getFromSocket("/dispatch layoutmsg splitratio"), "ok");

    OK(getFromSocket("/dispatch layoutmsg togglesplit"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,823");
    }

    OK(getFromSocket("/dispatch layoutmsg swapsplit"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,859");
        EXPECT_CONTAINS(str, "size: 1876,199");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testRotatesplit() {
    OK(getFromSocket("r/keyword general:gaps_in 0"));
    OK(getFromSocket("r/keyword general:gaps_out 0"));
    OK(getFromSocket("r/keyword general:border_size 0"));

    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    // test 4 repeated rotations by 90 degrees
    OK(getFromSocket("/dispatch layoutmsg rotatesplit"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch layoutmsg rotatesplit"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch layoutmsg rotatesplit"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,540");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch layoutmsg rotatesplit"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    // test different angles
    OK(getFromSocket("/dispatch layoutmsg rotatesplit 180"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch layoutmsg rotatesplit 270"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,540");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    OK(getFromSocket("/dispatch layoutmsg rotatesplit 360"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 1920,540");
    }

    // test negative angles
    OK(getFromSocket("/dispatch layoutmsg rotatesplit -90"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 0,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    OK(getFromSocket("/dispatch layoutmsg rotatesplit -180"));
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 960,0");
        EXPECT_CONTAINS(str, "size: 960,1080");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    OK(getFromSocket("/reload"));
}

static void testForceSplitOnMoveToWorkspace() {
    OK(getFromSocket("/dispatch workspace 2"));
    EXPECT(!!Tests::spawnKitty("kitty"), true);

    OK(getFromSocket("/dispatch workspace 1"));
    EXPECT(!!Tests::spawnKitty("kitty"), true);
    std::string posBefore = Tests::getWindowAttribute(getFromSocket("/activewindow"), "at:");

    OK(getFromSocket("/keyword dwindle:force_split 2"));
    OK(getFromSocket("/dispatch movecursortocorner 3")); // top left
    OK(getFromSocket("/dispatch movetoworkspace 2"));

    // Should be moved to the right, so the position should change
    std::string activeWindow = getFromSocket("/activewindow");
    EXPECT(activeWindow.contains(posBefore), false);

    // clean up
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
    Tests::waitUntilWindowsN(0);
}

static void testPreserveSplit() {
    OK(getFromSocket("r/keyword dwindle:preserve_split true"));
    OK(getFromSocket("r/keyword general:gaps_in 0"));
    OK(getFromSocket("r/keyword general:gaps_out 0"));
    OK(getFromSocket("r/keyword general:border_size 0"));

    for (auto const& win : {"ps_a", "ps_b"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    auto splitBefore = Tests::getWindowAttribute(getFromSocket("/activewindow"), "at:");

    // swap positions -- with preserve_split, the split direction stays the same
    OK(getFromSocket("/dispatch swapwindow l"));

    auto splitAfter = Tests::getWindowAttribute(getFromSocket("/activewindow"), "size:");

    // spawn a third window: the split direction at the root should be preserved
    Tests::spawnKitty("ps_c");

    {
        auto str = getFromSocket("/clients");
        // with preserve_split, after vertical root split, the third window splits horizontally from the second
        EXPECT_CONTAINS(str, "at: 0,0");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

static void testSpecialScaleFactor() {
    OK(getFromSocket("r/keyword dwindle:special_scale_factor 0.5"));
    OK(getFromSocket("r/keyword general:gaps_in 0"));
    OK(getFromSocket("r/keyword general:gaps_out 0"));
    OK(getFromSocket("r/keyword general:border_size 0"));

    OK(getFromSocket("/dispatch workspace special:scale_test"));

    if (!Tests::spawnKitty("ssf_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        // at 0.5 scale on 1920x1080, window should be 960x540 centered at (480, 270)
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 960,540");
        EXPECT_CONTAINS(str, "at: 480,270");
    }

    OK(getFromSocket("/dispatch togglespecialworkspace scale_test"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/reload"));
}

static bool test() {
    NLog::log("{}Testing Dwindle layout", Colors::GREEN);

    // test
    NLog::log("{}Testing float clamp", Colors::GREEN);
    testFloatClamp();

    NLog::log("{}Testing #13349", Colors::GREEN);
    test13349();

    NLog::log("{}Testing splits", Colors::GREEN);
    testSplit();

    NLog::log("{}Testing rotatesplit", Colors::GREEN);
    testRotatesplit();

    NLog::log("{}Testing force_split on move to workspace", Colors::GREEN);
    testForceSplitOnMoveToWorkspace();

    NLog::log("{}Testing preserve_split", Colors::GREEN);
    testPreserveSplit();

    NLog::log("{}Testing special_scale_factor", Colors::GREEN);
    testSpecialScaleFactor();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    getFromSocket("/dispatch workspace 1");
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
