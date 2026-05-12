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

// Uncomment once test vm can run hyprland-dialog
// static void testAnrDialogs() {
//     NLog::log("{}Testing ANR dialogs", Colors::YELLOW);
//
//     OK(getFromSocket("/eval hl.config({ misc = { enable_anr_dialog = true } })"));
//     OK(getFromSocket("/eval hl.config({ misc = { anr_missed_pings = 1 } })"));
//
//     NLog::log("{}ANR dialog: regular workspaces", Colors::YELLOW);
//     {
//         OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
//
//         auto kitty = Tests::spawnKitty("bad_kitty");
//
//         if (!kitty) {
//             ret = 1;
//             return;
//         }
//
//         {
//             auto str = getFromSocket("/activewindow");
//             ASSERT_CONTAINS(str, "workspace: 2");
//         }
//
//         OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
//
//         ::kill(kitty->pid(), SIGSTOP);
//         Tests::waitUntilWindowsN(2);
//
//         {
//             auto str = getFromSocket("/activeworkspace");
//             ASSERT_CONTAINS(str, "windows: 0");
//         }
//
//         {
//             OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:hyprland-dialog' })"))
//             auto str = getFromSocket("/activewindow");
//             ASSERT_CONTAINS(str, "workspace: 2");
//         }
//     }
//
//     Tests::killAllWindows();
//
//     NLog::log("{}ANR dialog: named workspaces", Colors::YELLOW);
//     {
//         OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:yummy' })"));
//
//         auto kitty = Tests::spawnKitty("bad_kitty");
//
//         if (!kitty) {
//             ret = 1;
//             return;
//         }
//
//         {
//             auto str = getFromSocket("/activewindow");
//             ASSERT_CONTAINS(str, "yummy");
//         }
//
//         OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
//
//         ::kill(kitty->pid(), SIGSTOP);
//         Tests::waitUntilWindowsN(2);
//
//         {
//             auto str = getFromSocket("/activeworkspace");
//             ASSERT_CONTAINS(str, "windows: 0");
//         }
//
//         {
//             OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:hyprland-dialog' })"))
//             auto str = getFromSocket("/activewindow");
//             ASSERT_CONTAINS(str, "yummy");
//         }
//     }
//
//     Tests::killAllWindows();
//
//     NLog::log("{}ANR dialog: special workspaces", Colors::YELLOW);
//     {
//         OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'special:apple' })"));
//
//         auto kitty = Tests::spawnKitty("bad_kitty");
//
//         if (!kitty) {
//             ret = 1;
//             return;
//         }
//
//         {
//             auto str = getFromSocket("/activewindow");
//             ASSERT_CONTAINS(str, "special:apple");
//         }
//
//         OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('apple')"));
//         OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
//
//         ::kill(kitty->pid(), SIGSTOP);
//         Tests::waitUntilWindowsN(2);
//
//         {
//             auto str = getFromSocket("/activeworkspace");
//             ASSERT_CONTAINS(str, "windows: 0");
//         }
//
//         {
//             OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:hyprland-dialog' })"))
//             auto str = getFromSocket("/activewindow");
//             ASSERT_CONTAINS(str, "special:apple");
//         }
//     }
//
//     OK(getFromSocket("/reload"));
//     Tests::killAllWindows();
// }

// TODO: decompose this into multiple test cases
TEST_CASE(misc) {
    NLog::log("{}Testing close_special_on_empty", Colors::YELLOW);

    OK(getFromSocket("/eval hl.config({ misc = { close_special_on_empty = false } })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'special:test' })"));

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/monitors");
        ASSERT_CONTAINS(str, "special workspace: -");
    }

    Tests::killAllWindows();

    {
        auto str = getFromSocket("/monitors");
        ASSERT_CONTAINS(str, "special workspace: -");
    }

    Tests::spawnKitty();

    OK(getFromSocket("/eval hl.config({ misc = { close_special_on_empty = true } })"));

    Tests::killAllWindows();

    {
        auto str = getFromSocket("/monitors");
        ASSERT_NOT_CONTAINS(str, "special workspace: -");
    }

    NLog::log("{}Testing new_window_takes_over_fullscreen", Colors::YELLOW);

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::spawnKitty("kitty_A");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    Tests::spawnKitty("kitty_B");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));

    {
        // should be ignored as per focus_under_fullscreen 0
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

    Tests::spawnKitty("kitty_C");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
        EXPECT_CONTAINS(str, "kitty_C");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

    Tests::spawnKitty("kitty_D");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
        EXPECT_CONTAINS(str, "kitty_D");
    }

    OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

    Tests::killAllWindows();

    NLog::log("{}Testing exit_window_retains_fullscreen", Colors::YELLOW);

    OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = false } })"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    Tests::spawnKitty("kitty_B");
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    OK(getFromSocket("/eval hl.config({ misc = { exit_window_retains_fullscreen = true } })"));

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    Tests::killAllWindows();

    NLog::log("{}Testing fullscreen and fullscreenstate dispatcher", Colors::YELLOW);

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'unset' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 1");
        EXPECT_CONTAINS(str, "fullscreenClient: 1");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 3, client = 3, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 3");
        EXPECT_CONTAINS(str, "fullscreenClient: 3");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 3, client = 3, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 3");
        EXPECT_CONTAINS(str, "fullscreenClient: 3");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "fullscreenClient: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'toggle' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "fullscreenClient: 2");
    }
}

TEST_CASE(processesThatDieEarlyAreReaped) {
    // Ensure that the process autostarted in the config does not
    // become a zombie even if it terminates very quickly.
    ASSERT(Tests::execAndGet("pgrep -f 'sleep 0'").empty(), true);
}
