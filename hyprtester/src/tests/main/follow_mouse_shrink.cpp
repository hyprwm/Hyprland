#include <chrono>
#include <cstring>
#include <thread>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

static int  ret = 0;

static bool spawnKitty(const std::string& class_) {
    NLog::log("{}Spawning {}", Colors::YELLOW, class_);
    if (!Tests::spawnKitty(class_)) {
        NLog::log("{}Error: {} did not spawn", Colors::RED, class_);
        return false;
    }
    return true;
}

static bool isActiveWindow(const std::string& class_, char fullscreen = '0', bool log = true) {
    std::string activeWin     = getFromSocket("/activewindow");
    auto        winClass      = Tests::getWindowAttribute(activeWin, "class:");
    auto        winFullscreen = Tests::getWindowAttribute(activeWin, "fullscreen:").back();
    if (winClass.substr(strlen("class: ")) == class_ && winFullscreen == fullscreen)
        return true;
    else {
        if (log)
            NLog::log("{}Wrong active window: expected class {} fullscreen '{}', found class {}, fullscreen '{}'", Colors::RED, class_, fullscreen, winClass, winFullscreen);
        return false;
    }
}

static bool waitForActiveWindow(const std::string& class_, char fullscreen = '0', bool logLastCheck = true, int maxTries = 50) {
    int cnt = 0;
    while (!isActiveWindow(class_, fullscreen, false)) {
        ++cnt;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (cnt > maxTries) {
            return isActiveWindow(class_, fullscreen, logLastCheck);
        }
    }
    return true;
}

static bool test() {
    NLog::log("{}Testing follow_mouse_shrink", Colors::GREEN);

    getFromSocket("/dispatch workspace name:follow_mouse_shrink");

    // Setup: follow_mouse 1 so cursor movement changes focus, float_switch_override_focus 1
    OK(getFromSocket("/keyword input:follow_mouse 1"));
    OK(getFromSocket("/keyword input:float_switch_override_focus 1"));

    // Spawn two floating windows with a 20px gap
    // fms_a: position (100,100), size 400x400 -> hitbox [100,499] x [100,499]
    if (!spawnKitty("fms_a"))
        return false;
    OK(getFromSocket("/dispatch setfloating"));
    OK(getFromSocket("/dispatch resizewindowpixel exact 400 400,activewindow"));
    OK(getFromSocket("/dispatch movewindowpixel exact 100 100,activewindow"));

    // fms_b: position (520,100), size 400x400 -> hitbox [520,919] x [100,499]
    if (!spawnKitty("fms_b"))
        return false;
    OK(getFromSocket("/dispatch setfloating"));
    OK(getFromSocket("/dispatch resizewindowpixel exact 400 400,activewindow"));
    OK(getFromSocket("/dispatch movewindowpixel exact 520 100,activewindow"));

    // --- Test 1: Baseline shrink=0, edge focus works ---
    NLog::log("{}Test 1: shrink=0, cursor at B's left edge focuses B", Colors::GREEN);
    OK(getFromSocket("/keyword input:follow_mouse_shrink 0"));
    // Focus A first
    OK(getFromSocket("/dispatch movecursor 300 300"));
    EXPECT(waitForActiveWindow("fms_a"), true);
    // Move to just inside B's left edge
    OK(getFromSocket("/dispatch movecursor 521 300"));
    EXPECT(waitForActiveWindow("fms_b"), true);

    // --- Test 2: Shrink=20, cursor in dead zone does NOT change focus ---
    NLog::log("{}Test 2: shrink=20, cursor in B's dead zone stays on A", Colors::GREEN);
    OK(getFromSocket("/keyword input:follow_mouse_shrink 20"));
    // Focus A
    OK(getFromSocket("/dispatch movecursor 300 300"));
    EXPECT(waitForActiveWindow("fms_a"), true);
    // Move to 530,300 -- 10px inside B, within 20px shrink zone (B's shrunk hitbox starts at 540)
    OK(getFromSocket("/dispatch movecursor 530 300"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT(isActiveWindow("fms_a"), true);

    // --- Test 3: Shrink=20, cursor well inside inactive window DOES focus it ---
    NLog::log("{}Test 3: shrink=20, cursor at B's center focuses B", Colors::GREEN);
    // Still focused on A from test 2
    OK(getFromSocket("/dispatch movecursor 720 300"));
    EXPECT(waitForActiveWindow("fms_b"), true);

    // --- Test 4: Focused window's hitbox is NOT shrunk ---
    NLog::log("{}Test 4a: focused window hitbox is not shrunk", Colors::GREEN);
    // Focus A, move near A's right edge (490,300). A's shrunk box would end at 479, but A is focused so not shrunk.
    OK(getFromSocket("/dispatch movecursor 300 300"));
    EXPECT(waitForActiveWindow("fms_a"), true);
    OK(getFromSocket("/dispatch movecursor 490 300"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT(isActiveWindow("fms_a"), true);

    NLog::log("{}Test 4b: inactive window hitbox IS shrunk at same position", Colors::GREEN);
    // Focus B first, then move to (490,300). Now A is inactive with shrunk box ending at 479, so 490 is outside A.
    OK(getFromSocket("/dispatch movecursor 720 300"));
    EXPECT(waitForActiveWindow("fms_b"), true);
    OK(getFromSocket("/dispatch movecursor 490 300"));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT(isActiveWindow("fms_b"), true);

    // Cleanup
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    return ret == 0;
}

REGISTER_TEST_FN(test)
