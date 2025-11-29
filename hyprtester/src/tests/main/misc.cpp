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
    NLog::log("{}Testing config: misc:", Colors::GREEN);

    NLog::log("{}Testing close_special_on_empty", Colors::YELLOW);

    OK(getFromSocket("/keyword misc:close_special_on_empty false"));
    OK(getFromSocket("/dispatch workspace special:test"));

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "special workspace: -");
    }

    Tests::killAllWindows();

    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "special workspace: -");
    }

    Tests::spawnKitty();

    OK(getFromSocket("/keyword misc:close_special_on_empty true"));

    Tests::killAllWindows();

    {
        auto str = getFromSocket("/monitors");
        EXPECT_NOT_CONTAINS(str, "special workspace: -");
    }

    NLog::log("{}Testing new_window_takes_over_fullscreen", Colors::YELLOW);

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 0"));

    Tests::spawnKitty("kitty_A");

    OK(getFromSocket("/dispatch fullscreen 0"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    Tests::spawnKitty("kitty_B");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/dispatch focuswindow class:kitty_B"));

    {
        // should be ignored as per focus_under_fullscreen 0
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 1"));

    Tests::spawnKitty("kitty_C");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
        EXPECT_CONTAINS(str, "kitty_C");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 2"));

    Tests::spawnKitty("kitty_D");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
        EXPECT_CONTAINS(str, "kitty_D");
    }

    OK(getFromSocket("/keyword misc:on_focus_under_fullscreen 0"));

    Tests::killAllWindows();

    NLog::log("{}Testing exit_window_retains_fullscreen", Colors::YELLOW);

    OK(getFromSocket("/keyword misc:exit_window_retains_fullscreen false"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch fullscreen 0"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch killwindow activewindow"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    Tests::spawnKitty("kitty_B");
    OK(getFromSocket("/dispatch fullscreen 0"));
    OK(getFromSocket("/keyword misc:exit_window_retains_fullscreen true"));

    OK(getFromSocket("/dispatch killwindow activewindow"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    Tests::killAllWindows();

    NLog::log("{}Testing fullscreen and fullscreenstate dispatcher", Colors::YELLOW);

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch focuswindow class:kitty_A"));
    OK(getFromSocket("/dispatch fullscreen 0 set"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch fullscreen 0 unset"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch fullscreen 1 toggle"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    OK(getFromSocket("/dispatch fullscreen 1 toggle"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch fullscreenstate 2 2 set"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch fullscreenstate 2 2 set"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch fullscreenstate 2 2 toggle"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch fullscreenstate 2 2 toggle"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    // Ensure that the process autostarted in the config does not
    // become a zombie even if it terminates very quickly.
    EXPECT(Tests::execAndGet("pgrep -f 'sleep 0'").empty(), true);

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_TEST_FN(test);
