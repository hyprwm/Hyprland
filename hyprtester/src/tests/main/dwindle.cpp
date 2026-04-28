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
    std::string posBefore = Tests::getWindowAttribute(getFromSocket("/activewindow"), "at:");

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move_to_corner({ corner = 3 })")); // top left
    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = '2' })"));

    // Should be moved to the right, so the position should change
    std::string activeWindow = getFromSocket("/activewindow");
    EXPECT(activeWindow.contains(posBefore), false);
}
