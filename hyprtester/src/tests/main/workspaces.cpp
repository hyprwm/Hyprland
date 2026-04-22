#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <print>
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <csignal>
#include <cerrno>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Utils;

#define UP CUniquePointer
#define SP CSharedPointer

static bool testSpecialWorkspaceFullscreen() {
    NLog::log("{}Testing special workspace fullscreen detection", Colors::YELLOW);

    CScopeGuard guard = {[&]() {
        NLog::log("{}Cleaning up special workspace fullscreen test", Colors::YELLOW);
        // Close special workspace if open
        auto monitors = getFromSocket("/monitors");
        if (monitors.contains("(special:test_fs_special)") && !monitors.contains("special workspace: 0 ()"))
            getFromSocket("/dispatch hl.dsp.workspace.toggle_special('test_fs_special')");
        Tests::killAllWindows();
        OK(getFromSocket("/reload"));
    }};

    getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })");
    EXPECT(Tests::windowCount(), 0);

    NLog::log("{}Test 1: Fullscreen detection on special workspace", Colors::YELLOW);

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'special:test_fs_special' })"));

    if (!Tests::spawnKitty("kitty_special"))
        return false;

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_special");
        EXPECT_CONTAINS(str, "(special:test_fs_special)");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "(special:test_fs_special)");
    }

    NLog::log("{}Test 2: Special workspace fullscreen precedence", Colors::YELLOW);

    // Close special workspace before spawning on regular workspace
    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('test_fs_special')"));
    getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })");

    if (!Tests::spawnKitty("kitty_regular"))
        return false;

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_regular");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('test_fs_special')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_special' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_special");
    }

    NLog::log("{}Test 3: Toggle special workspace hides it", Colors::YELLOW);

    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('test_fs_special')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_regular' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_regular");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "special workspace: 0 ()");
    }

    return true;
}

static bool testAsymmetricGaps() {
    NLog::log("{}Testing asymmetric gap splits", Colors::YELLOW);
    {

        CScopeGuard guard = {[&]() {
            NLog::log("{}Cleaning up asymmetric gap test", Colors::YELLOW);
            Tests::killAllWindows();
            OK(getFromSocket("/reload"));
        }};

        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:gap_split_test' })"));
        OK(getFromSocket("r/eval hl.config({ general = { gaps_in = 0 } })"));
        OK(getFromSocket("r/eval hl.config({ general = { border_size = 0 } })"));
        OK(getFromSocket("r/eval hl.config({ dwindle = { split_width_multiplier = 1.0 } })"));
        OK(getFromSocket("r/eval hl.workspace_rule({ workspace = 'name:gap_split_test', gaps_out = { top = 0, right = 1000, bottom = 0, left = 0 } })"));

        NLog::log("{}Testing default split (force_split = 0)", Colors::YELLOW);
        OK(getFromSocket("r/eval hl.config({ dwindle = { force_split = 0 } })"));

        if (!Tests::spawnKitty("gaps_kitty_A") || !Tests::spawnKitty("gaps_kitty_B"))
            return false;

        NLog::log("{}Expecting vertical split (B below A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_A' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_B' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,540");

        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);

        NLog::log("{}Testing force_split = 1", Colors::YELLOW);
        OK(getFromSocket("r/eval hl.config({ dwindle = { force_split = 1 } })"));

        if (!Tests::spawnKitty("gaps_kitty_A") || !Tests::spawnKitty("gaps_kitty_B"))
            return false;

        NLog::log("{}Expecting vertical split (B above A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_B' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_A' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,540");

        NLog::log("{}Expecting horizontal split (C left of B)", Colors::YELLOW);
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_B' })"));

        if (!Tests::spawnKitty("gaps_kitty_C"))
            return false;

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_C' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_B' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 460,0");

        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);

        NLog::log("{}Testing force_split = 2", Colors::YELLOW);
        OK(getFromSocket("r/eval hl.config({ dwindle = { force_split = 2 } })"));

        if (!Tests::spawnKitty("gaps_kitty_A") || !Tests::spawnKitty("gaps_kitty_B"))
            return false;

        NLog::log("{}Expecting vertical split (B below A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_A' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_B' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,540");

        NLog::log("{}Expecting horizontal split (C right of A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_A' })"));

        if (!Tests::spawnKitty("gaps_kitty_C"))
            return false;

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_A' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:gaps_kitty_C' })"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 460,0");
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    return true;
}

static void testWorkspaceHistoryMultiMon() {
    NLog::log("{}Testing multimon workspace history tracker", Colors::YELLOW);

    // Initial state:
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '10' })"));
    Tests::spawnKitty();
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '11' })"));
    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '12' })"));
    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 11");
    }
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'previous_per_monitor' })"));
    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 10");
    }

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static void testMultimonBAF() {
    NLog::log("{}Testing multimon back and forth", Colors::YELLOW);

    OK(getFromSocket("/eval hl.config({ binds = { workspace_back_and_forth = 1 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));

    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));

    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 2 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '4' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '4' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 2 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 4 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 4 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 3 ");
    }

    Tests::killAllWindows();
}

