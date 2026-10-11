#include "../../hyprctlCompat.hpp"
#include "tests.hpp"

#include <cstdint>
#include <format>
#include <linux/input-event-codes.h>

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;

static std::string keyboardKey(bool pressed, uint32_t key) {
    return getFromSocket(std::format("/eval hl.plugin.test.keybind({}, 0, {})", pressed ? 1 : 0, key + 8));
}

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

TEST_CASE(orderedChordConsumption) {
    CScopeGuard guard = {[&]() {
        EXPECT_OK(getFromSocket("/eval hl.unbind('TAB + T'); _G.hyprtester_chord_count = nil"));
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.remove_keyboard_event_recorder()"));
    }};

    OK(getFromSocket("/eval _G.hyprtester_chord_count = 0; "
                     "hl.bind('TAB + T', function() _G.hyprtester_chord_count = _G.hyprtester_chord_count + 1 end)"));
    OK(getFromSocket("/eval hl.plugin.test.register_keyboard_event_recorder()"));

    // The prefix is consumed even when the chord is not completed.
    EXPECT_OK(keyboardKey(true, KEY_TAB));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_keyboard_events()"));
    EXPECT_OK(keyboardKey(false, KEY_TAB));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(getFromSocket("/eval hl.plugin.test.expect_keyboard_events()"));

    // An unrelated key is forwarded while the prefix is held, without firing the bind.
    EXPECT_OK(keyboardKey(true, KEY_TAB));
    EXPECT_OK(keyboardKey(true, KEY_X));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1)", KEY_X)));
    EXPECT_OK(keyboardKey(false, KEY_X));
    EXPECT_OK(keyboardKey(false, KEY_TAB));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 0)", KEY_X, KEY_X)));
    OK(getFromSocket("/eval hl.plugin.test.register_keyboard_event_recorder()"));

    // A trailing key on its own is forwarded.
    EXPECT_OK(keyboardKey(true, KEY_T));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1)", KEY_T)));
    EXPECT_OK(keyboardKey(false, KEY_T));
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 0)", KEY_T, KEY_T)));

    // Completing the chord fires exactly once and consumes the final press.
    EXPECT_OK(keyboardKey(true, KEY_TAB));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(keyboardKey(true, KEY_T));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "1");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 0)", KEY_T, KEY_T)));
    EXPECT_OK(keyboardKey(false, KEY_T));
    EXPECT_OK(keyboardKey(false, KEY_TAB));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "1");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 0)", KEY_T, KEY_T)));
}

TEST_CASE(orderedChordRejectsReorderedPrefix) {
    CScopeGuard guard = {[&]() { EXPECT_OK(getFromSocket("/eval hl.unbind('A + B + C'); _G.hyprtester_chord_count = nil")); }};

    OK(getFromSocket("/eval _G.hyprtester_chord_count = 0; "
                     "hl.bind('A + B + C', function() _G.hyprtester_chord_count = _G.hyprtester_chord_count + 1 end)"));

    // Having all keys held is insufficient: the prefix must be pressed in order.
    EXPECT_OK(keyboardKey(true, KEY_B));
    EXPECT_OK(keyboardKey(true, KEY_A));
    EXPECT_OK(keyboardKey(true, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(keyboardKey(false, KEY_C));
    EXPECT_OK(keyboardKey(false, KEY_A));
    EXPECT_OK(keyboardKey(false, KEY_B));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");

    // A fresh, correctly ordered chord still works after the rejected attempt.
    EXPECT_OK(keyboardKey(true, KEY_A));
    EXPECT_OK(keyboardKey(true, KEY_B));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(keyboardKey(true, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "1");
    EXPECT_OK(keyboardKey(false, KEY_C));
    EXPECT_OK(keyboardKey(false, KEY_B));
    EXPECT_OK(keyboardKey(false, KEY_A));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "1");
}

TEST_CASE(orderedReleaseChord) {
    CScopeGuard guard = {[&]() { EXPECT_OK(getFromSocket("/eval hl.unbind('A + B + C'); _G.hyprtester_chord_count = nil")); }};

    OK(getFromSocket("/eval _G.hyprtester_chord_count = 0; "
                     "hl.bind('A + B + C', function() _G.hyprtester_chord_count = _G.hyprtester_chord_count + 1 end, { release = true })"));

    // An out-of-order prefix must not arm the release bind.
    EXPECT_OK(keyboardKey(true, KEY_B));
    EXPECT_OK(keyboardKey(true, KEY_A));
    EXPECT_OK(keyboardKey(true, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(keyboardKey(false, KEY_A));
    EXPECT_OK(keyboardKey(false, KEY_B));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(keyboardKey(false, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");

    // Releasing the prefix first must preserve a correctly armed release bind.
    EXPECT_OK(keyboardKey(true, KEY_A));
    EXPECT_OK(keyboardKey(true, KEY_B));
    EXPECT_OK(keyboardKey(true, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(keyboardKey(false, KEY_A));
    EXPECT_OK(keyboardKey(false, KEY_B));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(keyboardKey(false, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "1");
}

TEST_CASE(nonConsumingChordPreservesOrder) {
    CScopeGuard guard = {[&]() {
        EXPECT_OK(getFromSocket("/eval hl.unbind('A + B + C'); _G.hyprtester_chord_count = nil"));
        EXPECT_OK(getFromSocket("/eval hl.plugin.test.remove_keyboard_event_recorder()"));
    }};

    OK(getFromSocket("/eval _G.hyprtester_chord_count = 0; "
                     "hl.bind('A + B + C', function() _G.hyprtester_chord_count = _G.hyprtester_chord_count + 1 end, { non_consuming = true })"));
    OK(getFromSocket("/eval hl.plugin.test.register_keyboard_event_recorder()"));

    EXPECT_OK(keyboardKey(true, KEY_B));
    EXPECT_OK(keyboardKey(true, KEY_A));
    EXPECT_OK(keyboardKey(true, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 1, {}, 1)", KEY_B, KEY_A, KEY_C)));
    EXPECT_OK(keyboardKey(false, KEY_C));
    EXPECT_OK(keyboardKey(false, KEY_A));
    EXPECT_OK(keyboardKey(false, KEY_B));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");

    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 1, {}, 1, {}, 0, {}, 0, {}, 0)", KEY_B, KEY_A, KEY_C, KEY_C, KEY_A, KEY_B)));

    // Reset only after releasing all keys: the recorder owns forwarded keys until release.
    OK(getFromSocket("/eval hl.plugin.test.register_keyboard_event_recorder()"));
    EXPECT_OK(keyboardKey(true, KEY_A));
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1)", KEY_A)));
    EXPECT_OK(keyboardKey(true, KEY_B));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "0");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 1)", KEY_A, KEY_B)));
    EXPECT_OK(keyboardKey(true, KEY_C));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "1");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 1, {}, 1)", KEY_A, KEY_B, KEY_C)));
    EXPECT_OK(keyboardKey(false, KEY_C));
    EXPECT_OK(keyboardKey(false, KEY_B));
    EXPECT_OK(keyboardKey(false, KEY_A));
    EXPECT(getFromSocket("/repl return _G.hyprtester_chord_count"), "1");
    EXPECT_OK(getFromSocket(std::format("/eval hl.plugin.test.expect_keyboard_events({}, 1, {}, 1, {}, 1, {}, 0, {}, 0, {}, 0)", KEY_A, KEY_B, KEY_C, KEY_C, KEY_B, KEY_A)));
}
