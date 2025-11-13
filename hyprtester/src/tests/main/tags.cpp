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

    OK(getFromSocket("/keyword windowrule[tag-test-1]:tag +testTag"));
    OK(getFromSocket("/keyword windowrule[tag-test-1]:match:class tagged"));
    OK(getFromSocket("/keyword windowrule[tag-test-2]:match:tag negative:testTag"));
    OK(getFromSocket("/keyword windowrule[tag-test-2]:no_shadow true"));
    OK(getFromSocket("/keyword windowrule[tag-test-3]:match:tag testTag"));
    OK(getFromSocket("/keyword windowrule[tag-test-3]:no_dim true"));

    EXPECT(Tests::windowCount(), 2);
    OK(getFromSocket("/dispatch focuswindow class:tagged"));
    NLog::log("{}Testing tagged window for no_dim 0 & no_shadow", Colors::YELLOW);
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_dim"), "false");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_shadow"), "true");
    NLog::log("{}Testing untagged window for no_dim & no_shadow", Colors::YELLOW);
    OK(getFromSocket("/dispatch focuswindow class:untagged"));
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), "testTag");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_shadow"), "false");
    EXPECT_CONTAINS(getFromSocket("/getprop activewindow no_dim"), "true");

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    OK(getFromSocket("/reload"));

    return ret == 0;
}

REGISTER_TEST_FN(testTags)
