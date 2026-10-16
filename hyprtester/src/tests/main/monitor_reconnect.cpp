#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

#include <chrono>
#include <format>
#include <string>
#include <thread>

using namespace Hyprutils::Utils;

static constexpr const char* RECONNECT_OUTPUT = "HYPRTEST-RECONNECT";
static constexpr const char* RECONNECT_CLASS  = "reconnect_floating";

static bool                  waitForOutput(bool present) {
    for (int i = 0; i < 50; ++i) {
        if (getFromSocket("/monitors all").contains(std::format("Monitor {} (ID ", RECONNECT_OUTPUT)) == present)
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

static std::string floatingWindowPosition() {
    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'class:{}' }})", RECONNECT_CLASS)) != "ok")
        return "";

    return Tests::getAttribute(getFromSocket("/activewindow"), "at");
}

static bool waitForFloatingWindowPosition(const std::string& expected) {
    for (int i = 0; i < 50; ++i) {
        Tests::sync();
        if (floatingWindowPosition() == expected)
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

TEST_CASE(floatingWindowKeepsPositionAfterMonitorReconnect) {
    getFromSocket(std::format("/output remove {}", RECONNECT_OUTPUT));
    waitForOutput(false);

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1' })"));
    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '1920x1080@60', position = '1920x0', scale = '1' }})", RECONNECT_OUTPUT)));
    OK(getFromSocket(std::format("/output create headless {}", RECONNECT_OUTPUT)));
    ASSERT(waitForOutput(true), true);

    CScopeGuard guard = {[&]() {
        Tests::killAllWindows();
        getFromSocket(std::format("/output remove {}", RECONNECT_OUTPUT));
        waitForOutput(false);
    }};

    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ monitor = '{}' }})", RECONNECT_OUTPUT)));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:reconnect_floating' })"));

    SPAWN_KITTY(RECONNECT_CLASS);
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 400, y = 300 })"));
    // move away from the center, so re-centering the window can't make this pass by accident
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = -300, y = -200, relative = true })"));
    Tests::sync();

    const auto ACTIVE = getFromSocket("/activewindow");
    ASSERT_CONTAINS(ACTIVE, std::format("class: {}", RECONNECT_CLASS));
    ASSERT_CONTAINS(ACTIVE, "floating: 1");
    ASSERT_CONTAINS(ACTIVE, "(reconnect_floating)");

    const auto POS_BEFORE = Tests::getAttribute(ACTIVE, "at");
    ASSERT(POS_BEFORE.empty(), false);
    // sanity check: the window really is on the output we're about to disconnect
    ASSERT(std::stoi(POS_BEFORE.substr(0, POS_BEFORE.find(','))) >= 1920, true);

    OK(getFromSocket(std::format("/output remove {}", RECONNECT_OUTPUT)));
    ASSERT(waitForOutput(false), true);
    Tests::sync();

    OK(getFromSocket(std::format("/output create headless {}", RECONNECT_OUTPUT)));
    ASSERT(waitForOutput(true), true);

    // the workspace (and the floating window on it) should go back to the reconnected output, at the same position.
    // Previously, it was moved relative to the output's unset (-1, -1) position and ended up outside of it.
    EXPECT(waitForFloatingWindowPosition(POS_BEFORE), true);
    EXPECT(floatingWindowPosition(), POS_BEFORE);

    const auto ACTIVE_AFTER = getFromSocket("/activewindow");
    EXPECT_CONTAINS(ACTIVE_AFTER, "(reconnect_floating)");
    EXPECT_CONTAINS(ACTIVE_AFTER, "floating: 1");
}
