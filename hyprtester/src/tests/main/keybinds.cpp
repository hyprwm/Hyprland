#include "../../Log.hpp"
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <filesystem>
#include <thread>

static int ret = 0;
static std::string flagFile = "/tmp/hyprtester-keybinds.txt";

static void clearFlag() {
    std::filesystem::remove(flagFile);
}

static bool checkFlag() {
    bool exists = std::filesystem::exists(flagFile);
    clearFlag();
    return exists;
}

static void testBind() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bind SUPER,Y,exec,touch " + flagFile), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT(checkFlag(), false);
    // release key, keep modifier
    OK(getFromSocket("/dispatch plugin:test:keybind 0,7,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static void testLongPressOnlyModifierRelease() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bindo SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // check no flag on short press
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT(checkFlag(), false);
    // release modifier, keep key
    OK(getFromSocket("/dispatch plugin:test:keybind 1,0,29"));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
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

static void testRepeatOnlyModifierRelease() {
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword binde SUPER,Y,exec,touch " + flagFile), "ok");
    EXPECT(getFromSocket("/keyword input:repeat_delay 100"), "ok");
    // press keybind
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    // await flag
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT(checkFlag(), true);
    // release modifier, keep key
    OK(getFromSocket("/dispatch plugin:test:keybind 1,0,29"));
    // await repeat delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    // check that it is not repeating
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT(checkFlag(), false);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");
}

static bool test() {
    NLog::log("{}Testing keybinds", Colors::GREEN);

    testBind();
    testBindKey();
    testLongPress();
    testKeyLongPress();
    testLongPressRelease();
    testLongPressOnlyKeyRelease();
    testLongPressOnlyModifierRelease();
    testRepeat();
    testKeyRepeat();
    testRepeatRelease();
    testRepeatOnlyKeyRelease();
    testRepeatOnlyModifierRelease();
    
    clearFlag();
    return !ret;
}

REGISTER_TEST_FN(test)
