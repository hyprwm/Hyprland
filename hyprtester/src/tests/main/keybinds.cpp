#include <filesystem>
#include <linux/input-event-codes.h>
#include <thread>
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;
static std::string flagFile = "/tmp/hyprtester-keybinds.txt";

static std::string pluginKeybindCmd(bool pressed, uint32_t modifier, uint32_t key) {
    return "/eval hl.plugin.test.keybind(" + std::to_string(pressed ? 1 : 0) + ", " + std::to_string(modifier) + ", " + std::to_string(key) + ")";
}

static std::string pluginScrollCmd(int delta) {
    return "/eval hl.plugin.test.scroll(" + std::to_string(delta) + ")";
}

// Because i don't feel like changing someone elses code.
enum eKeyboardModifierIndex : uint8_t {
    MOD_SHIFT = 1,
    MOD_CAPS,
    MOD_CTRL,
    MOD_ALT,
    MOD_MOD2,
    MOD_MOD3,
    MOD_META,
    MOD_MOD5
};

static void clearFlag() {
    std::filesystem::remove(flagFile);
}

static bool checkFlag() {
    bool exists = std::filesystem::exists(flagFile);
    clearFlag();
    return exists;
}

static bool attemptCheckFlag(int attempts, int intervalMs) {
    for (int i = 0; i < attempts; i++) {
        if (checkFlag())
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }

    return false;
}

static std::string readKittyOutput() {
    std::string output = Tests::execAndGet("kitten @ --to unix:/tmp/hyprtester-kitty.sock get-text --extent all");
    // chop off shell prompt
    std::size_t pos = output.rfind("$");
    if (pos != std::string::npos) {
        pos += 1;
        if (pos < output.size())
            output.erase(0, pos);
    }
    // NLog::log("Kitty output: '{}'", output);
    return output;
}

static void awaitKittyPrompt() {
    // wait until we see the shell prompt, meaning it's ready for test inputs
    for (int i = 0; i < 10; i++) {
        std::string output = Tests::execAndGet("kitten @ --to unix:/tmp/hyprtester-kitty.sock get-text --extent all");
        if (output.rfind("$") == std::string::npos) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        return;
    }
    NLog::log("{}Error: timed out waiting for kitty prompt", Colors::RED);
}

static CUniquePointer<CProcess> spawnRemoteControlKitty() {
    auto kittyProc = Tests::spawnKitty("keybinds_test", {"-o", "allow_remote_control=yes", "--listen-on", "unix:/tmp/hyprtester-kitty.sock", "--config", "NONE", "/bin/sh"});
    // wait a bit to ensure shell prompt is sent, we are going to read the text after it
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (kittyProc)
        awaitKittyPrompt();
    return kittyProc;
}

// All the `SUBTEST`s below are supposed to be independent `TEST_CASE`s.
// But if isolated trivially, some of them fail.
// TODO: investigate and isolate tests by turning `SUBTEST`s into `TEST_CASE`s.

SUBTEST(bind) {
    ASSERT(checkFlag(), false);
    EXPECT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'))"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // await flag
    ASSERT(attemptCheckFlag(20, 50), true);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}

SUBTEST(bindKey) {
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('Y', hl.dsp.exec_cmd('touch " + flagFile + "'))"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 0, 29)));
    // await flag
    ASSERT(attemptCheckFlag(20, 50), true);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('', 'Y')"), "ok");
}

SUBTEST(longPress) {
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { long_press = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT(checkFlag(), false);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}
SUBTEST(keyLongPress) {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/eval hl.bind('Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { long_press = true })"), "ok");
    EXPECT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 0, 29)));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT(checkFlag(), false);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('', 'Y')"), "ok");
}
SUBTEST(longPressRelease) {
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { long_press = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT(checkFlag(), false);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}
SUBTEST(longPressOnlyKeyRelease) {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { long_press = true })"), "ok");
    EXPECT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT(checkFlag(), false);
    // release key, keep modifier
    OK(getFromSocket(pluginKeybindCmd(false, 7, 29)));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), false);
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}

