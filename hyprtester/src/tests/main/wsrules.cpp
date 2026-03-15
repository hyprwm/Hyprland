#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <thread>
#include <chrono>

static int  ret = 0;

static void testLayoutOverride() {
    // workspace 700 uses master while the default is dwindle
    OK(getFromSocket("r/keyword workspace 700,layout:master"));
    OK(getFromSocket("/dispatch workspace 700"));

    for (auto const& win : {"wsr_a", "wsr_b"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // verify master layout is active by using a master-specific layoutmsg
    OK(getFromSocket("/dispatch layoutmsg orientationnext"));

    // now verify that the default workspace still uses dwindle by checking that
    // a dwindle-specific layoutmsg works there
    Tests::killAllWindows();
    OK(getFromSocket("/dispatch workspace 701"));

    for (auto const& win : {"wsr_c", "wsr_d"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    // togglesplit is dwindle-specific and should succeed
    OK(getFromSocket("/dispatch layoutmsg togglesplit"));

    Tests::killAllWindows();
}

static void testGapsOverride() {
    OK(getFromSocket("r/keyword general:gaps_in 5"));
    OK(getFromSocket("r/keyword general:gaps_out 20"));
    OK(getFromSocket("r/keyword general:border_size 0"));

    OK(getFromSocket("r/keyword workspace 702,gapsout:50,gapsin:0"));
    OK(getFromSocket("/dispatch workspace 702"));

    if (!Tests::spawnKitty("gapws_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // single window with gapsout:50 and border_size:0 should be at 50,50 with size 1820x980
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 50,50");
        EXPECT_CONTAINS(str, "size: 1820,980");
    }

    Tests::killAllWindows();
}

static void testOnCreatedEmpty() {
    OK(getFromSocket("r/keyword workspace 703,on-created-empty:kitty --class on_created_kitty"));
    OK(getFromSocket("/dispatch workspace 703"));

    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: on_created_kitty");
    }

    Tests::killAllWindows();
}

static void testDecorateOverride() {
    OK(getFromSocket("r/keyword workspace 704,decorate:0"));
    OK(getFromSocket("/dispatch workspace 704"));

    if (!Tests::spawnKitty("decws_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto res = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(res, "0");
    }

    Tests::killAllWindows();
}

static void testBorderSizeOverride() {
    OK(getFromSocket("r/keyword workspace 705,bordersize:10"));
    OK(getFromSocket("/dispatch workspace 705"));

    if (!Tests::spawnKitty("bsws_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    {
        auto res = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(res, "10");
    }

    Tests::killAllWindows();
}

static void testDefaultName() {
    OK(getFromSocket("r/keyword workspace 706,defaultName:MyTestWorkspace"));
    OK(getFromSocket("/dispatch workspace 706"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "MyTestWorkspace");
    }

    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing workspace rules", Colors::GREEN);

    NLog::log("{}Testing layout override per workspace", Colors::GREEN);
    testLayoutOverride();

    NLog::log("{}Testing gapsout override per workspace", Colors::GREEN);
    testGapsOverride();

    NLog::log("{}Testing on-created-empty workspace rule", Colors::GREEN);
    testOnCreatedEmpty();

    NLog::log("{}Testing decorate override per workspace", Colors::GREEN);
    testDecorateOverride();

    NLog::log("{}Testing bordersize override per workspace", Colors::GREEN);
    testBorderSizeOverride();

    NLog::log("{}Testing defaultName workspace rule", Colors::GREEN);
    testDefaultName();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
