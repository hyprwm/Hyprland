#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include "../shared.hpp"

#include <chrono>
#include <thread>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
using namespace Hyprutils::Utils;

#define UP CUniquePointer
#define SP CSharedPointer

static bool waitForMonitorReservations(const std::string& reservedMonitor, const std::string& clearMonitor) {
    for (size_t i = 0; i < 50; ++i) {
        const auto result = getFromSocket(std::format("r/repl hl.get_monitor('{}').reserved.top > 0 and hl.get_monitor('{}').reserved.top == 0", reservedMonitor, clearMonitor));
        if (result == "true")
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Tests::sync();
    }

    return false;
}

static bool waitForMonitorReservationsCleared(const std::string& monitorA, const std::string& monitorB) {
    for (size_t i = 0; i < 50; ++i) {
        const auto result = getFromSocket(std::format("r/repl hl.get_monitor('{}').reserved.top == 0 and hl.get_monitor('{}').reserved.top == 0", monitorA, monitorB));
        if (result == "true")
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Tests::sync();
    }

    return false;
}

TEST_CASE(errorBarReservationFollowsFocusedMonitor) {
    static constexpr const char* TEST_OUTPUT = "HYPRTEST-ERROR-BAR";

    getFromSocket(std::format("/output remove {}", TEST_OUTPUT));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '1920x1080@60', position = '1920x0', scale = '1' }})", TEST_OUTPUT)));
    OK(getFromSocket(std::format("/output create headless {}", TEST_OUTPUT)));

    CScopeGuard guard = {[&]() {
        getFromSocket("/seterror disable");
        getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })");
        waitForMonitorReservationsCleared("HEADLESS-2", TEST_OUTPUT);
        getFromSocket(std::format("/output remove {}", TEST_OUTPUT));
    }};

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/seterror rgb(ff0000) reservation-test"));
    ASSERT(waitForMonitorReservations("HEADLESS-2", TEST_OUTPUT), true);

    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ monitor = '{}' }})", TEST_OUTPUT)));
    ASSERT(waitForMonitorReservations(TEST_OUTPUT, "HEADLESS-2"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    EXPECT(waitForMonitorReservations("HEADLESS-2", TEST_OUTPUT), true);
}

TEST_CASE(crossMonitorFullscreenFocus) {
    // Create a destination monitor to the right of the default one and pin the
    // destination workspace to it
    OK(getFromSocket("/eval hl.monitor({ output = 'HYPRTEST-2', mode = '1920x1080@60', position = 'auto-right', scale = '1' })"));
    OK(getFromSocket("/output create headless HYPRTEST-2"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'name:workspace2', monitor = 'HYPRTEST-2' })"));

    // 1 window on the left monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:workspace1' })"));
    SPAWN_KITTY("1A");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:1A' })"));
    const auto MON_SRC_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // 2 windows on the right monitor
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:workspace2' })"));
    SPAWN_KITTY("2A");
    SPAWN_KITTY("2B");
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:2B' })"));
    const auto MON_DST_ID = Tests::getAttribute(getFromSocket("/activewindow"), "monitor");

    // Sanity check: the two windows really do live on different monitors
    ASSERT_NOT(MON_SRC_ID, MON_DST_ID);

    // float and fullscreen 2B
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:2B' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'toggle' })"));

    // focus 1A and focus to the right
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:1A' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = \"right\" })"));

    const auto active = getFromSocket("/activewindow");
    EXPECT_CONTAINS(active, "class: 2B");

    Tests::killAllWindows();
    OK(getFromSocket("/output remove HYPRTEST-2"));
}

TEST_CASE(crossMonitorEmptyWorkspaceUnfocusesWindow) {
    getFromSocket("/output remove HYPRTEST-UNFOCUS");
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HYPRTEST-UNFOCUS', mode = '1920x1080@60', position = '1920x0', scale = '1' })"));
    OK(getFromSocket("/output create headless HYPRTEST-UNFOCUS"));

    CScopeGuard guard = {[&]() {
        Tests::killAllWindows();
        OK(getFromSocket("/output remove HYPRTEST-UNFOCUS"));
    }};

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    ASSERT(Tests::windowCount(), 0);

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HYPRTEST-UNFOCUS' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));

    SPAWN_KITTY("cross_monitor_ws2");

    {
        auto str = getFromSocket("/activewindow");
        ASSERT_CONTAINS(str, "class: cross_monitor_ws2");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    {
        auto str = getFromSocket("/activeworkspace");
        ASSERT_CONTAINS(str, "workspace 1 ");
    }

    ASSERT(getFromSocket("/activewindow"), "Invalid");
}