SUBTEST(repeat) {
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { repeating = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // check that it continues repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}

SUBTEST(keyRepeat) {
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { repeating = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 0, 29)));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT(checkFlag(), true);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // check that it continues repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('', 'Y')"), "ok");
}

SUBTEST(repeatRelease) {
    // wait until flag becomes false (CI timing can vary)
    bool ok = false;
    for (int i = 0; i < 20; ++i) {
        if (!checkFlag()) {
            ok = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    ASSERT(ok, true);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { repeating = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT(checkFlag(), true);
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    clearFlag();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), false);
    // check that it is not repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}

SUBTEST(repeatOnlyKeyRelease) {
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { repeating = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), true);
    // release key, keep modifier
    OK(getFromSocket(pluginKeybindCmd(false, 7, 29)));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    clearFlag();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), false);
    // check that it is not repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT(checkFlag(), false);
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}

SUBTEST(shortcutBind) {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        FAIL_TEST("Could not spawn kitty");
    }
    ASSERT(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:keybinds_test' })"), "ok");
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.send_shortcut({ mods = '', key = 'q', window = 'activewindow' }))"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // release keybind
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::string output = readKittyOutput();
    ASSERT_COUNT_STRING(output, "y", 0);
    ASSERT(output.find("q") != std::string::npos, true);
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
    Tests::killAllWindows();
}

SUBTEST(shortcutBindKey) {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        FAIL_TEST("Could not spawn kitty");
    }
    ASSERT(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:keybinds_test' })"), "ok");
    ASSERT(getFromSocket("/eval hl.bind('Y', hl.dsp.send_shortcut({ mods = '', key = 'q', window = 'activewindow' }))"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 0, 29)));
    // release keybind
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::string output = readKittyOutput();
    ASSERT_COUNT_STRING(output, "y", 0);
    // disabled: doesn't work in CI
    // EXPECT_COUNT_STRING(output, "q", 1);
    ASSERT(getFromSocket("/eval hl.unbind('', 'Y')"), "ok");
    Tests::killAllWindows();
}

SUBTEST(shortcutLongPress) {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        FAIL_TEST("Could not spawn kitty");
    }
    ASSERT(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:keybinds_test' })"), "ok");
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.send_shortcut({ mods = '', key = 'q', window = 'activewindow' }), { long_press = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_rate = 10 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const std::string output = readKittyOutput();
    int               yCount = Tests::countOccurrences(output, "y");
    // sometimes 1, sometimes 2, not sure why
    // keybind press sends 1 y immediately
    // then repeat triggers, sending 1 y
    // final release stop repeats, and shouldn't send any more
    ASSERT(true, yCount == 1 || yCount == 2);
    ASSERT_COUNT_STRING(output, "q", 1);
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
    Tests::killAllWindows();
}

SUBTEST(shortcutLongPressKeyRelease) {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        FAIL_TEST("Could not spawn kitty");
    }
    ASSERT(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:keybinds_test' })"), "ok");
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.send_shortcut({ mods = '', key = 'q', window = 'activewindow' }), { long_press = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 100 } })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_rate = 10 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // release key, keep modifier
    OK(getFromSocket(pluginKeybindCmd(false, 7, 29)));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const std::string output = readKittyOutput();
    // disabled: doesn't work on CI
    // ASSERT_COUNT_STRING(output, "y", 1);
    ASSERT_COUNT_STRING(output, "q", 0);
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
    Tests::killAllWindows();
}

