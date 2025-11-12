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
    OK(getFromSocket("/keyword monitor addreserved, 0, 20, 0, 20"));
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
}

static bool test() {
    NLog::log("{}Testing Dwindle layout", Colors::GREEN);

    // test
    NLog::log("{}Testing float clamp", Colors::GREEN);
    testFloatClamp();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    getFromSocket("/dispatch workspace 1");
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
