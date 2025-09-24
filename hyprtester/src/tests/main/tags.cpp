#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

static int  ret = 0;

static bool testTags() {
    NLog::log("{}Testing tags", Colors::GREEN);

    EXPECT(Tests::windowCount(), 0);

    NLog::log("{}Spawning kittyProcA&B on ws 1", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty("tagged");
    auto kittyProcB = Tests::spawnKitty("untagged");

    if (!kittyProcA || !kittyProcB) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Testing testTag tags", Colors::YELLOW);

    OK(getFromSocket("/keyword windowrule tag +testTag, class:tagged"));
    OK(getFromSocket("/keyword windowrule noshadow, tag:negative:testTag"));
    OK(getFromSocket("/keyword windowrule noborder, tag:testTag"));

    EXPECT(Tests::windowCount(), 2);
    OK(getFromSocket("/dispatch focuswindow class:tagged"));
    NLog::log("{}Testing tagged window for noborder & noshadow", Colors::YELLOW);
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow noborder"), "true");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow noshadow"), "false");
    NLog::log("{}Testing untagged window for noborder & noshadow", Colors::YELLOW);
    OK(getFromSocket("/dispatch focuswindow class:untagged"));
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow noborder"), "false");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow noshadow"), "true");

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    OK(getFromSocket("/reload"));

    return ret == 0;
}

REGISTER_TEST_FN(testTags)