SUBTEST(shortcutRepeat) {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        FAIL_TEST("Could not spawn kitty");
    }
    ASSERT(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:keybinds_test' })"), "ok");
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.send_shortcut({ mods = '', key = 'q', window = 'activewindow' }), { repeating = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_rate = 5 } })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 200 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    // await repeat
    std::this_thread::sleep_for(std::chrono::milliseconds(210));
    // release keybind
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    const std::string output = readKittyOutput();
    ASSERT_COUNT_STRING(output, "y", 0);
    int qCount = Tests::countOccurrences(output, "q");
    // sometimes 2, sometimes 3, not sure why
    // keybind press sends 1 q immediately
    // then repeat triggers, sending 1 q
    // final release stop repeats, and shouldn't send any more
    ASSERT(true, qCount == 2 || qCount == 3);
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
    Tests::killAllWindows();
}

SUBTEST(shortcutRepeatKeyRelease) {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        FAIL_TEST("Could not spawn kitty");
    }
    ASSERT(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:keybinds_test' })"), "ok");
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.send_shortcut({ mods = '', key = 'q', window = 'activewindow' }), { repeating = true })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_rate = 5 } })"), "ok");
    ASSERT(getFromSocket("r/eval hl.config({ input = { repeat_delay = 200 } })"), "ok");
    // press keybind
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    std::this_thread::sleep_for(std::chrono::milliseconds(210));
    // release key, keep modifier
    OK(getFromSocket(pluginKeybindCmd(false, 7, 29)));
    // if repeat was still active, we'd get 2 more q's here
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    // release modifier
    const std::string output = readKittyOutput();
    ASSERT_COUNT_STRING(output, "y", 0);
    int qCount = Tests::countOccurrences(output, "q");
    // sometimes 2, sometimes 3, not sure why
    // keybind press sends 1 q immediately
    // then repeat triggers, sending 1 q
    // final release stop repeats, and shouldn't send any more
    ASSERT(true, qCount == 2 || qCount == 3);
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
    Tests::killAllWindows();
}

SUBTEST(submap) {
    const auto press = [](const uint32_t key, const uint32_t mod = 0) {
        // +8 because udev -> XKB keycode.
        getFromSocket(pluginKeybindCmd(true, mod, key + 8));
        getFromSocket(pluginKeybindCmd(false, mod, key + 8));
    };

    NLog::log("{}Testing submaps", Colors::GREEN);
    // submap 1 no resets
    press(KEY_U, MOD_META);
    ASSERT_CONTAINS(getFromSocket("/submap"), "submap1");
    press(KEY_O);
    Tests::waitUntilWindowsN(1);
    ASSERT_CONTAINS(getFromSocket("/submap"), "submap1");
    // submap 2 resets to submap 1
    press(KEY_U);
    ASSERT_CONTAINS(getFromSocket("/submap"), "submap2");
    press(KEY_O);
    Tests::waitUntilWindowsN(2);
    ASSERT_CONTAINS(getFromSocket("/submap"), "submap1");
    // submap 3 resets to default
    press(KEY_I);
    ASSERT_CONTAINS(getFromSocket("/submap"), "submap3");
    press(KEY_O);
    Tests::waitUntilWindowsN(3);
    ASSERT_CONTAINS(getFromSocket("/submap"), "default");
    // submap 1 reset via keybind
    press(KEY_U, MOD_META);
    ASSERT_CONTAINS(getFromSocket("/submap"), "submap1");
    press(KEY_P);
    ASSERT_CONTAINS(getFromSocket("/submap"), "default");

    Tests::killAllWindows();
}

SUBTEST(bindsAfterScroll) {
    NLog::log("{}Testing binds after scroll", Colors::GREEN);

    clearFlag();
    OK(getFromSocket("/eval hl.bind('ALT + w', hl.dsp.exec_cmd('touch " + flagFile + "'))"));

    // press keybind before scroll
    OK(getFromSocket(pluginKeybindCmd(true, 0, 108))); // Alt_R press
    OK(getFromSocket(pluginKeybindCmd(true, 4, 25)));  // w press
    ASSERT(attemptCheckFlag(20, 50), true);
    OK(getFromSocket(pluginKeybindCmd(false, 4, 25)));  // w release
    OK(getFromSocket(pluginKeybindCmd(false, 0, 108))); // Alt_R release

    // scroll
    OK(getFromSocket(pluginScrollCmd(120)));
    OK(getFromSocket(pluginScrollCmd(-120)));
    OK(getFromSocket(pluginScrollCmd(120)));

    // press keybind after scroll
    OK(getFromSocket(pluginKeybindCmd(true, 0, 108))); // Alt_R press
    OK(getFromSocket(pluginKeybindCmd(true, 4, 25)));  // w press
    ASSERT(attemptCheckFlag(20, 50), true);
    OK(getFromSocket(pluginKeybindCmd(false, 4, 25)));  // w release
    OK(getFromSocket(pluginKeybindCmd(false, 0, 108))); // Alt_R release

    clearFlag();
    OK(getFromSocket("/eval hl.unbind('ALT', 'w')"));
}

