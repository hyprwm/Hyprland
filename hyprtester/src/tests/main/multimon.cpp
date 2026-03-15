#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

static int ret = 0;

// use high workspace numbers to avoid collisions with other tests
static void testSwapActiveWorkspaces() {
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 501"));

    if (!Tests::spawnKitty("swap_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch focusmonitor HEADLESS-3"));
    OK(getFromSocket("/dispatch workspace 502"));

    if (!Tests::spawnKitty("swap_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch swapactiveworkspaces HEADLESS-2 HEADLESS-3"));

    // after swap, HEADLESS-2 should have ws 502, HEADLESS-3 should have ws 501
    {
        auto str = getFromSocket("/monitors");

        auto h2pos = str.find("Monitor HEADLESS-2");
        auto h3pos = str.find("Monitor HEADLESS-3");

        if (h2pos != std::string::npos && h3pos != std::string::npos) {
            auto h2end     = (h3pos > h2pos) ? h3pos : str.size();
            auto h3end     = (h2pos > h3pos) ? h2pos : str.size();
            auto h2section = str.substr(h2pos, h2end - h2pos);
            auto h3section = str.substr(h3pos, h3end - h3pos);

            EXPECT_CONTAINS(h2section, "active workspace: 502");
            EXPECT_CONTAINS(h3section, "active workspace: 501");
        } else {
            NLog::log("{}Could not find both monitors in output", Colors::RED);
            ++TESTS_FAILED;
            ret = 1;
        }
    }

    // verify the windows stayed on their workspaces
    OK(getFromSocket("/dispatch focuswindow class:swap_a"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "workspace: 501");

    OK(getFromSocket("/dispatch focuswindow class:swap_b"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "workspace: 502");

    Tests::killAllWindows();
}

static void testMoveWorkspaceToMonitor() {
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 503"));

    if (!Tests::spawnKitty("movews_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch moveworkspacetomonitor 503 HEADLESS-3"));

    {
        auto str   = getFromSocket("/monitors");
        auto h3pos = str.find("Monitor HEADLESS-3");
        if (h3pos != std::string::npos) {
            auto h3section = str.substr(h3pos);
            EXPECT_CONTAINS(h3section, "active workspace: 503");
        } else {
            NLog::log("{}Could not find HEADLESS-3 in monitors output", Colors::RED);
            ++TESTS_FAILED;
            ret = 1;
        }
    }

    Tests::killAllWindows();
}

static void testMoveCurrentWorkspaceToMonitor() {
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 504"));

    if (!Tests::spawnKitty("movecur_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch movecurrentworkspacetomonitor HEADLESS-3"));

    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "workspace ID 504 ");

    {
        auto str   = getFromSocket("/monitors");
        auto h3pos = str.find("Monitor HEADLESS-3");
        if (h3pos != std::string::npos) {
            auto h3section = str.substr(h3pos);
            EXPECT_CONTAINS(h3section, "active workspace: 504");
        } else {
            NLog::log("{}Could not find HEADLESS-3 in monitors output", Colors::RED);
            ++TESTS_FAILED;
            ret = 1;
        }
    }

    Tests::killAllWindows();
}

static void testFocusWorkspaceOnCurrentMonitor() {
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 505"));

    if (!Tests::spawnKitty("fwocm_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch focusmonitor HEADLESS-3"));
    OK(getFromSocket("/dispatch workspace 506"));

    if (!Tests::spawnKitty("fwocm_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    // from HEADLESS-3, focus workspace 505 on current monitor -- should pull it here
    OK(getFromSocket("/dispatch focusworkspaceoncurrentmonitor 505"));

    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "workspace ID 505 ");

    OK(getFromSocket("/dispatch focuswindow class:fwocm_a"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: fwocm_a");

    Tests::killAllWindows();
}

static void testMoveWindowAcrossMonitors() {
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 507"));

    if (!Tests::spawnKitty("crossmon_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: crossmon_a");

    OK(getFromSocket("/dispatch movetoworkspace 508"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "class: crossmon_a");
        EXPECT_CONTAINS(str, "workspace: 508");
    }

    Tests::killAllWindows();
}

static void testFocusMonitorCycling() {
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 509"));

    if (!Tests::spawnKitty("focidx_a")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch focusmonitor HEADLESS-3"));
    OK(getFromSocket("/dispatch workspace 510"));

    if (!Tests::spawnKitty("focidx_b")) {
        ++TESTS_FAILED;
        ret = 1;
        return;
    }

    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: focidx_a");

    OK(getFromSocket("/dispatch focusmonitor +1"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: focidx_b");

    OK(getFromSocket("/dispatch focusmonitor +1"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "class: focidx_a");

    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing multi-monitor dispatchers", Colors::GREEN);

    EXPECT(getFromSocket("/output create headless HEADLESS-3"), "ok");

    NLog::log("{}Testing swapactiveworkspaces", Colors::GREEN);
    testSwapActiveWorkspaces();

    NLog::log("{}Testing moveworkspacetomonitor", Colors::GREEN);
    testMoveWorkspaceToMonitor();

    NLog::log("{}Testing movecurrentworkspacetomonitor", Colors::GREEN);
    testMoveCurrentWorkspaceToMonitor();

    NLog::log("{}Testing focusworkspaceoncurrentmonitor", Colors::GREEN);
    testFocusWorkspaceOnCurrentMonitor();

    NLog::log("{}Testing movewindow across monitors", Colors::GREEN);
    testMoveWindowAcrossMonitors();

    NLog::log("{}Testing focusmonitor cycling", Colors::GREEN);
    testFocusMonitorCycling();

    // clean up: kill windows, focus back to HEADLESS-2 ws 1, remove extra monitor
    NLog::log("Cleaning up", Colors::YELLOW);
    Tests::killAllWindows();
    OK(getFromSocket("/dispatch focusmonitor HEADLESS-2"));
    OK(getFromSocket("/dispatch workspace 1"));
    OK(getFromSocket("/output remove HEADLESS-3"));
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test);
