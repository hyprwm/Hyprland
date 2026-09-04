#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <cstdint>
#include <format>
#include <linux/input-event-codes.h>

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

TEST_CASE(pluginKeyboardEventHandler) {
    constexpr uint32_t KEY     = KEY_F24;
    constexpr uint32_t XKB_KEY = KEY + 8;

    CScopeGuard        guard = {[&]() { OK(getFromSocket("/eval hl.plugin.test.remove_keyboard_event_recorder()")); }};

    OK(getFromSocket("/eval hl.plugin.test.remove_keyboard_event_recorder()"));
    OK(getFromSocket("/eval hl.plugin.test.register_keyboard_event_recorder()"));

    OK(getFromSocket(std::format("/eval hl.plugin.test.keybind(1, 0, {})", XKB_KEY)));
    OK(getFromSocket(std::format("/eval hl.plugin.test.keybind(0, 0, {})", XKB_KEY)));
    OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 0)", KEY, KEY)));

    OK(getFromSocket("/eval hl.plugin.test.remove_keyboard_event_recorder()"));
    OK(getFromSocket(std::format("/eval hl.plugin.test.keybind(1, 0, {})", XKB_KEY)));
    OK(getFromSocket(std::format("/eval hl.plugin.test.keybind(0, 0, {})", XKB_KEY)));
    OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 0)", KEY, KEY)));
}
