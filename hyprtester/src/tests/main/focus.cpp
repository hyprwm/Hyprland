#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include "../shared.hpp"

#include <chrono>
#include <string>
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

TEST_CASE(silentMoveLastTiledWindowUnfocusesWindow) {
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    ASSERT(Tests::windowCount(), 0);

    SPAWN_KITTY("silent_move_last_tiled");
    ASSERT(Tests::windowCount(), 1);

    const auto active = getFromSocket("/activewindow");
    ASSERT_CONTAINS(active, "class: silent_move_last_tiled");
    ASSERT_CONTAINS(active, "workspace: 1 (1)");
    ASSERT_CONTAINS(active, "floating: 0");

    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = '2', follow = false })"));

    // Check focus before switching workspaces, which could hide stale focus.
    EXPECT_CONTAINS(getFromSocket("/activeworkspace"), "workspace 1 ");
    EXPECT(getFromSocket("/activewindow"), "Invalid");
    ASSERT(Tests::windowCount(), 1);

    const auto clients = getFromSocket("/clients");
    EXPECT_CONTAINS(clients, "class: silent_move_last_tiled");
    EXPECT_CONTAINS(clients, "workspace: 2 (2)");
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

TEST_CASE(followMouseThreshold) {

    // follow_mouse = 1 + follow_mouse_threshold = 100:
    // the window under the cursor must NOT get focus until the cursor has travelled the threshold, and must get focus once it has.
    NLog::log("{}Testing follow_mouse_threshold", Colors::GREEN);

    const auto activeClass = [&]() -> std::string { return Tests::getAttribute(getFromSocket("/activewindow"), "class"); };

    const auto waitForActiveClass = [&](const std::string& class_, int maxTries = 50) -> bool {
        for (int i = 0; i < maxTries; ++i) {
            if (activeClass() == class_)
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return activeClass() == class_;
    };

    getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:follow_mouse_threshold' })");

    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 1 } })"));
    OK(getFromSocket("/eval hl.config({ input = { follow_mouse_threshold = 100 } })"));

    // Two floating windows with a 20px gap, side by side
    // fmt_a: position (100,100), size 400x400 -> hitbox [100,499] x [100,499]
    SPAWN_KITTY("fmt_a");
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 400, y = 400 })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 100, y = 100 })"));

    // fmt_b: position (520,100), size 400x400 -> hitbox [520,919] x [100,499]
    SPAWN_KITTY("fmt_b");
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 400, y = 400 })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 520, y = 100 })"));

    // Start state: fmt_a focused, cursor inside fmt_a right next to fmt_b.
    // Focusing may warp the cursor, so place the cursor explicitly afterwards.
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:fmt_a' })"));
    ASSERT(waitForActiveClass("fmt_a"), true);
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 490, y = 300 })"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT(activeClass(), "fmt_a");

    // The cursor moves right in 40px steps, entering fmt_b on the first one.
    // Travelled so far, from x = 490:

    NLog::log("{}Cursor travelled 40px (< 100), now over fmt_b: focus must stay on fmt_a", Colors::GREEN);
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 530, y = 300 })"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT(activeClass(), "fmt_a");

    NLog::log("{}Cursor travelled 80px (< 100), still over fmt_b: focus must stay on fmt_a", Colors::GREEN);
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 570, y = 300 })"));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT(activeClass(), "fmt_a");

    NLog::log("{}Cursor travelled 120px (> 100), still over fmt_b: focus must switch to fmt_b", Colors::GREEN);
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 610, y = 300 })"));
    ASSERT(waitForActiveClass("fmt_b"), true);
}