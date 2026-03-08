#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int ret = 0;

// Don't crash when a monitor is removed while windows are tiled and a config reload follows
static void testCrashOnMonitorDisconnectReload() {
    NLog::log("{}Focusing HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));

    NLog::log("{}Spawning tiled windows on HEADLESS-2", Colors::YELLOW);
    for (auto const& win : {"disconnect_a", "disconnect_b"}) {
        if (!Tests::spawnKitty(win)) {
            NLog::log("{}Failed to spawn kitty with class `{}`", Colors::RED, win);
            ++TESTS_FAILED;
            ret = 1;
            return;
        }
    }

    EXPECT(Tests::windowCount(), 2);

    // remove the monitor (simulates disconnect)
    NLog::log("{}Removing HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/output remove HEADLESS-2"));

    // trigger a config reload while the monitor is gone — this used to crash
    // in CDwindleAlgorithm::newTarget() during updateWorkspaceLayouts()
    NLog::log("{}Reloading config with HEADLESS-2 removed", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    // verify Hyprland is still responsive
    {
        auto str = getFromSocket("/monitors");
        EXPECT_CONTAINS(str, "HEADLESS-1");
        EXPECT_NOT_CONTAINS(str, "HEADLESS-2");
    }

    // windows should have been moved to a remaining monitor
    {
        auto clients = getFromSocket("/clients");
        EXPECT_NOT_CONTAINS(clients, "HEADLESS-2");
    }

    // restore the monitor for subsequent tests
    NLog::log("{}Restoring HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/output create headless HEADLESS-2"));
    OK(getFromSocket("/keyword monitor HEADLESS-2,1920x1080@60,auto-right,1"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

// Don't crash when layout name resolves via fallback and a monitor is removed
static void testCrashOnFallbackLayoutMonitorDisconnect() {
    // set layout to an unknown name that falls back to dwindle
    NLog::log("{}Setting layout to fallback name", Colors::YELLOW);
    OK(getFromSocket("/keyword general:layout default"));

    NLog::log("{}Focusing HEADLESS-2", Colors::YELLOW);
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));

    NLog::log("{}Spawning window on HEADLESS-2", Colors::YELLOW);
    if (!Tests::spawnKitty("disconnect_fallback")) {
        NLog::log("{}Failed to spawn kitty", Colors::RED);
        ++TESTS_FAILED;
        ret = 1;
        OK(getFromSocket("/keyword general:layout dwindle"));
        return;
    }

    // remove monitor + reload — the fallback name mismatch used to cause
    // a redundant layout switchup that crashed during monitor disconnect
    NLog::log("{}Removing HEADLESS-2 with fallback layout", Colors::YELLOW);
    OK(getFromSocket("/output remove HEADLESS-2"));

    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    {
        auto str = getFromSocket("/monitors");
        EXPECT_NOT_CONTAINS(str, "HEADLESS-2");
    }

    // restore
    NLog::log("{}Restoring HEADLESS-2 and layout", Colors::YELLOW);
    OK(getFromSocket("/output create headless HEADLESS-2"));
    OK(getFromSocket("/keyword monitor HEADLESS-2,1920x1080@60,auto-right,1"));
    OK(getFromSocket("/keyword general:layout dwindle"));
    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing monitor disconnect + config reload", Colors::GREEN);

    NLog::log("{}Test 1: crash on monitor disconnect + reload", Colors::GREEN);
    testCrashOnMonitorDisconnectReload();

    NLog::log("{}Test 2: crash on fallback layout + monitor disconnect", Colors::GREEN);
    testCrashOnFallbackLayoutMonitorDisconnect();

    // clean up
    NLog::log("{}Cleaning up", Colors::YELLOW);
    getFromSocket("/dispatch workspace 1");
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
