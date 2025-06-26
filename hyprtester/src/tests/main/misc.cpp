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

bool testMisc() {
    NLog::log("{}Testing config: misc:", Colors::GREEN);

    NLog::log("{}Testing close_special_on_empty", Colors::YELLOW);

    OK(getFromSocket("/keyword misc:close_special_on_empty false"));
    OK(getFromSocket("/dispatch workspace special:test"));

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/monitors");
        EXPECT(str.contains("special workspace: -"), true);
    }

    Tests::killAllWindows();

    {
        auto str = getFromSocket("/monitors");
        EXPECT(str.contains("special workspace: -"), true);
    }

    Tests::spawnKitty();

    OK(getFromSocket("/keyword misc:close_special_on_empty true"));

    Tests::killAllWindows();

    {
        auto str = getFromSocket("/monitors");
        EXPECT(str.contains("special workspace: -"), false);
    }

    NLog::log("{}Testing new_window_takes_over_fullscreen", Colors::YELLOW);

    OK(getFromSocket("/keyword misc:new_window_takes_over_fullscreen 0"));

    Tests::spawnKitty("kitty_A");

    OK(getFromSocket("/dispatch fullscreen 0"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("fullscreen: 2"), true);
        EXPECT(str.contains("kitty_A"), true);
    }

    Tests::spawnKitty("kitty_B");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("fullscreen: 2"), true);
        EXPECT(str.contains("kitty_A"), true);
    }

    OK(getFromSocket("/keyword misc:new_window_takes_over_fullscreen 1"));

    Tests::spawnKitty("kitty_C");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("fullscreen: 2"), true);
        EXPECT(str.contains("kitty_C"), true);
    }

    OK(getFromSocket("/keyword misc:new_window_takes_over_fullscreen 2"));

    Tests::spawnKitty("kitty_D");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("fullscreen: 0"), true);
        EXPECT(str.contains("kitty_D"), true);
    }

    OK(getFromSocket("/keyword misc:new_window_takes_over_fullscreen 0"));

    Tests::killAllWindows();

    NLog::log("{}Testing exit_window_retains_fullscreen", Colors::YELLOW);

    OK(getFromSocket("/keyword misc:exit_window_retains_fullscreen false"));

    Tests::spawnKitty("kitty_A");
    Tests::spawnKitty("kitty_B");

    OK(getFromSocket("/dispatch fullscreen 0"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("fullscreen: 2"), true);
    }

    OK(getFromSocket("/dispatch killwindow activewindow"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("fullscreen: 0"), true);
    }

    Tests::spawnKitty("kitty_B");
    OK(getFromSocket("/dispatch fullscreen 0"));
    OK(getFromSocket("/keyword misc:exit_window_retains_fullscreen true"));

    OK(getFromSocket("/dispatch killwindow activewindow"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("fullscreen: 2"), true);
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}
