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

    // clean up
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing layout generic", Colors::GREEN);

    // setup
    OK(getFromSocket("/dispatch workspace 10"));

    // test
    NLog::log("{}Testing `single_window_aspect_ratio`", Colors::GREEN);
    swar();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
