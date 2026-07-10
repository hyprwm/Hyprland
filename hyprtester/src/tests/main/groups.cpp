#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include "../shared.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Utils;

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

    // test that we can auto-group a new floated window into the focused floated group
    NLog::log("{}Test that we can auto-group a new floated window into the focused floated group.", Colors::GREEN);

    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-floated-A', match = { class = 'kitty_floated_A' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-floated-A', float = true })"));
    auto kittyFA = Tests::spawnKitty("kitty_floated_A");
    if (!kittyFA) {
        FAIL_TEST("Could not spawn kitty");
    }
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-floated-B', match = { class = 'kitty_floated_B' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-floated-B', float = true })"));
    auto kittyFB = Tests::spawnKitty("kitty_floated_B");
    if (!kittyFB) {
        FAIL_TEST("Could not spawn kitty");
    }
    ASSERT(Tests::windowCount(), 2);

    {
        auto clients  = getFromSocket("/clients");
        auto classPos = clients.find("class: kitty_floated_B");
        if (classPos == std::string::npos) {
            FAIL_TEST("Could not find kitty_floated_B in clients output");
        } else {
            auto entryStart  = clients.rfind("Window ", classPos);
            auto entryEnd    = clients.find("\n\n", classPos);
            auto windowEntry = clients.substr(entryStart, entryEnd - entryStart);
            EXPECT_CONTAINS(windowEntry, "floating: 1");
            EXPECT_NOT_CONTAINS(windowEntry, "grouped: 0");
        }
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

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

TEST_CASE(groupsNoCrash) {
    auto kittyA = Tests::spawnKitty("kittyA");

    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    {
        auto curr = getFromSocket("/activewindow");
        EXPECT_CONTAINS(curr, "kittyA");
    }
}

TEST_CASE(groupsLuaApi) {
    NLog::log("{}Dispatching workspace `groups-lua-api`", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-api' })"));
    OK(getFromSocket("/eval hl.config({ group = { auto_group = false, insert_after_current = false, groupbar = { enabled = false } } })"));

    auto kittyA = Tests::spawnKitty("luaGroupA");
    auto kittyB = Tests::spawnKitty("luaGroupB");
    auto kittyC = Tests::spawnKitty("luaGroupC");
    auto kittyD = Tests::spawnKitty("luaGroupD");
    auto kittyE = Tests::spawnKitty("luaGroupE");

    if (!kittyA || !kittyB || !kittyC || !kittyD || !kittyE)
        FAIL_TEST("Could not spawn kitty");

    ASSERT(Tests::windowCount(), 5);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:luaGroupA' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    NLog::log("{}Testing Lua group add/remove by object and index", Colors::YELLOW);
    OK(getFromSocket("/eval local a=hl.get_window('class:luaGroupA') local b=hl.get_window('class:luaGroupB') local c=hl.get_window('class:luaGroupC') assert(a and b and c and "
                     "a.group) local g=a.group g:add(b) assert(b.group == g) assert(g.size == 2) g:add(c, 1) assert(c.group == g) assert(g.members[1] == c) assert(g.size == 3) "
                     "g:remove(1) assert(c.group == nil) assert(g.size == 2) g:remove(b) assert(b.group == nil) assert(g.size == 1)"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:luaGroupD' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    NLog::log("{}Testing Lua group merge with insertion index", Colors::YELLOW);
    OK(getFromSocket("/eval local a=hl.get_window('class:luaGroupA') local d=hl.get_window('class:luaGroupD') local e=hl.get_window('class:luaGroupE') assert(a and d and e and "
                     "a.group and d.group) local g=a.group local source=d.group source:add(e) assert(source.size == 2) g:add(d, 1) assert(d.group == g) assert(e.group == g) "
                     "assert(g.size == 3) assert(g.members[1] == d) assert(g.members[2] == e) assert(g.members[3] == a)"));

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}

TEST_CASE(groupsLuaApiInvalidOps) {
    NLog::log("{}Dispatching workspace `groups-lua-invalid`", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-invalid' })"));
    OK(getFromSocket("/eval hl.config({ group = { auto_group = false, insert_after_current = false, groupbar = { enabled = false } } })"));

    auto kittyA = Tests::spawnKitty("luaInvalidA");
    auto kittyB = Tests::spawnKitty("luaInvalidB");
    auto kittyC = Tests::spawnKitty("luaInvalidC");

    if (!kittyA || !kittyB || !kittyC)
        FAIL_TEST("Could not spawn kitty");

    ASSERT(Tests::windowCount(), 3);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:luaInvalidA' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    OK(getFromSocket(
        "/eval local a=hl.get_window('class:luaInvalidA') local b=hl.get_window('class:luaInvalidB') local c=hl.get_window('class:luaInvalidC') local g=a.group g:add(b) "
        "assert(g.size == 2 and b.group == g and c.group == nil)"));

    NOK(getFromSocket("/eval local a=hl.get_window('class:luaInvalidA') local c=hl.get_window('class:luaInvalidC') a.group:add(c, 0)"));
    NOK(getFromSocket("/eval local a=hl.get_window('class:luaInvalidA') local c=hl.get_window('class:luaInvalidC') a.group:add(c, 4)"));
    NOK(getFromSocket("/eval local a=hl.get_window('class:luaInvalidA') a.group:remove(0)"));
    NOK(getFromSocket("/eval local a=hl.get_window('class:luaInvalidA') a.group:remove(3)"));
    NOK(getFromSocket("/eval local a=hl.get_window('class:luaInvalidA') local c=hl.get_window('class:luaInvalidC') a.group:remove(c)"));

    OK(getFromSocket("/eval local a=hl.get_window('class:luaInvalidA') local b=hl.get_window('class:luaInvalidB') local c=hl.get_window('class:luaInvalidC') local g=a.group "
                     "assert(g.size == 2 and b.group == g and c.group == nil) local first, second = g.members[1], g.members[2] g:add(b, 1) "
                     "assert(g.size == 2 and g.members[1] == first and g.members[2] == second)"));

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}

TEST_CASE(groupsLuaApiCrossWorkspaceMonitor) {
    NLog::log("{}Testing Lua group add across workspaces and monitors", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ group = { auto_group = false, insert_after_current = false, groupbar = { enabled = false } } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-ws-a' })"));
    auto kittyWorkspaceA = Tests::spawnKitty("luaWorkspaceA");
    if (!kittyWorkspaceA)
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-ws-b' })"));
    auto kittyWorkspaceB = Tests::spawnKitty("luaWorkspaceB");
    if (!kittyWorkspaceB)
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/eval local a=hl.get_window('class:luaWorkspaceA') local b=hl.get_window('class:luaWorkspaceB') assert(a and b and a.group) local g=a.group g:add(b) "
                     "assert(b.group == g) assert(b.workspace == a.workspace) assert(b.monitor == a.monitor) assert(g.size == 2)"));

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    static const std::string TEST_OUTPUT = "HEADLESS-LUA-GROUP";
    getFromSocket(std::format("/output remove {}", TEST_OUTPUT));
    OK(getFromSocket(std::format("/output create headless {}", TEST_OUTPUT)));

    CScopeGuard guard = {[&]() { OK(getFromSocket(std::format("/output remove {}", TEST_OUTPUT))); }};

    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', disabled = false, mode = '1920x1080@60', position = '1920x0', scale = '1' }})", TEST_OUTPUT)));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-monitor-a' })"));

    auto kittyMonitorA = Tests::spawnKitty("luaMonitorA");
    if (!kittyMonitorA)
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ monitor = '{}' }})", TEST_OUTPUT)));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-monitor-b' })"));

    auto kittyMonitorB = Tests::spawnKitty("luaMonitorB");
    if (!kittyMonitorB)
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/eval local a=hl.get_window('class:luaMonitorA') local b=hl.get_window('class:luaMonitorB') assert(a and b and a.group) local g=a.group g:add(b) "
                     "assert(b.group == g) assert(b.workspace == a.workspace) assert(b.monitor == a.monitor) assert(g.size == 2)"));

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}

