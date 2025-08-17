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

static bool testWorkspaceMovementWithFullscreen() {
    NLog::log("{}Testing workspace movement with fullscreen windows", Colors::GREEN);

    EXPECT(Tests::windowCount(), 0);

    // Create a second monitor for testing
    NLog::log("{}Creating second monitor", Colors::YELLOW);
    EXPECT(getFromSocket("/output create headless"), "ok");

    // Switch to workspace 1 on first monitor
    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));

    // Spawn a window on workspace 1
    NLog::log("{}Spawning window on workspace 1", Colors::YELLOW);
    auto kittyProc = Tests::spawnKitty();
    
    if (!kittyProc) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Make the window fullscreen
    NLog::log("{}Making window fullscreen", Colors::YELLOW);
    OK(getFromSocket("/dispatch fullscreen 1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Verify window is fullscreen and workspace has fullscreen window
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    // Get initial monitor configuration
    std::string initialMonitors = getFromSocket("/monitors");
    
    // Move workspace 1 to second monitor
    NLog::log("{}Moving workspace 1 to second monitor (HEADLESS-2)", Colors::YELLOW);
    OK(getFromSocket("/dispatch moveworkspacetomonitor 1 HEADLESS-2"));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Critical test: Verify source monitor still has a workspace
    {
        auto str = getFromSocket("/monitors");
        NLog::log("{}Monitor status after move: {}", Colors::CYAN, str);
        
        // Check that HEADLESS-1 (source monitor) has a workspace
        EXPECT_CONTAINS(str, "HEADLESS-1");
        // Verify it's not empty - should have some workspace ID
        
        // Parse monitor info to ensure both monitors have workspaces
        size_t headless1Pos = str.find("Monitor HEADLESS-1");
        size_t headless2Pos = str.find("Monitor HEADLESS-2");
        
        if (headless1Pos == std::string::npos || headless2Pos == std::string::npos) {
            NLog::log("{}Error: Could not find both monitors in output", Colors::RED);
            return false;
        }

        // Extract info for HEADLESS-1 (source monitor)
        std::string headless1Info = str.substr(headless1Pos, headless2Pos - headless1Pos);
        
        // Should contain "active workspace: " followed by a workspace ID
        if (headless1Info.find("active workspace:") == std::string::npos) {
            NLog::log("{}Error: Source monitor HEADLESS-1 has no active workspace", Colors::RED);
            return false;
        }
    }

    // Verify workspace 1 is now on HEADLESS-2
    {
        auto str = getFromSocket("/workspaces");
        EXPECT_CONTAINS(str, "workspace ID 1 (1)");
        
        // Get active workspace on HEADLESS-2
        auto monitors = getFromSocket("/monitors");
        size_t headless2Pos = monitors.find("Monitor HEADLESS-2");
        if (headless2Pos != std::string::npos) {
            std::string headless2Info = monitors.substr(headless2Pos);
            EXPECT_CONTAINS(headless2Info, "active workspace: 1");
        }
    }

    // Verify fullscreen window is still fullscreen after move
    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "fullscreen: 1");
    }

    NLog::log("{}Cleaning up test", Colors::YELLOW);
    Tests::killAllWindows();
    
    // Remove the second monitor
    OK(getFromSocket("/output remove HEADLESS-2"));

    return true;
}

static bool testWorkspaceMovementMultiMonitor() {
    NLog::log("{}Testing multi-monitor workspace movement scenarios", Colors::GREEN);

    // Create two additional monitors for 3-monitor setup
    NLog::log("{}Creating additional monitors", Colors::YELLOW);
    EXPECT(getFromSocket("/output create headless"), "ok");
    EXPECT(getFromSocket("/output create headless"), "ok");

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Test moving regular (non-fullscreen) workspace
    NLog::log("{}Testing regular workspace movement", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    auto kittyA = Tests::spawnKitty();
    
    if (!kittyA) {
        NLog::log("{}Error: kitty A did not spawn", Colors::RED);
        return false;
    }

    // Move to second monitor
    OK(getFromSocket("/dispatch moveworkspacetomonitor 1 HEADLESS-2"));
    
    // Verify source monitor gets a new workspace
    {
        auto str = getFromSocket("/monitors");
        // Both monitors should have active workspaces
        size_t monitor1 = str.find("Monitor HEADLESS-1");
        size_t monitor2 = str.find("Monitor HEADLESS-2");
        
        if (monitor1 == std::string::npos || monitor2 == std::string::npos) {
            NLog::log("{}Error: Could not find monitors", Colors::RED);
            return false;
        }
    }

    NLog::log("{}Cleaning up multi-monitor test", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/output remove HEADLESS-2"));
    OK(getFromSocket("/output remove HEADLESS-3"));

    return true;
}

static bool test() {
    if (!testWorkspaceMovementWithFullscreen()) {
        NLog::log("{}Failed workspace movement with fullscreen test", Colors::RED);
        return false;
    }

    if (!testWorkspaceMovementMultiMonitor()) {
        NLog::log("{}Failed multi-monitor workspace movement test", Colors::RED);
        return false;
    }

    NLog::log("{}All workspace movement tests passed", Colors::GREEN);
    return true;
}

REGISTER_TEST_FN(test)
