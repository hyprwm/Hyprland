#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <print>
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <csignal>
#include <cerrno>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

static bool test() {
    NLog::log("{}Testing groups", Colors::GREEN);

    // test on workspace "window"
    NLog::log("{}Dispatching workspace `groups`", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:groups");

    NLog::log("{}Testing movewindoworgroup from group to group", Colors::YELLOW);
    auto kittyA = Tests::spawnKitty("kittyA");
    if (!kittyA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    // check kitty properties. One kitty should take the entire screen, minus the gaps.
    NLog::log("{}Check kittyA dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "size: 1876,1036", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    auto kittyB = Tests::spawnKitty("kittyB");
    if (!kittyB) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    OK(getFromSocket("/dispatch focuswindow class:kittyB"));
    OK(getFromSocket("/dispatch togglegroup"));
    OK(getFromSocket("/dispatch focuswindow class:kittyA"));
    OK(getFromSocket("/dispatch togglegroup"));

    NLog::log("{}Check kittyB dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_COUNT_STRING(str, "size: 931,1015", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    auto kittyC = Tests::spawnKitty("kittyC");
    if (!kittyC) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Check kittyC dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_COUNT_STRING(str, "size: 931,1015", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    OK(getFromSocket("/dispatch movewindoworgroup r"));
    NLog::log("{}Check that dimensions remain the same after move", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_COUNT_STRING(str, "size: 931,1015", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    // kill all
    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Spawning kittyProcA", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty();
    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 1);

    // check kitty properties. One kitty should take the entire screen, minus the gaps.
    NLog::log("{}Check kitty dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "size: 1876,1036", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    // group the kitty
    NLog::log("{}Enable group and groupbar", Colors::YELLOW);
    OK(getFromSocket("/dispatch togglegroup"));
    OK(getFromSocket("/keyword group:groupbar:enabled 1"));

    // check the height of the window now
    NLog::log("{}Recheck kitty dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 22,43");
        EXPECT_CONTAINS(str, "size: 1876,1015");
    }

    // disable the groupbar for ease of testing for now
    NLog::log("{}Disable groupbar", Colors::YELLOW);
    OK(getFromSocket("r/keyword group:groupbar:enabled 0"));

    // kill all
    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Spawn kitty again", Colors::YELLOW);
    kittyProcA = Tests::spawnKitty();
    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Group kitty", Colors::YELLOW);
    OK(getFromSocket("/dispatch togglegroup"));

    // check the height of the window now
    NLog::log("{}Check kitty dimensions 2", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
    }

    NLog::log("{}Spawn kittyProcB", Colors::YELLOW);
    auto kittyProcB = Tests::spawnKitty();
    if (!kittyProcB) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);

    size_t lastActiveKittyIdx = 0;

    NLog::log("{}Get last active kitty id", Colors::YELLOW);
    try {
        auto str           = getFromSocket("/activewindow");
        lastActiveKittyIdx = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    // test cycling through

    NLog::log("{}Test cycling through grouped windows", Colors::YELLOW);
    OK(getFromSocket("/dispatch changegroupactive f"));

    try {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx != std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16), true);
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    getFromSocket("/dispatch changegroupactive f");

    try {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx, std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16));
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    // test movegroupwindow: focus should follow the moved window
    NLog::log("{}Test movegroupwindow focus follows window", Colors::YELLOW);
    try {
        auto str              = getFromSocket("/activewindow");
        auto activeBeforeMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        OK(getFromSocket("/dispatch movegroupwindow f"));
        str                  = getFromSocket("/activewindow");
        auto activeAfterMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        EXPECT(activeAfterMove, activeBeforeMove);
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    // and backwards
    NLog::log("{}Test movegroupwindow backwards", Colors::YELLOW);
    try {
        auto str              = getFromSocket("/activewindow");
        auto activeBeforeMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        OK(getFromSocket("/dispatch movegroupwindow b"));
        str                  = getFromSocket("/activewindow");
        auto activeAfterMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        EXPECT(activeAfterMove, activeBeforeMove);
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    NLog::log("{}Disable autogrouping", Colors::YELLOW);
    OK(getFromSocket("/keyword group:auto_group false"));

    NLog::log("{}Spawn kittyProcC", Colors::YELLOW);
    auto kittyProcC = Tests::spawnKitty();
    if (!kittyProcC) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 3 windows 2", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 3);
    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 2);
    }

    OK(getFromSocket("/dispatch movefocus l"));
    OK(getFromSocket("/dispatch changegroupactive 1"));
    OK(getFromSocket("/keyword group:auto_group true"));
    OK(getFromSocket("/keyword group:insert_after_current false"));

    NLog::log("{}Spawn kittyProcD", Colors::YELLOW);
    auto kittyProcD = Tests::spawnKitty();
    if (!kittyProcD) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 4 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 4);

    OK(getFromSocket("/dispatch changegroupactive 3"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, std::format("pid: {}", kittyProcD->pid()));
    }

    // kill all
    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    // test movewindoworgroup: direction should be respected when extracting from group
    NLog::log("{}Test movewindoworgroup respects direction out of group", Colors::YELLOW);
    OK(getFromSocket("/keyword group:groupbar:enabled 0"));
    {
        auto kittyE = Tests::spawnKitty();
        if (!kittyE) {
            NLog::log("{}Error: kitty did not spawn", Colors::RED);
            return false;
        }

        // group kitty, and new windows should be auto-grouped
        OK(getFromSocket("/dispatch togglegroup"));

        auto kittyF = Tests::spawnKitty();
        if (!kittyF) {
            NLog::log("{}Error: kitty did not spawn", Colors::RED);
            return false;
        }
        EXPECT(Tests::windowCount(), 2);

        // both windows should be grouped at the same position
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 2);
        }

        // move active window out of group to the right
        NLog::log("{}Test movewindoworgroup r", Colors::YELLOW);
        OK(getFromSocket("/dispatch movewindoworgroup r"));

        // the group should stay at x=22, the extracted window should be to the right
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        }

        // move it back into the group
        OK(getFromSocket("/dispatch moveintogroup l"));

        // move active window out of group downward
        NLog::log("{}Test movewindoworgroup d", Colors::YELLOW);
        OK(getFromSocket("/dispatch movewindoworgroup d"));

        // the group should stay at y=22, the extracted window should be below
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        }

        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);
    }

    // test that we deny a floated window getting auto-grouped into a tiled group.
    NLog::log("{}Test that we deny a floated window getting auto-grouped into a tiled group.", Colors::GREEN);

    OK(getFromSocket("/keyword windowrule[kitty-tiled]:match:class kitty_tiled"));
    OK(getFromSocket("/keyword windowrule[kitty-tiled]:tile yes"));
    auto kittyProcE = Tests::spawnKitty("kitty_tiled");
    if (!kittyProcE) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }
    OK(getFromSocket("/dispatch togglegroup"));

    OK(getFromSocket("/keyword windowrule[kitty-floated]:match:class kitty_floated"));
    OK(getFromSocket("/keyword windowrule[kitty-floated]:float yes"));
    auto kittyProcF = Tests::spawnKitty("kitty_floated");
    if (!kittyProcF) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    EXPECT(Tests::windowCount(), 2);

    {
        auto clients  = getFromSocket("/clients");
        auto classPos = clients.find("class: kitty_floated");
        if (classPos == std::string::npos) {
            NLog::log("{}Could not find kitty_floated in clients output", Colors::RED);
            ret = 1;
        } else {
            auto entryStart  = clients.rfind("Window ", classPos);
            auto entryEnd    = clients.find("\n\n", classPos);
            auto windowEntry = clients.substr(entryStart, entryEnd - entryStart);
            EXPECT_CONTAINS(windowEntry, "floating: 1");
            EXPECT_CONTAINS(windowEntry, "grouped: 0");
        }
    }

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // Tests for grouping/merging logic
    NLog::log("{}Testing locked groups w/ invade", Colors::GREEN);

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // Test normal, unlocked groups
    {
        auto winA = Tests::spawnKitty("unlocked");
        if (!winA) {
            NLog::log("{}Error: unlocked kitty did not spawn", Colors::RED);
            return false;
        }
        OK(getFromSocket("/dispatch togglegroup"));

        auto winB = Tests::spawnKitty("top");
        if (!winB) {
            NLog::log("{}Error: top kitty did not spawn", Colors::RED);
            return false;
        }

        // Verify it DID merge into a group
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 2);
        }
    }

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // Test locked groups
    {
        auto lockedWin = Tests::spawnKitty("locked");
        if (!lockedWin) {
            NLog::log("{}Error: locked kitty did not spawn", Colors::RED);
            return false;
        }
        OK(getFromSocket("/dispatch togglegroup"));
        OK(getFromSocket(std::format("/dispatch focuswindow pid:{}", lockedWin->pid())));
        OK(getFromSocket("/dispatch lockactivegroup lock"));

        auto winB = Tests::spawnKitty("top");
        if (!winB) {
            NLog::log("{}Error: top kitty did not spawn", Colors::RED);
            return false;
        }

        // Verify it did NOT merge into the locked group
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        }
    }

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // Test locked groups WITH invade rule
    {
        OK(getFromSocket("/keyword windowrule[locked-im]:match:class ^locked|invade$"));
        OK(getFromSocket("/keyword windowrule[locked-im]:group set always lock invade"));

        auto lockedWin = Tests::spawnKitty("locked");
        if (!lockedWin) {
            NLog::log("{}Error: locked kitty did not spawn", Colors::RED);
            return false;
        }

        auto invadingWin = Tests::spawnKitty("invade");
        if (!invadingWin) {
            NLog::log("{}Error: invading kitty did not spawn", Colors::RED);
            return false;
        }

        // Verify it DID merge into the locked group
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 2);
    }

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