TEST_CASE(groupsLuaApiFullscreen) {
    NLog::log("{}Dispatching workspace `groups-lua-fullscreen`", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-fullscreen' })"));
    OK(getFromSocket("/eval hl.config({ group = { auto_group = false, insert_after_current = false, groupbar = { enabled = false } } })"));

    auto kittyA = Tests::spawnKitty("luaFullscreenGroupA");
    auto kittyB = Tests::spawnKitty("luaFullscreenSourceB");
    if (!kittyA || !kittyB)
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:luaFullscreenGroupA' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:luaFullscreenSourceB' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    OK(getFromSocket(
        "/eval local a=hl.get_window('class:luaFullscreenGroupA') local b=hl.get_window('class:luaFullscreenSourceB') assert(a and b and a.group and b.fullscreen == 2) "
        "local g=a.group g:add(b) assert(b.group == g) assert(b.fullscreen == 0) assert(a.fullscreen == 0) assert(g.size == 2)"));

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:groups-lua-fullscreen-target' })"));
    auto kittyC = Tests::spawnKitty("luaFullscreenTargetC");
    auto kittyD = Tests::spawnKitty("luaFullscreenUnderD");
    if (!kittyC || !kittyD)
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:luaFullscreenTargetC' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    OK(getFromSocket("/eval local c=hl.get_window('class:luaFullscreenTargetC') local d=hl.get_window('class:luaFullscreenUnderD') assert(c and d and c.group and c.fullscreen == "
                     "2 and d.fullscreen == 0) "
                     "local g=c.group g:add(d) assert(d.group == g) assert(d.fullscreen == 2) assert(c.fullscreen == 0) assert(g.size == 2)"));

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}
