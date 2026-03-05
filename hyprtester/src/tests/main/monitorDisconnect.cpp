#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int ret = 0;

// Don't crash when a monitor is removed while windows are tiled and a config reload follows
static void testCrashOnMonitorDisconnectReload() {
    // focus a secondary monitor and spawn tiled windows on it
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    Tests::spawnKitty();
    Tests::spawnKitty();

    EXPECT(Tests::windowCount(), 2);

    // remove the monitor (simulates disconnect)
    OK(getFromSocket("/output remove HEADLESS-2"));

    // trigger a config reload while the monitor is gone — this used to crash
    // in CDwindleAlgorithm::newTarget() during updateWorkspaceLayouts()
    OK(getFromSocket("/reload"));

    // verify Hyprland is still responsive
    auto str = getFromSocket("/monitors");
    EXPECT_CONTAINS(str, "HEADLESS-1");
    EXPECT_NOT_CONTAINS(str, "HEADLESS-2");

    // windows should have been moved to a remaining monitor
    auto clients = getFromSocket("/clients");
    EXPECT_NOT_CONTAINS(clients, "HEADLESS-2");

    // restore the monitor for subsequent tests
    OK(getFromSocket("/output create headless HEADLESS-2"));
    OK(getFromSocket("/keyword monitor HEADLESS-2,1920x1080@60,auto-right,1"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

// Don't crash when layout name resolves via fallback and a monitor is removed
static void testCrashOnFallbackLayoutMonitorDisconnect() {
    // set layout to an unknown name that falls back to dwindle
    OK(getFromSocket("/keyword general:layout default"));

    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    Tests::spawnKitty();

    // remove monitor + reload — the fallback name mismatch used to cause
    // a redundant layout switchup that crashed during monitor disconnect
    OK(getFromSocket("/output remove HEADLESS-2"));
    OK(getFromSocket("/reload"));

    auto str = getFromSocket("/monitors");
    EXPECT_NOT_CONTAINS(str, "HEADLESS-2");

    // restore
    OK(getFromSocket("/output create headless HEADLESS-2"));
    OK(getFromSocket("/keyword monitor HEADLESS-2,1920x1080@60,auto-right,1"));
    OK(getFromSocket("/keyword general:layout dwindle"));
    OK(getFromSocket("/reload"));

    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing monitor disconnect + config reload", Colors::GREEN);

    testCrashOnMonitorDisconnectReload();
    testCrashOnFallbackLayoutMonitorDisconnect();

    // clean up
    NLog::log("Cleaning up", Colors::YELLOW);
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
