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

static bool test() {
    NLog::log("{}Testing workspaces", Colors::GREEN);

    EXPECT(Tests::windowCount(), 0);

    // test on workspace "window"
    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));

    NLog::log("{}Checking persistent no-mon", Colors::YELLOW);
    OK(getFromSocket("r/keyword workspace 966,persistent:1"));

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
    OK(getFromSocket("/dispatch workspace 3"));

    NLog::log("{}Spawning kittyProc on ws 3", Colors::YELLOW);
    auto kittyProcB = Tests::spawnKitty();

    if (!kittyProcB) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));

    NLog::log("{}Switching to workspace +1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace +1"));

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
    OK(getFromSocket("/dispatch workspace 1"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace m+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace m+1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace -1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace -1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));

    NLog::log("{}Switching to workspace r+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace r+1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace r+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace r+1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace r~1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace r~1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace empty", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace empty"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 2 (2)");
    }

    NLog::log("{}Switching to workspace previous", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace previous"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace name:TEST_WORKSPACE_NULL", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace name:TEST_WORKSPACE_NULL"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID -1337 (TEST_WORKSPACE_NULL)");
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));

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
    OK(getFromSocket("/dispatch focusmonitor 0"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace r+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace r+1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace r~2", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/dispatch workspace r~2"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    NLog::log("{}Switching to workspace m+1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace m+1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    // no OK: this will throw an error as it should
    getFromSocket("/dispatch workspace 1");

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    NLog::log("{}Testing back_and_forth", Colors::YELLOW);
    OK(getFromSocket("/keyword binds:workspace_back_and_forth true"));
    OK(getFromSocket("/dispatch workspace 1"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    OK(getFromSocket("/keyword binds:workspace_back_and_forth false"));

    NLog::log("{}Testing hide_special_on_workspace_change", Colors::YELLOW);
    OK(getFromSocket("/keyword binds:hide_special_on_workspace_change true"));
    OK(getFromSocket("/dispatch workspace special:HELLO"));

    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "special workspace: -");
        EXPECT_CONTAINS(str, "special:HELLO");
    }

    // no OK: will err (it shouldn't prolly but oh well)
    getFromSocket("/dispatch workspace 3");

    {
        auto str = getFromSocket("/monitors");
        EXPECT_COUNT_STRING(str, "special workspace: 0 ()", 2);
    }

    OK(getFromSocket("/keyword binds:hide_special_on_workspace_change false"));

    NLog::log("{}Testing allow_workspace_cycles", Colors::YELLOW);
    OK(getFromSocket("/keyword binds:allow_workspace_cycles true"));

    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/dispatch workspace 3"));
    OK(getFromSocket("/dispatch workspace 1"));

    OK(getFromSocket("/dispatch workspace previous"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    OK(getFromSocket("/dispatch workspace previous"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 1 (1)");
    }

    OK(getFromSocket("/dispatch workspace previous"));

    {
        auto str = getFromSocket("/activeworkspace");
        EXPECT_STARTS_WITH(str, "workspace ID 3 (3)");
    }

    OK(getFromSocket("/keyword binds:allow_workspace_cycles false"));

    OK(getFromSocket("/dispatch workspace 1"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    // spawn 3 kitties
    NLog::log("{}Testing focus_preferred_method", Colors::YELLOW);
    OK(getFromSocket("/keyword dwindle:force_split 2"));
    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");
    Tests::spawnKitty("kitty_C");
    OK(getFromSocket("/keyword dwindle:force_split 0"));

    // focus kitty 2: will be top right (dwindle)
    OK(getFromSocket("/dispatch focuswindow class:kitty_B"));

    // resize it to be a bit taller
    OK(getFromSocket("/dispatch resizeactive +20 +20"));

    // now we test focus methods.
    OK(getFromSocket("/keyword binds:focus_preferred_method 0"));

    OK(getFromSocket("/dispatch focuswindow class:kitty_C"));
    OK(getFromSocket("/dispatch focuswindow class:kitty_A"));

    OK(getFromSocket("/dispatch movefocus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_C");
    }

    OK(getFromSocket("/dispatch focuswindow class:kitty_A"));

    OK(getFromSocket("/keyword binds:focus_preferred_method 1"));

    OK(getFromSocket("/dispatch movefocus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_B");
    }

    NLog::log("{}Testing movefocus_cycles_fullscreen", Colors::YELLOW);
    OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-3"));
    Tests::spawnKitty("kitty_D");
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_D");
    }

    OK(getFromSocket("/dispatch focusmonitor l"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_A");
    }

    OK(getFromSocket("/keyword binds:movefocus_cycles_fullscreen false"));
    OK(getFromSocket("/dispatch fullscreen 0"));

    OK(getFromSocket("/dispatch movefocus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_D");
    }

    OK(getFromSocket("/dispatch focusmonitor l"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_A");
    }

    OK(getFromSocket("/keyword binds:movefocus_cycles_fullscreen true"));

    OK(getFromSocket("/dispatch movefocus r"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: kitty_B");
    }

    // destroy the headless output
    OK(getFromSocket("/output remove HEADLESS-3"));

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Testing asymmetric gap splits", Colors::YELLOW);
    {

        CScopeGuard guard = {[&]() {
            NLog::log("{}Cleaning up asymmetric gap test", Colors::YELLOW);
            Tests::killAllWindows();
            OK(getFromSocket("/reload"));
        }};

        OK(getFromSocket("/dispatch workspace name:gap_split_test"));
        OK(getFromSocket("r/keyword general:gaps_in 0"));
        OK(getFromSocket("r/keyword general:border_size 0"));
        OK(getFromSocket("r/keyword dwindle:split_width_multiplier 1.0"));
        OK(getFromSocket("r/keyword workspace name:gap_split_test,gapsout:0 1000 0 0"));

        NLog::log("{}Testing default split (force_split = 0)", Colors::YELLOW);
        OK(getFromSocket("r/keyword dwindle:force_split 0"));

        if (!Tests::spawnKitty("gaps_kitty_A") || !Tests::spawnKitty("gaps_kitty_B")) {
            return false;
        }

        NLog::log("{}Expecting vertical split (B below A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_A"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_B"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,540");

        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);

        NLog::log("{}Testing force_split = 1", Colors::YELLOW);
        OK(getFromSocket("r/keyword dwindle:force_split 1"));

        if (!Tests::spawnKitty("gaps_kitty_A") || !Tests::spawnKitty("gaps_kitty_B")) {
            return false;
        }

        NLog::log("{}Expecting vertical split (B above A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_B"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_A"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,540");

        NLog::log("{}Expecting horizontal split (C left of B)", Colors::YELLOW);
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_B"));

        if (!Tests::spawnKitty("gaps_kitty_C")) {
            return false;
        }

        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_C"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_B"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 460,0");

        Tests::killAllWindows();
        EXPECT(Tests::windowCount(), 0);

        NLog::log("{}Testing force_split = 2", Colors::YELLOW);
        OK(getFromSocket("r/keyword dwindle:force_split 2"));

        if (!Tests::spawnKitty("gaps_kitty_A") || !Tests::spawnKitty("gaps_kitty_B")) {
            return false;
        }

        NLog::log("{}Expecting vertical split (B below A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_A"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_B"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,540");

        NLog::log("{}Expecting horizontal split (C right of A)", Colors::YELLOW);
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_A"));

        if (!Tests::spawnKitty("gaps_kitty_C")) {
            return false;
        }

        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_A"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 0,0");
        OK(getFromSocket("/dispatch focuswindow class:gaps_kitty_C"));
        EXPECT_CONTAINS(getFromSocket("/activewindow"), "at: 460,0");
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test)
