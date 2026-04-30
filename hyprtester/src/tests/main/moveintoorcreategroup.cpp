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

TEST_CASE(moveIntoOrCreateGroup) {
    NLog::log("{}Dispatching workspace `moveintoorcreategroup`", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:moveintoorcreategroup' })");

    OK(getFromSocket("/eval hl.config({ group = { auto_group = false } })"));

    NLog::log("{}Spawning kittyA", Colors::YELLOW);
    auto kittyA = Tests::spawnKitty("kitty_A");
    if (!kittyA) {
        FAIL_TEST("Could not spawn kitty_A");
    }

    NLog::log("{}Spawning kittyB", Colors::YELLOW);
    auto kittyB = Tests::spawnKitty("kitty_B");
    if (!kittyB) {
        FAIL_TEST("Could not spawn kitty_B");
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 2);

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped: 0");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));

    NLog::log("{}Move kittyA into group with kittyB (creates group)", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_or_create_group = 'right' })"));

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
    ASSERT(Tests::windowCount(), 0);

    NLog::log("{}Testing moveintoorcreategroup into existing group", Colors::YELLOW);

    NLog::log("{}Spawning kittyC", Colors::YELLOW);
    auto kittyC = Tests::spawnKitty("kitty_C");
    NLog::log("{}Spawning kittyD", Colors::YELLOW);
    auto kittyD = Tests::spawnKitty("kitty_D");
    NLog::log("{}Spawning kittyE", Colors::YELLOW);
    auto kittyE = Tests::spawnKitty("kitty_E");

    NLog::log("{}Expecting 3 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 3);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_D' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_E' })"));

    NLog::log("{}Move kittyE into existing group with kittyD", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_or_create_group = 'left' })"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "grouped:");
    }

    NLog::log("{}Verify active window is kitty_E (the moved window)", Colors::YELLOW);
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "kitty_E");
    }
}
