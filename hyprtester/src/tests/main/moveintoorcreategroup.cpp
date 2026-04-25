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
    NLog::log("{}Testing moveintoorcreategroup", Colors::GREEN);

    NLog::log("{}Dispatching workspace `moveintoorcreategroup`", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:moveintoorcreategroup");

    OK(getFromSocket("/keyword group:auto_group false"));

    NLog::log("{}Spawning kittyA", Colors::YELLOW);
    auto kittyA = Tests::spawnKitty("kitty_A");
    if (!kittyA) {
        NLog::log("{}Error: kittyA did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Spawning kittyB", Colors::YELLOW);
    auto kittyB = Tests::spawnKitty("kitty_B");
    if (!kittyB) {
        NLog::log("{}Error: kittyB did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped: 0");
    }

    OK(getFromSocket("/dispatch focuswindow class:kitty_A"));

    NLog::log("{}Move kittyA into group with kittyB (creates group)", Colors::YELLOW);
    OK(getFromSocket("/dispatch moveintoorcreategroup r"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped:");
    }

    NLog::log("{}Verify active window is kitty_A (the moved window)", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "kitty_A");
    }

    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    NLog::log("{}Testing moveintoorcreategroup into existing group", Colors::YELLOW);

    NLog::log("{}Spawning kittyC", Colors::YELLOW);
    auto kittyC = Tests::spawnKitty("kitty_C");
    NLog::log("{}Spawning kittyD", Colors::YELLOW);
    auto kittyD = Tests::spawnKitty("kitty_D");
    NLog::log("{}Spawning kittyE", Colors::YELLOW);
    auto kittyE = Tests::spawnKitty("kitty_E");

    NLog::log("{}Expecting 3 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 3);

    OK(getFromSocket("/dispatch focuswindow class:kitty_D"));
    OK(getFromSocket("/dispatch togglegroup"));

    OK(getFromSocket("/dispatch focuswindow class:kitty_E"));

    NLog::log("{}Move kittyE into existing group with kittyD", Colors::YELLOW);
    OK(getFromSocket("/dispatch moveintoorcreategroup l"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped:");
    }

    NLog::log("{}Verify active window is kitty_E (the moved window)", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "kitty_E");
    }

    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/keyword group:auto_group true"));

    return !ret;
}

REGISTER_TEST_FN(test)
