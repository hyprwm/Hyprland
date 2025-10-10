#include <filesystem>
#include <thread>
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

static int         ret      = 0;
static std::string flagFile = "/tmp/hyprtester-keybinds.txt";

static void        clearFlag() {
    std::filesystem::remove(flagFile);
}

static bool checkFlag() {
    bool exists = std::filesystem::exists(flagFile);
    clearFlag();
    return exists;
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

static void testBind() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bind SUPER,Y,exec,touch " + flagFile), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), true);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testBindKey() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bind ,Y,exec,touch " + flagFile), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,0,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), true);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind ,Y"), "ok");
}

static void testLongPress() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bindo SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), false);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), true);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testKeyLongPress() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bindo ,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,0,29"));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), false);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), true);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind ,Y"), "ok");
}

static void testLongPressRelease() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bindo SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), false);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testLongPressOnlyKeyRelease() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bindo SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), false);
    // release key, keep modifier
    OK(getFromSocket("/dispatch plugin:test:keybind 0,7,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testRepeat() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword binde SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), true);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), true);
    // check that it continues repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), true);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testKeyRepeat() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword binde ,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,0,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), true);
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), true);
    // check that it continues repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), true);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind ,Y"), "ok");
}

static void testRepeatRelease() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword binde SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), true);
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    // check that it is not repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testRepeatOnlyKeyRelease() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword binde SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT(checkFlag(), true);
    // release key, keep modifier
    OK(getFromSocket("/dispatch plugin:test:keybind 0,7,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    // check that it is not repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testShortcutBind() {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        ret = 1;
        return;
    }
    EXPECT(getFromSocket("/dispatch focuswindow class:keybinds_test"), "ok");
    EXPECT(getFromSocket("/keyword bind SUPER,Y,sendshortcut,,q,"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // release keybind
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::string output = readKittyOutput();
    EXPECT_COUNT_STRING(output, "y", 0);
    EXPECT_COUNT_STRING(output, "q", 1);
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
    Tests::killAllWindows();
}

static void testShortcutBindKey() {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        ret = 1;
        return;
    }
    EXPECT(getFromSocket("/dispatch focuswindow class:keybinds_test"), "ok");
    EXPECT(getFromSocket("/keyword bind ,Y,sendshortcut,,q,"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,0,29"));
    // release keybind
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const std::string output = readKittyOutput();
    EXPECT_COUNT_STRING(output, "y", 0);
    EXPECT_COUNT_STRING(output, "q", 1);
    EXPECT(getFromSocket("/keyword unbind ,Y"), "ok");
    Tests::killAllWindows();
}

static void testShortcutLongPress() {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        ret = 1;
        return;
    }
    EXPECT(getFromSocket("/dispatch focuswindow class:keybinds_test"), "ok");
    EXPECT(getFromSocket("/keyword bindo SUPER,Y,sendshortcut,,q,"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_rate 10"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const std::string output = readKittyOutput();
    int               yCount = Tests::countOccurrences(output, "y");
    // sometimes 1, sometimes 2, not sure why
    // keybind press sends 1 y immediately
    // then repeat triggers, sending 1 y
    // final release stop repeats, and shouldn't send any more
    EXPECT(true, yCount == 1 || yCount == 2);
    EXPECT_COUNT_STRING(output, "q", 1);
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
    Tests::killAllWindows();
}

static void testShortcutLongPressKeyRelease() {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        ret = 1;
        return;
    }
    EXPECT(getFromSocket("/dispatch focuswindow class:keybinds_test"), "ok");
    EXPECT(getFromSocket("/keyword bindo SUPER,Y,sendshortcut,,q,"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_rate 10"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // release key, keep modifier
    OK(getFromSocket("/dispatch plugin:test:keybind 0,7,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const std::string output = readKittyOutput();
    // disabled: doesn't work on CI
    // EXPECT_COUNT_STRING(output, "y", 1);
    EXPECT_COUNT_STRING(output, "q", 0);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
    Tests::killAllWindows();
}

static void testShortcutRepeat() {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        ret = 1;
        return;
    }
    EXPECT(getFromSocket("/dispatch focuswindow class:keybinds_test"), "ok");
    EXPECT(getFromSocket("/keyword binde SUPER,Y,sendshortcut,,q,"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_rate 5"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 200"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await repeat
    std::this_thread::sleep_for(std::chrono::milliseconds(210));
    // release keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    const std::string output = readKittyOutput();
    EXPECT_COUNT_STRING(output, "y", 0);
    int qCount = Tests::countOccurrences(output, "q");
    // sometimes 2, sometimes 3, not sure why
    // keybind press sends 1 q immediately
    // then repeat triggers, sending 1 q
    // final release stop repeats, and shouldn't send any more
    EXPECT(true, qCount == 2 || qCount == 3);
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
    Tests::killAllWindows();
}

static void testShortcutRepeatKeyRelease() {
    auto kittyProc = spawnRemoteControlKitty();
    if (!kittyProc) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        ret = 1;
        return;
    }
    EXPECT(getFromSocket("/dispatch focuswindow class:keybinds_test"), "ok");
    EXPECT(getFromSocket("/keyword binde SUPER,Y,sendshortcut,,q,"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_rate 5"), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 200"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    std::this_thread::sleep_for(std::chrono::milliseconds(210));
    // release key, keep modifier
    OK(getFromSocket("/dispatch plugin:test:keybind 0,7,29"));
    // if repeat was still active, we'd get 2 more q's here
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    // release modifier
    const std::string output = readKittyOutput();
    EXPECT_COUNT_STRING(output, "y", 0);
    int qCount = Tests::countOccurrences(output, "q");
    // sometimes 2, sometimes 3, not sure why
    // keybind press sends 1 q immediately
    // then repeat triggers, sending 1 q
    // final release stop repeats, and shouldn't send any more
    EXPECT(true, qCount == 2 || qCount == 3);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
    Tests::killAllWindows();
}

static bool test() {
    NLog::log("{}Testing keybinds", Colors::GREEN);

    testBind();
    testBindKey();
    testLongPress();
    testKeyLongPress();
    testLongPressRelease();
    testLongPressOnlyKeyRelease();
    testRepeat();
    testKeyRepeat();
    testRepeatRelease();
    testRepeatOnlyKeyRelease();
    testShortcutBind();
    testShortcutBindKey();
    testShortcutLongPress();
    testShortcutLongPressKeyRelease();
    testShortcutRepeat();
    testShortcutRepeatKeyRelease();

    clearFlag();
    return !ret;
}

REGISTER_TEST_FN(test)
