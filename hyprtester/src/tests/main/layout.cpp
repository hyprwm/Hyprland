#include "../shared.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <chrono>
#include <format>
#include <thread>

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

static bool waitForMonitor(const char* name) {
    for (int i = 0; i < 50; ++i) {
        if (getFromSocket("/monitors").contains(name))
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

static bool waitForMonitorRemoved(const char* name) {
    for (int i = 0; i < 50; ++i) {
        if (!getFromSocket("/monitors").contains(name))
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

TEST_CASE(single_window_aspect_ratio) {
    OK(getFromSocket("/eval hl.config({ layout = { single_window_aspect_ratio = '1 1' } })"));

    Tests::spawnKitty();

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 442,22");
        EXPECT_CONTAINS(str, "size: 1036,1036");
    }

    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));

    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 442,22");
        EXPECT_CONTAINS(str, "size: 1036,1036");
    }

    // don't use swar on maximized
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
    }
}

// Don't crash when focus after global geometry changes
TEST_CASE(crashOnGeomUpdate) {
    Tests::spawnKitty();
    Tests::spawnKitty();
    Tests::spawnKitty();

    // move the layout
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '1000x0', scale = '1' })"));

    // shouldnt crash
    OK(getFromSocket("/dispatch hl.dsp.focus({ direction = 'right' })"));

    // restore HEADLESS-2 position so subsequent tests don't inherit the relocated monitor
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
}

// Don't crash when a pending layout refresh runs after monitor teardown.
TEST_CASE(layoutRefreshDuringMonitorTeardown) {
    static constexpr const char* TEST_OUTPUT = "HEADLESS-5";

    getFromSocket(std::format("/output remove {}", TEST_OUTPUT));
    OK(getFromSocket(std::format("/output create headless {}", TEST_OUTPUT)));
    ASSERT(waitForMonitor(TEST_OUTPUT), true);

    CScopeGuard guard = {[&]() {
        Tests::killAllWindows();
        OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', disabled = false, mode = '1920x1080@60', position = '1920x0', scale = '1' }})", TEST_OUTPUT)));
        ASSERT(waitForMonitor(TEST_OUTPUT), true);
        OK(getFromSocket(std::format("/output remove {}", TEST_OUTPUT)));
        ASSERT(waitForMonitorRemoved(TEST_OUTPUT), true);
        OK(getFromSocket("/reload"));
    }};

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', disabled = false, mode = '1920x1080@60', position = '1920x0', scale = '1' }})", TEST_OUTPUT)));
    ASSERT(waitForMonitor(TEST_OUTPUT), true);

    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ monitor = '{}' }})", TEST_OUTPUT)));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '30' })"));
    ASSERT(!!Tests::spawnKitty("layout_teardown_a"), true);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '31' })"));
    ASSERT(!!Tests::spawnKitty("layout_teardown_b"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));

    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', disabled = true }}); hl.config({{ general = {{ layout = 'master' }} }})", TEST_OUTPUT)));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ASSERT_CONTAINS(getFromSocket("/version"), "Hyprland");
    EXPECT_CONTAINS(getFromSocket("/monitors all"), TEST_OUTPUT);
}

// Test if size + pos is preserved after fs cycle
TEST_CASE(posPreserve) {
    Tests::spawnKitty();

    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 1337, y = 69, window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 420, y = 420, window = 'class:kitty' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 420,420");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ direction = 'right' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 581,420");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "at: 581,420");
        EXPECT_CONTAINS(str, "size: 1337,69");
    }
}

TEST_CASE(focusMRUAfterClose) {
    NLog::log("{}Testing focus after close (MRU order)", Colors::GREEN);

    OK(getFromSocket("/reload"));
    OK(getFromSocket("/eval hl.config({ dwindle = { default_split_ratio = 1.25 } })"));
    OK(getFromSocket("/eval hl.config({ input = { focus_on_close = 2 } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);
    ASSERT(!!Tests::spawnKitty("kitty_C"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_C' })"));

    OK(getFromSocket("/dispatch hl.dsp.window.kill()"));
    Tests::waitUntilWindowsN(2);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_B"), true);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.kill()"));
    Tests::waitUntilWindowsN(1);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_A"), true);
    }
}

TEST_CASE(focusPreservedLayoutChange) {
    OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

    ASSERT(!!Tests::spawnKitty("kitty_A"), true);
    ASSERT(!!Tests::spawnKitty("kitty_B"), true);
    ASSERT(!!Tests::spawnKitty("kitty_C"), true);
    ASSERT(!!Tests::spawnKitty("kitty_D"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_C' })"));

    OK(getFromSocket("r/eval hl.config({ general = { layout = 'monocle' } })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_C"), true);
    }
}
