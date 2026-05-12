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

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

// TODO: decompose this into multiple test cases
TEST_CASE(groups) {
    // test on workspace "window"
    NLog::log("{}Dispatching workspace `groups`", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups' })");

    NLog::log("{}Testing movewindoworgroup from group to group", Colors::YELLOW);
    auto kittyA = Tests::spawnKitty("kittyA");
    if (!kittyA) {
        FAIL_TEST("Could not spawn kitty");
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
        FAIL_TEST("Could not spawn kitty");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kittyB' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kittyA' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    NLog::log("{}Check kittyB dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_COUNT_STRING(str, "size: 931,1015", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    auto kittyC = Tests::spawnKitty("kittyC");
    if (!kittyC) {
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Check kittyC dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_COUNT_STRING(str, "size: 931,1015", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right', group_aware = true })"));
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
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 1);

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
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/eval hl.config({ group = { groupbar = { enabled = 1 } } })"));

    // check the height of the window now
    NLog::log("{}Recheck kitty dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 22,43");
        EXPECT_CONTAINS(str, "size: 1876,1015");
    }

    // disable the groupbar for ease of testing for now
    NLog::log("{}Disable groupbar", Colors::YELLOW);
    OK(getFromSocket("r/eval hl.config({ group = { groupbar = { enabled = 0 } } })"));

    // kill all
    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Spawn kitty again", Colors::YELLOW);
    kittyProcA = Tests::spawnKitty();
    if (!kittyProcA) {
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Group kitty", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

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
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 2);

    size_t lastActiveKittyIdx = 0;

    NLog::log("{}Get last active kitty id", Colors::YELLOW);
    try {
        auto str           = getFromSocket("/activewindow");
        lastActiveKittyIdx = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
    } catch (...) { FAIL_TEST("Could not extract the active window id"); }

    // test cycling through

    NLog::log("{}Test cycling through grouped windows", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.group.next()"));

    try {
        auto str = getFromSocket("/activewindow");
        ASSERT(lastActiveKittyIdx != std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16), true);
    } catch (...) { FAIL_TEST("Could not extract the active window id"); }

    getFromSocket("/dispatch hl.dsp.group.next()");

    try {
        auto str = getFromSocket("/activewindow");
        ASSERT(lastActiveKittyIdx, std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16));
    } catch (...) { FAIL_TEST("Could not extract the active window id"); }

    // test movegroupwindow: focus should follow the moved window
    NLog::log("{}Test movegroupwindow focus follows window", Colors::YELLOW);
    try {
        auto str              = getFromSocket("/activewindow");
        auto activeBeforeMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        OK(getFromSocket("/dispatch hl.dsp.group.move_window({ forward = true })"));
        str                  = getFromSocket("/activewindow");
        auto activeAfterMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        EXPECT(activeAfterMove, activeBeforeMove);
    } catch (...) { FAIL_TEST("Could not extract the active window id"); }

    // and backwards
    NLog::log("{}Test movegroupwindow backwards", Colors::YELLOW);
    try {
        auto str              = getFromSocket("/activewindow");
        auto activeBeforeMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        OK(getFromSocket("/dispatch hl.dsp.group.move_window({ forward = false })"));
        str                  = getFromSocket("/activewindow");
        auto activeAfterMove = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
        EXPECT(activeAfterMove, activeBeforeMove);
    } catch (...) { FAIL_TEST("Could not extract the active window id"); }

    NLog::log("{}Disable autogrouping", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ group = { auto_group = false } })"));
    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));

    NLog::log("{}Spawn kittyProcC", Colors::YELLOW);
    auto kittyProcC = Tests::spawnKitty();
    if (!kittyProcC) {
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Expecting 3 windows 2", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 3);
    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 2);
    }

    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 0 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.active({ index = 1 })"));
    OK(getFromSocket("/eval hl.config({ group = { auto_group = true } })"));
    OK(getFromSocket("/eval hl.config({ group = { insert_after_current = false } })"));

    NLog::log("{}Spawn kittyProcD", Colors::YELLOW);
    auto kittyProcD = Tests::spawnKitty();
    if (!kittyProcD) {
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Expecting 4 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 4);

    OK(getFromSocket("/dispatch hl.dsp.group.active({ index = 3 })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, std::format("pid: {}", kittyProcD->pid()));
    }

    // kill all
    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 0);

    // test movewindoworgroup: direction should be respected when extracting from group
    NLog::log("{}Test movewindoworgroup respects direction out of group", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ group = { groupbar = { enabled = 0 } } })"));
    {
        auto kittyE = Tests::spawnKitty();
        if (!kittyE) {
            FAIL_TEST("Could not spawn kitty");
        }

        // group kitty, and new windows should be auto-grouped
        OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

        auto kittyF = Tests::spawnKitty();
        if (!kittyF) {
            FAIL_TEST("Could not spawn kitty");
        }
        ASSERT(Tests::windowCount(), 2);

        // both windows should be grouped at the same position
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 2);
        }

        // move active window out of group to the right
        NLog::log("{}Test movewindoworgroup r", Colors::YELLOW);
        OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right', group_aware = true })"));

        // the group should stay at x=22, the extracted window should be to the right
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        }

        // move it back into the group
        OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

        // move active window out of group downward
        NLog::log("{}Test movewindoworgroup d", Colors::YELLOW);
        OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'down', group_aware = true })"));

        // the group should stay at y=22, the extracted window should be below
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        }

        Tests::killAllWindows();
        ASSERT(Tests::windowCount(), 0);
    }

    // test that we deny a floated window getting auto-grouped into a tiled group.
    NLog::log("{}Test that we deny a floated window getting auto-grouped into a tiled group.", Colors::GREEN);

    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-tiled', match = { class = 'kitty_tiled' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-tiled', tile = true })"));
    auto kittyProcE = Tests::spawnKitty("kitty_tiled");
    if (!kittyProcE) {
        FAIL_TEST("Could not spawn kitty");
    }
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-floated', match = { class = 'kitty_floated' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-floated', float = true })"));
    auto kittyProcF = Tests::spawnKitty("kitty_floated");
    if (!kittyProcF) {
        FAIL_TEST("Could not spawn kitty");
    }

    ASSERT(Tests::windowCount(), 2);

    {
        auto clients  = getFromSocket("/clients");
        auto classPos = clients.find("class: kitty_floated");
        if (classPos == std::string::npos) {
            FAIL_TEST("Could not find kitty_floated in clients output");
        } else {
            auto entryStart  = clients.rfind("Window ", classPos);
            auto entryEnd    = clients.find("\n\n", classPos);
            auto windowEntry = clients.substr(entryStart, entryEnd - entryStart);
            EXPECT_CONTAINS(windowEntry, "floating: 1");
            EXPECT_CONTAINS(windowEntry, "grouped: 0");
        }
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    // Tests for grouping/merging logic
    NLog::log("{}Testing locked groups w/ invade", Colors::GREEN);

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    // Test normal, unlocked groups
    {
        auto winA = Tests::spawnKitty("unlocked");
        if (!winA) {
            FAIL_TEST("Could not spawn unlocked kitty");
        }
        OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

        auto winB = Tests::spawnKitty("top");
        if (!winB) {
            FAIL_TEST("Could not spawn top kitty");
        }

        // Verify it DID merge into a group
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 2);
        }
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    // Test locked groups
    {
        auto lockedWin = Tests::spawnKitty("locked");
        if (!lockedWin) {
            FAIL_TEST("Could not spawn locked kitty");
        }
        OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
        OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", lockedWin->pid())));
        OK(getFromSocket("/dispatch hl.dsp.group.lock_active({ action = 'set' })"));

        auto winB = Tests::spawnKitty("top");
        if (!winB) {
            FAIL_TEST("Could not spawn top kitty");
        }

        // Verify it did NOT merge into the locked group
        {
            auto str = getFromSocket("/clients");
            EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        }
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    // Test locked groups WITH invade rule
    {
        OK(getFromSocket("/eval hl.window_rule({ name = 'locked-im', match = { class = '^locked|invade$' } })"));
        OK(getFromSocket("/eval hl.window_rule({ name = 'locked-im', group = 'set always lock invade' })"));

        auto lockedWin = Tests::spawnKitty("locked");
        if (!lockedWin) {
            FAIL_TEST("Could not spawn locked kitty");
        }

        auto invadingWin = Tests::spawnKitty("invade");
        if (!invadingWin) {
            FAIL_TEST("Could not spawn invading kitty");
        }

        // Verify it DID merge into the locked group
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 2);
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    // Test groupbar middle click close config
    {
        OK(getFromSocket("/eval hl.config({ group = { auto_group = true, groupbar = { enabled = true, middle_click_close = false } } })"));

        auto kittyA = Tests::spawnKitty("kittyA");
        if (!kittyA) {
            FAIL_TEST("Could not spawn kitty");
        }

        OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

        auto kittyB = Tests::spawnKitty("kittyB");
        if (!kittyB) {
            FAIL_TEST("Could not spawn kitty");
        }

        EXPECT(Tests::windowCount(), 2);

        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 80, y = 32 })"));
        OK(getFromSocket("/eval hl.plugin.test.click(274, 1)"));
        OK(getFromSocket("/eval hl.plugin.test.click(274, 0)"));

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT(Tests::windowCount(), 2);

        OK(getFromSocket("/eval hl.config({ group = { groupbar = { middle_click_close = true } } })"));
        OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 80, y = 32 })"));
        OK(getFromSocket("/eval hl.plugin.test.click(274, 1)"));
        OK(getFromSocket("/eval hl.plugin.test.click(274, 0)"));

        Tests::waitUntilWindowsN(1);
        EXPECT(Tests::windowCount(), 1);

        OK(getFromSocket("/eval hl.config({ group = { groupbar = { enabled = 0 } } })"));
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}