static void testMultimonFocus() {
    NLog::log("{}Testing multimon focus and move", Colors::YELLOW);

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 0 } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '8' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '7' })"));

    for (auto const& win : {"a", "b"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with win class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 7 ");
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 8 ");
    }

    Tests::spawnKitty("c");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 8 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 7 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 8 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: c");
    }

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 8 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"));

    OK(getFromSocket("/eval hl.config({ general = { no_focus_fallback = true } })"));
    OK(getFromSocket("/eval hl.config({ binds = { window_direction_monitor_fallback = false } })"));

    EXPECT_NOT(getFromSocket("/dispatch hl.dsp.focus({ direction = 'left' })"), "ok");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 8 ");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'left' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: b");
    }

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_CONTAINS(str, "workspace ID 8 ");
    }

    OK(getFromSocket("/reload"));

    Tests::killAllWindows();
}

static void testDynamicWsEffects() {
    // test dynamic workspace effects, they shouldn't lag

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '69' })"));

    Tests::spawnKitty("bitch");

    OK(getFromSocket("r/eval hl.workspace_rule({ workspace = '69', border_size = 20 })"));
    OK(getFromSocket("r/eval hl.workspace_rule({ workspace = '69', no_rounding = true })"));

    EXPECT(getFromSocket("/getprop class:bitch border_size"), "20");
    EXPECT(getFromSocket("/getprop class:bitch rounding"), "0");

    OK(getFromSocket("/reload"));

    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing workspaces", Colors::GREEN);

    EXPECT(Tests::windowCount(), 0);

    // test on workspace "window"
    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })");

    NLog::log("{}Checking persistent no-mon", Colors::YELLOW);
    OK(getFromSocket("r/eval hl.workspace_rule({ workspace = '966', persistent = true })"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_CONTAINS(str, "workspace ID 966 (966)");
    }

    OK(getFromSocket("/reload"));

    NLog::log("{}Spawning kittyProc on ws 1", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty();

    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Switching to workspace 3", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));

    NLog::log("{}Spawning kittyProc on ws 3", Colors::YELLOW);
    auto kittyProcB = Tests::spawnKitty();

    if (!kittyProcB) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    NLog::log("{}Switching to workspace +1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '+1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 2 (2)");
    }

    // check if the other workspaces are alive
    {
        auto str = getFromSocket("/workspaces");
        EXPECT_CONTAINS(str, "workspace ID 3 (3)");
        EXPECT_CONTAINS(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace m+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'm+1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace -1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '-1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    NLog::log("{}Switching to workspace r+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'r+1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace r+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'r+1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace r~1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'r~1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace empty", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'empty' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace previous", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'previous' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace name:TEST_WORKSPACE_NULL", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:TEST_WORKSPACE_NULL' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID -1337 (TEST_WORKSPACE_NULL)");
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    // add a new monitor
    NLog::log("{}Adding a new monitor", Colors::YELLOW);
    EXPECT(getFromSocket("/output create headless"), "ok")

    // should take workspace 2
    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "active workspace: 2 (2)");
        EXPECT_CONTAINS(str, "active workspace: 1 (1)");
        EXPECT_CONTAINS(str, "HEADLESS-3");
    }

    // focus the first monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace r+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'r+1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace r~2", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'r~2' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace m+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'm+1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    // no OK: this will throw an error as it should
    getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })");

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Testing back_and_forth", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ binds = { workspace_back_and_forth = true } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    OK(getFromSocket("/eval hl.config({ binds = { workspace_back_and_forth = false } })"));

    NLog::log("{}Testing hide_special_on_workspace_change", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ binds = { hide_special_on_workspace_change = true } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'special:HELLO' })"));

    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "special workspace: -");
        EXPECT_CONTAINS(str, "special:HELLO");
    }

    // no OK: will err (it shouldn't prolly but oh well)
    getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })");

    {
        auto str = getFromSocket("/monitors");
        EXPECT_COUNT_STRING(str, "special workspace: 0 ()", 2);
    }

    OK(getFromSocket("/eval hl.config({ binds = { hide_special_on_workspace_change = false } })"));

    NLog::log("{}Testing allow_workspace_cycles", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ binds = { allow_workspace_cycles = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'previous' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'previous' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'previous' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    OK(getFromSocket("/eval hl.config({ binds = { allow_workspace_cycles = false } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    // spawn 3 kitties
    NLog::log("{}Testing focus_preferred_method", Colors::YELLOW);
    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 2 } })"));
    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");
    Tests::spawnKitty("kitty_C");
    OK(getFromSocket("/eval hl.config({ dwindle = { force_split = 0 } })"));

    // focus kitty 2: will be top right (dwindle)
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));

    // resize it to be a bit taller
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 20, y = 20, relative = true })"));

    // now we test focus methods.
    OK(getFromSocket("/eval hl.config({ binds = { focus_preferred_method = 0 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_C' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_C");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));

    OK(getFromSocket("/eval hl.config({ binds = { focus_preferred_method = 1 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_B");
    }

    NLog::log("{}Testing movefocus_cycles_fullscreen", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-3' })"));
    Tests::spawnKitty("kitty_D");
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_D");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'l' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_A");
    }

    OK(getFromSocket("/eval hl.config({ binds = { movefocus_cycles_fullscreen = false } })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_D");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'l' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_A");
    }

    OK(getFromSocket("/eval hl.config({ binds = { movefocus_cycles_fullscreen = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_B");
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    testMultimonBAF();
    testMultimonFocus();
    testWorkspaceHistoryMultiMon();

    // destroy the headless output
    OK(getFromSocket("/output remove HEADLESS-3"));

    testSpecialWorkspaceFullscreen();
    testAsymmetricGaps();
    testDynamicWsEffects();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
