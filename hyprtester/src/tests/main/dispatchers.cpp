#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <thread>
#include <chrono>

static int  ret = 0;

static void testCenterWindow() {
    OK(getFromSocket("/dispatch workspace 800"));

    if (!Tests::spawnKitty("cw_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch setfloating class:cw_kitty"));
    OK(getFromSocket("/dispatch resizewindowpixel exact 400 300,class:cw_kitty"));
    OK(getFromSocket("/dispatch centerwindow"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
        // centered on 1920x1080: (1920-400)/2=760, (1080-300)/2=390
        EXPECT_CONTAINS(str, "at: 760,390");
        EXPECT_CONTAINS(str, "size: 400,300");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testRenameWorkspace() {
    OK(getFromSocket("/dispatch workspace 801"));

    if (!Tests::spawnKitty("rn_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch renameworkspace 801 MyCustomName"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "MyCustomName");
    }

    // clear the name
    OK(getFromSocket("/dispatch renameworkspace 801"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_NOT_CONTAINS(str, "MyCustomName");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testPseudoToggle() {
    OK(getFromSocket("/dispatch workspace 802"));
    OK(getFromSocket("/keyword general:layout dwindle"));
    OK(getFromSocket("/keyword dwindle:pseudotile true"));

    for (auto const& win : {"ps_a", "ps_b"}) {
        if (!Tests::spawnKitty(win)) {
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch focuswindow class:ps_b"));

    auto beforeStr  = getFromSocket("/activewindow");
    auto beforeSize = Tests::getWindowAttribute(beforeStr, "size:");

    OK(getFromSocket("/dispatch pseudo"));

    {
        auto str       = getFromSocket("/activewindow");
        auto afterSize = Tests::getWindowAttribute(str, "size:");
        // pseudo-tiled windows use their preferred size, which should differ from the tiled allocation
        EXPECT_NOT(afterSize, beforeSize);
    }

    // toggle back
    OK(getFromSocket("/dispatch pseudo"));

    {
        auto str          = getFromSocket("/activewindow");
        auto revertedSize = Tests::getWindowAttribute(str, "size:");
        EXPECT(revertedSize, beforeSize);
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testLockGroups() {
    OK(getFromSocket("/dispatch workspace 803"));

    if (!Tests::spawnKitty("lg_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch togglegroup"));

    if (!Tests::spawnKitty("lg_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // both should be grouped (grouped shows comma-separated addresses, not "0")
    EXPECT(Tests::windowCount(), 2);
    {
        auto str = getFromSocket("/clients");
        EXPECT_NOT_CONTAINS(str, "grouped: 0");
    }

    // lock all groups globally
    OK(getFromSocket("/dispatch lockgroups lock"));

    // spawn a new window -- it should NOT enter the group
    if (!Tests::spawnKitty("lg_c")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT(Tests::windowCount(), 3);
    {
        auto str = getFromSocket("/clients");
        // lg_c should not be grouped
        auto cPos = str.find("class: lg_c");
        if (cPos != std::string::npos) {
            auto entryStart = str.rfind("Window ", cPos);
            auto entryEnd   = str.find("\n\n", cPos);
            auto entry      = str.substr(entryStart, entryEnd - entryStart);
            EXPECT_CONTAINS(entry, "grouped: 0");
        } else {
            NLog::log("{}Could not find lg_c in clients", Colors::RED);
            ++TESTS_FAILED;
            ret = 1;
        }
    }

    // unlock
    OK(getFromSocket("/dispatch lockgroups unlock"));

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testLockActiveGroup() {
    OK(getFromSocket("/dispatch workspace 804"));

    if (!Tests::spawnKitty("lag_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch togglegroup"));

    if (!Tests::spawnKitty("lag_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // lock only this group
    OK(getFromSocket("/dispatch lockactivegroup lock"));

    if (!Tests::spawnKitty("lag_c")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT(Tests::windowCount(), 3);
    {
        auto str  = getFromSocket("/clients");
        auto cPos = str.find("class: lag_c");
        if (cPos != std::string::npos) {
            auto entryStart = str.rfind("Window ", cPos);
            auto entryEnd   = str.find("\n\n", cPos);
            auto entry      = str.substr(entryStart, entryEnd - entryStart);
            EXPECT_CONTAINS(entry, "grouped: 0");
        } else {
            NLog::log("{}Could not find lag_c in clients", Colors::RED);
            ++TESTS_FAILED;
            ret = 1;
        }
    }

    // focus back to a grouped window before unlocking
    OK(getFromSocket("/dispatch focuswindow class:lag_a"));
    OK(getFromSocket("/dispatch lockactivegroup unlock"));

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testTagWindow() {
    OK(getFromSocket("/dispatch workspace 805"));

    if (!Tests::spawnKitty("tw_kitty")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // tag the window via the dispatcher (not via a rule)
    OK(getFromSocket("/dispatch tagwindow +myDispatchTag"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "myDispatchTag");
    }

    // remove the tag
    OK(getFromSocket("/dispatch tagwindow -myDispatchTag"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_NOT_CONTAINS(str, "myDispatchTag");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testFocusCurrentOrLast() {
    OK(getFromSocket("/dispatch workspace 806"));

    if (!Tests::spawnKitty("fcol_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    if (!Tests::spawnKitty("fcol_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: fcol_b");

    // focuscurrentorlast should toggle to the previous window
    OK(getFromSocket("/dispatch focuscurrentorlast"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: fcol_a");

    // and back
    OK(getFromSocket("/dispatch focuscurrentorlast"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: fcol_b");

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testFocusUrgentOrLast() {
    OK(getFromSocket("/dispatch workspace 807"));

    if (!Tests::spawnKitty("fuol_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    if (!Tests::spawnKitty("fuol_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    if (!Tests::spawnKitty("fuol_c")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: fuol_c");

    // no urgent window, so it should focus the last (previous) window
    OK(getFromSocket("/dispatch focusurgentorlast"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: fuol_b");

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testAlterZOrder() {
    OK(getFromSocket("/dispatch workspace 808"));
    OK(getFromSocket("/keyword input:follow_mouse 2"));
    OK(getFromSocket("/keyword input:float_switch_override_focus 0"));

    if (!Tests::spawnKitty("zord_a") || !Tests::spawnKitty("zord_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch setfloating class:zord_a"));
    OK(getFromSocket("/dispatch setfloating class:zord_b"));

    // b is on top (last spawned). Move a to top
    OK(getFromSocket("/dispatch focuswindow class:zord_a"));
    OK(getFromSocket("/dispatch alterzorder top"));

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: zord_a");

    // move a to bottom
    OK(getFromSocket("/dispatch alterzorder bottom"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: zord_a");

    // bringactivetotop brings it back
    OK(getFromSocket("/dispatch bringactivetotop"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: zord_a");

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testMoveToWorkspaceSilent() {
    OK(getFromSocket("/dispatch workspace 810"));

    if (!Tests::spawnKitty("mts_a") || !Tests::spawnKitty("mts_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mts_b");
    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "workspace ID 810 ");

    // move b to workspace 811 silently — active workspace should NOT change
    OK(getFromSocket("/dispatch movetoworkspacesilent 811"));

    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "workspace ID 810 ");
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mts_a");

    // verify b is now on workspace 811
    OK(getFromSocket("/dispatch workspace 811"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: mts_b");
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "workspace: 811");

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testCircleNext() {
    OK(getFromSocket("/dispatch workspace 812"));

    if (!Tests::spawnKitty("cn_a") || !Tests::spawnKitty("cn_b") || !Tests::spawnKitty("cn_c")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: cn_c");

    OK(getFromSocket("/dispatch cyclenext"));
    auto first = Tests::getWindowAttribute(getFromSocket("/activewindow"), "class:");
    OK(getFromSocket("/dispatch cyclenext"));
    auto second = Tests::getWindowAttribute(getFromSocket("/activewindow"), "class:");
    OK(getFromSocket("/dispatch cyclenext"));
    auto third = Tests::getWindowAttribute(getFromSocket("/activewindow"), "class:");

    // cycling 3 times should visit all 3 windows and return
    EXPECT_NOT(first, second);
    EXPECT_NOT(second, third);

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testSwapNext() {
    OK(getFromSocket("/dispatch workspace 813"));

    if (!Tests::spawnKitty("sn_a") || !Tests::spawnKitty("sn_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch focuswindow class:sn_a"));
    auto beforePos = Tests::getWindowAttribute(getFromSocket("/activewindow"), "at:");

    OK(getFromSocket("/dispatch swapnext"));

    auto afterPos = Tests::getWindowAttribute(getFromSocket("/activewindow"), "at:");

    // position should change after swap
    EXPECT_NOT(beforePos, afterPos);

    // focus should still be on sn_a
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: sn_a");

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testDpms() {
    // dpms off should turn off display, on should restore
    OK(getFromSocket("/dispatch dpms off"));
    EXPECT_CONTAINS(getFromSocket("/monitors"), "dpmsStatus: 0");

    OK(getFromSocket("/dispatch dpms on"));
    EXPECT_CONTAINS(getFromSocket("/monitors"), "dpmsStatus: 1");

    OK(getFromSocket("/reload"));
}

static void testSetIgnoreGroupLock() {
    OK(getFromSocket("/dispatch workspace 814"));
    OK(getFromSocket("/keyword general:layout dwindle"));
    OK(getFromSocket("/keyword dwindle:force_split 2"));

    if (!Tests::spawnKitty("igl_a") || !Tests::spawnKitty("igl_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // put igl_a in a locked group; igl_b is outside to the right
    OK(getFromSocket("/dispatch focuswindow class:igl_a"));
    OK(getFromSocket("/dispatch togglegroup"));
    OK(getFromSocket("/dispatch lockactivegroup lock"));

    // with the group locked, moveintogroup should move igl_b rather than grouping it
    OK(getFromSocket("/dispatch focuswindow class:igl_b"));
    OK(getFromSocket("/dispatch moveintogroup l"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "grouped: 0");

    // enable setignoregrouplock — moveintogroup should now bypass the lock
    OK(getFromSocket("/dispatch setignoregrouplock lock"));
    OK(getFromSocket("/dispatch focuswindow class:igl_b"));
    OK(getFromSocket("/dispatch moveintogroup l"));
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), "grouped: 0");

    // clean up
    OK(getFromSocket("/dispatch setignoregrouplock unlock"));
    OK(getFromSocket("/dispatch lockactivegroup unlock"));

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
}

static void testMoveCursor() {
    OK(getFromSocket("/dispatch movecursor 500 300"));
    EXPECT_CONTAINS(getFromSocket("/cursorpos"), "500, 300");

    OK(getFromSocket("/dispatch movecursor 0 0"));
    EXPECT_CONTAINS(getFromSocket("/cursorpos"), "0, 0");

    OK(getFromSocket("/dispatch movecursor 1919 1079"));
    EXPECT_CONTAINS(getFromSocket("/cursorpos"), "1919, 1079");
}

static bool test() {
    NLog::log("{}Testing miscellaneous dispatchers", Colors::GREEN);

    NLog::log("{}Testing centerwindow", Colors::GREEN);
    testCenterWindow();

    NLog::log("{}Testing renameworkspace", Colors::GREEN);
    testRenameWorkspace();

    NLog::log("{}Testing pseudo toggle", Colors::GREEN);
    testPseudoToggle();

    NLog::log("{}Testing lockgroups", Colors::GREEN);
    testLockGroups();

    NLog::log("{}Testing lockactivegroup", Colors::GREEN);
    testLockActiveGroup();

    NLog::log("{}Testing tagwindow dispatcher", Colors::GREEN);
    testTagWindow();

    NLog::log("{}Testing focuscurrentorlast", Colors::GREEN);
    testFocusCurrentOrLast();

    NLog::log("{}Testing focusurgentorlast", Colors::GREEN);
    testFocusUrgentOrLast();

    NLog::log("{}Testing alterzorder", Colors::GREEN);
    testAlterZOrder();

    NLog::log("{}Testing movetoworkspacesilent", Colors::GREEN);
    testMoveToWorkspaceSilent();

    NLog::log("{}Testing cyclenext", Colors::GREEN);
    testCircleNext();

    NLog::log("{}Testing swapnext", Colors::GREEN);
    testSwapNext();

    NLog::log("{}Testing dpms", Colors::GREEN);
    testDpms();

    NLog::log("{}Testing setIgnoreGroupLock", Colors::GREEN);
    testSetIgnoreGroupLock();

    NLog::log("{}Testing movecursor", Colors::GREEN);
    testMoveCursor();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
