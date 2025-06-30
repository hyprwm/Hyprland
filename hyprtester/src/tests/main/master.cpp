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
    NLog::log("{}Testing master layout focus_master_on_close feature", Colors::GREEN);

    // test on workspace "master"
    NLog::log("{}Switching to workspace `master`", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:master");

    // Set layout to master
    NLog::log("{}Setting layout to master", Colors::YELLOW);
    OK(getFromSocket("/keyword general:layout master"));

    // Test 1: Default behavior (focus_master_on_close = 0)
    NLog::log("{}Test 1: Default behavior (focus_master_on_close = 0)", Colors::YELLOW);
    OK(getFromSocket("/keyword master:focus_master_on_close 0"));

    // Spawn first window (will be master)
    NLog::log("{}Spawning first window (will be master)", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty("master-test-1");
    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    // Wait for window to appear
    Tests::waitUntilWindowsN(1);
    EXPECT(Tests::windowCount(), 1);

    // Spawn second window (will be stack)
    NLog::log("{}Spawning second window (will be stack)", Colors::YELLOW);
    auto kittyProcB = Tests::spawnKitty("master-test-2");
    if (!kittyProcB) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    Tests::waitUntilWindowsN(2);
    EXPECT(Tests::windowCount(), 2);

    // Check layout structure
    {
        auto str = getFromSocket("/clients");
        NLog::log("{}Checking layout structure", Colors::YELLOW);
        // Should have one master and one stack window
        EXPECT_COUNT_STRING(str, "master: 1", 1);
        EXPECT_COUNT_STRING(str, "master: 0", 1);
    }

    // Focus the master window
    NLog::log("{}Focusing master window", Colors::YELLOW);
    OK(getFromSocket("/dispatch focuswindow class:master-test-1"));

    // Close the master window
    NLog::log("{}Closing master window (default behavior)", Colors::YELLOW);
    OK(getFromSocket("/dispatch killactive"));

    // Wait for window to close
    Tests::waitUntilWindowsN(1);
    EXPECT(Tests::windowCount(), 1);

    // Check that focus went to the remaining window (stack window)
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "master-test-2");
    }

    // Close the remaining window
    OK(getFromSocket("/dispatch killactive"));
    Tests::waitUntilWindowsN(0);
    EXPECT(Tests::windowCount(), 0);

    // Test 2: Enabled behavior (focus_master_on_close = 1)
    NLog::log("{}Test 2: Enabled behavior (focus_master_on_close = 1)", Colors::YELLOW);
    OK(getFromSocket("/keyword master:focus_master_on_close 1"));

    // Spawn three windows to test master-to-master focus
    NLog::log("{}Spawning three windows for master-to-master test", Colors::YELLOW);
    auto kittyProcC = Tests::spawnKitty("master-test-3");
    auto kittyProcD = Tests::spawnKitty("master-test-4");
    auto kittyProcE = Tests::spawnKitty("master-test-5");

    if (!kittyProcC || !kittyProcD || !kittyProcE) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    Tests::waitUntilWindowsN(3);
    EXPECT(Tests::windowCount(), 3);

    // Make the first two windows master windows
    NLog::log("{}Making first two windows master windows", Colors::YELLOW);
    OK(getFromSocket("/dispatch layoutmsg addmaster"));
    OK(getFromSocket("/dispatch layoutmsg addmaster"));

    // Check layout structure
    {
        auto str = getFromSocket("/clients");
        NLog::log("{}Checking layout structure with multiple masters", Colors::YELLOW);
        // Should have two master windows and one stack window
        EXPECT_COUNT_STRING(str, "master: 1", 2);
        EXPECT_COUNT_STRING(str, "master: 0", 1);
    }

    // Focus the first master window
    NLog::log("{}Focusing first master window", Colors::YELLOW);
    OK(getFromSocket("/dispatch focuswindow class:master-test-3"));

    // Close the first master window
    NLog::log("{}Closing first master window (enabled behavior)", Colors::YELLOW);
    OK(getFromSocket("/dispatch killactive"));

    // Wait for window to close
    Tests::waitUntilWindowsN(2);
    EXPECT(Tests::windowCount(), 2);

    // Check that focus went to the next master window
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "master-test-4");
    }

    // Close the second master window
    NLog::log("{}Closing second master window", Colors::YELLOW);
    OK(getFromSocket("/dispatch killactive"));

    // Wait for window to close
    Tests::waitUntilWindowsN(1);
    EXPECT(Tests::windowCount(), 1);

    // Check that focus went to the remaining stack window (fallback behavior)
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "master-test-5");
    }

    // Close the remaining window
    OK(getFromSocket("/dispatch killactive"));
    Tests::waitUntilWindowsN(0);
    EXPECT(Tests::windowCount(), 0);

    // Test 3: Edge case - single master window
    NLog::log("{}Test 3: Edge case - single master window", Colors::YELLOW);
    
    // Spawn one window
    auto kittyProcF = Tests::spawnKitty("master-test-6");
    if (!kittyProcF) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    Tests::waitUntilWindowsN(1);
    EXPECT(Tests::windowCount(), 1);

    // Focus the window
    OK(getFromSocket("/dispatch focuswindow class:master-test-6"));

    // Close the window
    NLog::log("{}Closing single master window", Colors::YELLOW);
    OK(getFromSocket("/dispatch killactive"));

    // Wait for window to close
    Tests::waitUntilWindowsN(0);
    EXPECT(Tests::windowCount(), 0);

    // Test 4: Verify config option is working
    NLog::log("{}Test 4: Verify config option is working", Colors::YELLOW);
    
    // Check that the config option is set correctly
    {
        auto str = getFromSocket("/keyword master:focus_master_on_close");
        EXPECT_CONTAINS(str, "1");
    }

    // Reset to default
    OK(getFromSocket("/keyword master:focus_master_on_close 0"));
    {
        auto str = getFromSocket("/keyword master:focus_master_on_close");
        EXPECT_CONTAINS(str, "0");
    }

    // Clean up - go back to workspace 1
    NLog::log("{}Cleaning up - returning to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));

    NLog::log("{}All master layout focus_master_on_close tests completed", Colors::GREEN);
    return !ret;
}

REGISTER_TEST_FN(test) 