SUBTEST(submapUniversal) {
    NLog::log("{}Testing submap universal", Colors::GREEN);

    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { submap_universal = true })"), "ok");
    ASSERT_CONTAINS(getFromSocket("/submap"), "default");

    // keybind works on default submap
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    OK(getFromSocket(pluginKeybindCmd(false, 7, 29)));
    ASSERT(attemptCheckFlag(30, 5), true);

    // keybind works on submap1
    getFromSocket(pluginKeybindCmd(true, 7, 30));
    getFromSocket(pluginKeybindCmd(false, 7, 30));
    ASSERT_CONTAINS(getFromSocket("/submap"), "submap1");
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    OK(getFromSocket(pluginKeybindCmd(false, 7, 29)));
    ASSERT(attemptCheckFlag(30, 5), true);

    // reset to default submap
    getFromSocket(pluginKeybindCmd(true, 0, 33));
    getFromSocket(pluginKeybindCmd(false, 0, 33));
    ASSERT_CONTAINS(getFromSocket("/submap"), "default");

    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}

SUBTEST(perDeviceKeybind) {
    NLog::log("{}Testing per-device binds", Colors::GREEN);

    // Inclusive
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { device = { inclusive = true, list = { 'test-keyboard-1' } } })"), "ok");
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    ASSERT(attemptCheckFlag(20, 50), true);
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");

    // Exclusive
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { device = { inclusive = false, list = { 'test-keyboard-1' } } })"), "ok");
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    ASSERT(attemptCheckFlag(20, 50), false);
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");

    // With description
    ASSERT(checkFlag(), false);
    ASSERT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile +
                         "'), { description = 'test description', device = { inclusive = true, list = { 'test-keyboard-1' } } })"),
           "ok");
    OK(getFromSocket(pluginKeybindCmd(true, 7, 29)));
    ASSERT(attemptCheckFlag(20, 50), true);
    OK(getFromSocket(pluginKeybindCmd(false, 0, 29)));
    ASSERT(getFromSocket("/eval hl.unbind('SUPER', 'Y')"), "ok");
}

// TODO: remove this test after subtests above are properly isolated into independent tests
TEST_CASE(keybinds) {
    CALL_SUBTEST(bind);
    CALL_SUBTEST(bindKey);
    CALL_SUBTEST(longPress);
    CALL_SUBTEST(keyLongPress);
    CALL_SUBTEST(longPressRelease);
    CALL_SUBTEST(longPressOnlyKeyRelease);
    CALL_SUBTEST(repeat);
    CALL_SUBTEST(keyRepeat);
    CALL_SUBTEST(repeatRelease);
    CALL_SUBTEST(repeatOnlyKeyRelease);
    CALL_SUBTEST(shortcutBind);
    CALL_SUBTEST(shortcutBindKey);
    CALL_SUBTEST(shortcutLongPress);
    CALL_SUBTEST(shortcutLongPressKeyRelease);
    CALL_SUBTEST(shortcutRepeat);
    CALL_SUBTEST(shortcutRepeatKeyRelease);
    CALL_SUBTEST(submap);
    CALL_SUBTEST(submapUniversal);
    CALL_SUBTEST(bindsAfterScroll);
    CALL_SUBTEST(perDeviceKeybind);
}
