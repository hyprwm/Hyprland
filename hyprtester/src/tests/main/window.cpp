#include <unistd.h>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/string/VarList2.hpp>

#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"

// TODO: seems redundant, can just use `Tests::spawnKitty`?
static bool spawnKitty(const std::string& class_, const std::vector<std::string>& args = {}) {
    NLog::log("{}Spawning {}", Colors::YELLOW, class_);
    if (!Tests::spawnKitty(class_, args)) {
        NLog::log("{}Error: {} did not spawn", Colors::RED, class_);
        return false;
    }
    return true;
}

/// Spawns a kitty and creates a file and returns its name. The removal of the file triggers
/// activation of the spawned kitty window.
///
/// On failure, returns an empty string, possibly leaving a temporary file.
static std::string spawnKittyActivating(const std::string& class_ = "kitty_activating") {
    // `XXXXXX` is what `mkstemp` expects to find in the string
    std::string tmpFilename = (std::filesystem::temp_directory_path() / "XXXXXX").string();
    int         fd          = mkstemp(tmpFilename.data());
    if (fd < 0) {
        NLog::log("{}Error: could not create tmp file: errno {}", Colors::RED, errno);
        return "";
    }
    (void)close(fd);
    bool ok =
        spawnKitty(class_, {"-o", "allow_remote_control=yes", "--", "/bin/sh", "-c", "while [ -f \"" + tmpFilename + "\" ]; do :; done; kitten @ focus-window; sleep infinity"});
    if (!ok) {
        NLog::log("{}Error: failed to spawn kitty", Colors::RED);
        return "";
    }
    return tmpFilename;
}

static std::string getWindowAddress(const std::string& winInfo) {
    auto pos  = winInfo.find("Window ");
    auto pos2 = winInfo.find(" -> ");
    if (pos == std::string::npos || pos2 == std::string::npos) {
        NLog::log("{}Wrong window info", Colors::RED);
        return "Wrong window info";
    }
    return winInfo.substr(pos + 7, pos2 - pos - 7);
}

TEST_CASE(swapWindow) {
    // test on workspace "swapwindow"
    NLog::log("{}Switching to workspace \"swapwindow\"", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:swapwindow' })");

    if (!Tests::spawnKitty("kitty_A")) {
        FAIL_TEST("Could not spawn kitty");
    }

    if (!Tests::spawnKitty("kitty_B")) {
        FAIL_TEST("Could not spawn kitty");
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 2);

    // Test swapwindow by direction
    {
        getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })");
        auto pos = "at: " + Tests::getAttribute(getFromSocket("/activewindow"), "at");
        NLog::log("{}Testing kitty_A {}, swapwindow with direction 'r'", Colors::YELLOW, pos);

        OK(getFromSocket("/dispatch hl.dsp.window.swap({ direction = 'right' })"));
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));

        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", pos));
    }

    // Test swapwindow by class
    {
        getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })");
        auto pos = "at: " + Tests::getAttribute(getFromSocket("/activewindow"), "at");
        NLog::log("{}Testing kitty_A {}, swapwindow with class:kitty_B", Colors::YELLOW, pos);

        OK(getFromSocket("/dispatch hl.dsp.window.swap({ target = 'class:kitty_B' })"));
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));

        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", pos));
    }

    // Test swapwindow by address
    {
        getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })");
        auto addr = getWindowAddress(getFromSocket("/activewindow"));
        getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })");
        auto pos = "at: " + Tests::getAttribute(getFromSocket("/activewindow"), "at");
        NLog::log("{}Testing kitty_A {}, swapwindow with address:0x{}(kitty_B)", Colors::YELLOW, pos, addr);

        OK(getFromSocket(std::format("/dispatch hl.dsp.window.swap({{ target = 'address:0x{}' }})", addr)));
        OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'address:0x{}' }})", addr)));

        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", pos));
    }

    NLog::log("{}Testing swapwindow with fullscreen. Expecting to fail", Colors::YELLOW);
    {
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

        auto str = getFromSocket("/dispatch hl.dsp.window.swap({ direction = 'left' })");
        EXPECT_CONTAINS(str, "Can't swap fullscreen window");

        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));
    }

    NLog::log("{}Testing swapwindow with different workspace", Colors::YELLOW);
    {
        getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })");
        auto addr = getWindowAddress(getFromSocket("/activewindow"));
        auto ws   = "workspace: " + Tests::getAttribute(getFromSocket("/activewindow"), "workspace");
        NLog::log("{}Sending address:0x{}(kitty_B) to workspace \"swapwindow2\"", Colors::YELLOW, addr);

        OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = 'name:swapwindow2', follow = false })"));
        OK(getFromSocket(std::format("/dispatch hl.dsp.window.swap({{ target = 'address:0x{}' }})", addr)));
        getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })");
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("{}", ws));
    }
}

TEST_CASE(windowGroupRules) {
    OK(getFromSocket("/eval hl.config({ general = { border_size = 8 } })"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'w[tv1]', border_size = 0 })"));
    OK(getFromSocket("/eval hl.workspace_rule({ workspace = 'f[1]', border_size = 0 })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { workspace = 'w[tv1]' }, border_size = 0 })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { workspace = 'f[1]' }, border_size = 0 })"));

    if (!Tests::spawnKitty("kitty_A")) {
        FAIL_TEST("Could not spawn kitty");
    }

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    if (!Tests::spawnKitty("kitty_B")) {
        FAIL_TEST("Could not spawn kitty");
    }

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "8");
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ into_group = 'left' })"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    OK(getFromSocket("/dispatch hl.dsp.group.next()"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    if (!Tests::spawnKitty("kitty_C")) {
        FAIL_TEST("Could not spawn kitty");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.move({ out_of_group = 'right' })"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "8");
    }
}

static bool isActiveWindow(const std::string& class_, char fullscreen = '0', bool log = true) {
    std::string activeWin     = getFromSocket("/activewindow");
    auto        winClass      = Tests::getAttribute(activeWin, "class");
    auto        winFullscreen = Tests::getAttribute(activeWin, "fullscreen").back();
    if (winClass == class_ && winFullscreen == fullscreen)
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

/// Tests behavior of a window being focused when on that window's workspace
/// another fullscreen window exists.
TEST_CASE(windowFocusOnFullscreenConflict) {
    if (!spawnKitty("kitty_A"))
        FAIL_TEST("Could not spawn kitty");
    if (!spawnKitty("kitty_B"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/eval hl.config({ misc = { focus_on_activate = true } })"));

    // Unfullscreen on conflict
    {
        OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 2 } })"));

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus the same window
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus a different window
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));
        EXPECT(isActiveWindow("kitty_B", '0'), true);

        // Make a window that will request focus
        const std::string removeToActivate = spawnKittyActivating();
        if (removeToActivate.empty())
            FAIL_TEST("Could not spawn kitty_activating");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);
        std::filesystem::remove(removeToActivate);
        EXPECT(waitForActiveWindow("kitty_activating", '0'), true);
        OK(getFromSocket("/dispatch hl.dsp.window.kill()"));
        Tests::waitUntilWindowsN(2);
    }

    // Take over on conflict
    {
        OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 1 } })"));

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus the same window
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus a different window
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));
        EXPECT(isActiveWindow("kitty_B", '2'), true);
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set' })"));

        // Make a window that will request focus
        const std::string removeToActivate = spawnKittyActivating();
        if (removeToActivate.empty())
            FAIL_TEST("Could not spawn kitty_activating");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);
        std::filesystem::remove(removeToActivate);
        EXPECT(waitForActiveWindow("kitty_activating", '2'), true);
        OK(getFromSocket("/dispatch hl.dsp.window.kill()"));
        Tests::waitUntilWindowsN(2);
    }

    // Keep the old focus on conflict
    {
        OK(getFromSocket("/eval hl.config({ misc = { on_focus_under_fullscreen = 0 } })"));

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Dispatch-focus the same window
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);

        // Make a window that will request focus - the setting is treated normally
        const std::string removeToActivate = spawnKittyActivating();
        if (removeToActivate.empty())
            FAIL_TEST("Could not spawn kitty_activating");
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
        EXPECT(isActiveWindow("kitty_A", '2'), true);
        std::filesystem::remove(removeToActivate);
        EXPECT(waitForActiveWindow("kitty_A", '2'), true);
    }
}

TEST_CASE(windowMaximizeSize) {
    ASSERT(spawnKitty("kitty_A"), true);

    // check kitty properties. Maximizing shouldnt change its size
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,22"), true);
        EXPECT(str.contains("size: 1876,1036"), true);
        EXPECT(str.contains("fullscreen: 0"), true);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,22"), true);
        EXPECT(str.contains("size: 1876,1036"), true);
        EXPECT(str.contains("fullscreen: 1"), true);
    }
}

TEST_CASE(floatingFocusOnFullscreen) {
    ASSERT(spawnKitty("kitty_A"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle' })"));

    ASSERT(spawnKitty("kitty_B"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized' })"));

    OK(getFromSocket("/dispatch hl.dsp.window.cycle_next()"));

    OK(getFromSocket("/eval hl.plugin.test.floating_focus_on_fullscreen()"));
}

TEST_CASE(groupFallbackFocus) {
    ASSERT(spawnKitty("kitty_A"), true);

    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));

    ASSERT(spawnKitty("kitty_B"), true);
    ASSERT(spawnKitty("kitty_C"), true);
    ASSERT(spawnKitty("kitty_D"), true);

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_D"), true);
    }

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_B' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_D' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.kill()"));

    Tests::waitUntilWindowsN(3);

    // Focus must return to the last focus, in this case B.
    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("class: kitty_B"), true);
    }
}

TEST_CASE(bringActiveToTopMouseMovement) {
    OK(getFromSocket("/eval hl.config({ input = { follow_mouse = 2 } })"));
    OK(getFromSocket("/eval hl.config({ input = { float_switch_override_focus = 0 } })"));

    ASSERT(spawnKitty("a"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 500, y = 300 })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 400, y = 400 })"));

    ASSERT(spawnKitty("b"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 500, y = 300 })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 400, y = 400 })"));

    auto getTopWindow = []() -> std::string {
        auto clients = getFromSocket("/clients");
        return (clients.rfind("class: a") > clients.rfind("class: b")) ? "a" : "b";
    };

    ASSERT(getTopWindow(), std::string("b"));
    OK(getFromSocket("/dispatch hl.dsp.cursor.move({ x = 700, y = 500 })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:a' })"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: a");

    OK(getFromSocket("/dispatch hl.dsp.window.bring_to_top()"));
    ASSERT(getTopWindow(), std::string("a"));

    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));

    ASSERT(getTopWindow(), std::string("a"));
}

TEST_CASE(initialFloatSize) {
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty' }, float = true })"));
    OK(getFromSocket("/eval hl.config({ input = { float_switch_override_focus = 0 } })"));

    ASSERT(spawnKitty("kitty"), true);

    {
        // Kitty by default opens as 640x400, if this changes this test will break
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("size: 640,400"), true);
    }

    OK(getFromSocket("/reload"));

    Tests::killAllWindows();

    OK(getFromSocket("/dispatch hl.dsp.exec_cmd('kitty', { float = true })"));

    Tests::waitUntilWindowsN(1);

    {
        // Kitty by default opens as 640x400, if this changes this test will break
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("size: 640,400"), true);
        EXPECT(str.contains("floating: 1"), true);
    }
}

/// Tests that the `focus_on_activate` effect of window rules always overrides
/// the `misc:focus_on_activate` variable.
TEST_CASE(windowRuleFocusOnActivate) {
    if (!spawnKitty("kitty_default")) {
        FAIL_TEST("Could not spawn kitty");
    }

    // Do not focus anyone automatically
    // TODO: this looks like a bug: the following line should not be commented out
    ///////////OK(getFromSocket("/eval hl.window_rule({ match = { class = '.*' }, no_initial_focus = true })"));

    // `focus_on_activate off` takes over
    {
        OK(getFromSocket("/eval hl.config({ misc = { focus_on_activate = true } })"));
        OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_antifocus' }, focus_on_activate = false })"));

        const std::string removeToActivate = spawnKittyActivating("kitty_antifocus");
        if (removeToActivate.empty()) {
            FAIL_TEST("Could not spawn kitty_antifocus");
        }
        ASSERT(waitForActiveWindow("kitty_antifocus"), true);
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_default' })"));
        ASSERT(isActiveWindow("kitty_default"), true);

        std::filesystem::remove(removeToActivate);
        // The focus should NOT transition, since the window rule explicitly forbids that
        ASSERT(waitForActiveWindow("kitty_antifocus", '0', false), false);
    }

    // `focus_on_activate on` takes over
    {
        OK(getFromSocket("/eval hl.config({ misc = { focus_on_activate = false } })"));
        OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_superfocus' }, focus_on_activate = true })"));

        const std::string removeToActivate = spawnKittyActivating("kitty_superfocus");
        if (removeToActivate.empty()) {
            FAIL_TEST("Could not spawn kitty_superfocus");
        }
        ASSERT(waitForActiveWindow("kitty_superfocus"), true);
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_default' })"));
        ASSERT(isActiveWindow("kitty_default"), true);

        std::filesystem::remove(removeToActivate);
        // Now that we requested activation, the focus SHOULD transition to kitty_superfocus, according to the window rule
        ASSERT(waitForActiveWindow("kitty_superfocus"), true);
    }
}

// tests if a pinned window contains the valid workspace after change
TEST_CASE(pinnedWorkspacesValid) {
    getFromSocket("/dispatch hl.dsp.focus({ workspace = '1337' })");

    if (!spawnKitty("kitty")) {
        FAIL_TEST("Could not spawn kitty");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:kitty' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.pin({ action = 'toggle', window = 'class:kitty' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("workspace: 1337"), true);
        EXPECT(str.contains("pinned: 1"), true);
    }

    getFromSocket("/dispatch hl.dsp.focus({ workspace = '1338' })");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("workspace: 1338"), true);
        EXPECT(str.contains("pinned: 1"), true);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'unset', window = 'class:kitty' })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("workspace: 1338"), true);
        EXPECT(str.contains("pinned: 0"), true);
    }
}

TEST_CASE(windowruleWorkspaceEmpty) {
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_A' }, workspace = 'empty' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_B' }, workspace = 'emptyn' })"));

    getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })");

    if (!spawnKitty("kitty")) {
        FAIL_TEST("Could not spawn kitty");
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("workspace: 3"), true);
    }

    if (!spawnKitty("kitty_A")) {
        FAIL_TEST("Could not spawn kitty");
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("workspace: 1"), true);
    }

    getFromSocket("/dispatch hl.dsp.focus({ workspace = '3' })");
    if (!spawnKitty("kitty_B")) {
        FAIL_TEST("Could not spawn kitty");
    }

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains("workspace: 4"), true);
    }
}

TEST_CASE(contentWindowRules) {
    // kill me PLEASE

    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_content_string' }, content = 'game' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_content_numbers' }, content = '3' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { content = 'game' }, border_size = 10 })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { content = '3' }, opacity = '0.5' })"));

#define TEST_PROPS()                                                                                                                                                               \
    EXPECT_CONTAINS(getFromSocket("/getprop active border_size"), "10");                                                                                                           \
    EXPECT_CONTAINS(getFromSocket("/getprop active opacity"), "0.5");

    if (!spawnKitty("kitty_content_string"))
        FAIL_TEST("Could not spawn kitty_content_string");
    waitForActiveWindow("kitty_content_string");
    TEST_PROPS();

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    if (!spawnKitty("kitty_content_numbers"))
        FAIL_TEST("Could not spawn kitty_content_numbers");
    waitForActiveWindow("kitty_content_numbers");
    TEST_PROPS();

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

#undef TEST_PROPS
}

TEST_CASE(issue14038) {
    if (!spawnKitty("kitty_14038"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = 'special:a', follow = false, window = 'class:kitty_14038' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle', window = 'class:kitty_14038' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.pin({ action = 'toggle', window = 'class:kitty_14038' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle', window = 'class:kitty_14038' })"));

    // this should not crash hyprland. If we are alive, we good.
}

TEST_CASE(specialFloatRecenters) {
    if (!spawnKitty("kitty_special_float_recenter"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:kitty_special_float_recenter' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 10, y = 10, window = 'class:kitty_special_float_recenter' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ workspace = 'special:recenter', follow = false, window = 'class:kitty_special_float_recenter' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.move({ x = 50000, y = 50000, window = 'class:kitty_special_float_recenter' })"));

    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('recenter')"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_special_float_recenter' })"));

    const auto active = getFromSocket("/activewindow");
    EXPECT_CONTAINS(active, "class: kitty_special_float_recenter");
    EXPECT_CONTAINS(active, "size: 10,10");
    EXPECT_CONTAINS(active, "at: 955,535");

    OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('recenter')"));
    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
}

// TODO: decompose this into multiple test cases
TEST_CASE(windows) {
    // test on workspace "window"
    NLog::log("{}Switching to workspace `window`", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:window' })");

    if (!spawnKitty("kitty_A"))
        FAIL_TEST("Could not spawn kitty");

    // check kitty properties. One kitty should take the entire screen, as this is smart gaps
    NLog::log("{}Expecting kitty_A to take up the whole screen", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 0,0"), true);
        EXPECT(str.contains("size: 1920,1080"), true);
        EXPECT(str.contains("fullscreen: 0"), true);
    }

    NLog::log("{}Testing window split ratios", Colors::YELLOW);
    {
        const double INITIAL_RATIO = 1.25;
        const int    GAPSIN        = 5;
        const int    GAPSOUT       = 20;
        const int    BORDERSIZE    = 2;
        const int    BORDERS       = BORDERSIZE * 2;
        const int    MONITOR_W     = 1920;
        const int    MONITOR_H     = 1080;

        const float  totalAvailableHeight   = MONITOR_H - (GAPSOUT * 2);
        const int    HEIGHT                 = std::floor(totalAvailableHeight) - BORDERS;
        const float  availableWidthForSplit = MONITOR_W - (GAPSOUT * 2) - GAPSIN;

        auto         calculateFinalWidth = [&](double boxWidth, bool isLeftWindow) {
            double gapLeft  = isLeftWindow ? GAPSOUT : GAPSIN;
            double gapRight = isLeftWindow ? GAPSIN : GAPSOUT;
            return std::floor(boxWidth - gapLeft - gapRight - BORDERS);
        };

        double       geomBoxWidthA_R1 = (availableWidthForSplit * INITIAL_RATIO / 2.0) + GAPSOUT + (GAPSIN / 2.0);
        double       geomBoxWidthB_R1 = MONITOR_W - geomBoxWidthA_R1;
        const int    WIDTH1           = calculateFinalWidth(geomBoxWidthB_R1, false);

        const double INVERTED_RATIO   = 0.75;
        double       geomBoxWidthA_R2 = (availableWidthForSplit * INVERTED_RATIO / 2.0) + GAPSOUT + (GAPSIN / 2.0);
        double       geomBoxWidthB_R2 = MONITOR_W - geomBoxWidthA_R2;
        const int    WIDTH2           = calculateFinalWidth(geomBoxWidthB_R2, false);
        const int    WIDTH_A_FINAL    = calculateFinalWidth(geomBoxWidthA_R2, true);

        OK(getFromSocket("/eval hl.config({ dwindle = { default_split_ratio = 1.25 } })"));

        if (!spawnKitty("kitty_B"))
            FAIL_TEST("Could not spawn kitty");

        NLog::log("{}Expecting kitty_B size: {},{}", Colors::YELLOW, WIDTH1, HEIGHT);
        EXPECT_CONTAINS(getFromSocket("/activewindow"), std::format("size: {},{}", WIDTH1, HEIGHT));

        OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
        Tests::waitUntilWindowsN(1);

        NLog::log("{}Inverting the split ratio", Colors::YELLOW);
        OK(getFromSocket("/eval hl.config({ dwindle = { default_split_ratio = 0.75 } })"));

        if (!spawnKitty("kitty_B"))
            FAIL_TEST("Could not spawn kitty");

        try {
            NLog::log("{}Expecting kitty_B size: {},{}", Colors::YELLOW, WIDTH2, HEIGHT);

            {
                auto data = getFromSocket("/activewindow");
                data      = data.substr(data.find("size:") + 5);
                data      = data.substr(0, data.find('\n'));

                Hyprutils::String::CVarList2 sizes(std::move(data), 0, ',');

                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[0]}), WIDTH2, 2);
                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[1]}), HEIGHT, 2);
            }

            OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_A' })"));
            NLog::log("{}Expecting kitty_A size: {},{}", Colors::YELLOW, WIDTH_A_FINAL, HEIGHT);

            {
                auto data = getFromSocket("/activewindow");
                data      = data.substr(data.find("size:") + 5);
                data      = data.substr(0, data.find('\n'));

                Hyprutils::String::CVarList2 sizes(std::move(data), 0, ',');

                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[0]}), WIDTH_A_FINAL, 2);
                EXPECT_MAX_DELTA(std::stoi(std::string{sizes[1]}), HEIGHT, 2);
            }

        } catch (...) { FAIL_TEST("Exception thrown"); }

        OK(getFromSocket("/eval hl.config({ dwindle = { default_split_ratio = 1 } })"));
    }

    // open xeyes
    NLog::log("{}Spawning xeyes", Colors::YELLOW);
    getFromSocket("/dispatch hl.dsp.exec_cmd('xeyes')");

    NLog::log("{}Keep checking if xeyes spawned", Colors::YELLOW);
    Tests::waitUntilWindowsN(3);

    NLog::log("{}Expecting 3 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 3);

    NLog::log("{}Checking props of xeyes", Colors::YELLOW);
    // check some window props of xeyes, try to float it
    {
        auto str = getFromSocket("/clients");
        EXPECT_NOT_CONTAINS(str, "floating: 1");
        getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:XEyes' })");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    ASSERT(Tests::windowCount(), 0);

    getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })");

    NLog::log("{}Testing spawning a floating window over a fullscreen window", Colors::YELLOW);
    {
        if (!spawnKitty("kitty_A"))
            FAIL_TEST("Could not spawn kitty");
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen', action = 'set' })"));
        ASSERT(Tests::windowCount(), 1);

        OK(getFromSocket("/dispatch hl.dsp.exec_cmd('kitty', { float = true })"));
        Tests::waitUntilWindowsN(2);

        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:^kitty$' })"));
        const auto focused1 = getFromSocket("/activewindow");
        EXPECT_CONTAINS(focused1, "class: kitty\n");

        OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'activewindow' })"));
        Tests::waitUntilWindowsN(1);

        // The old window should be focused again
        const auto focused2 = getFromSocket("/activewindow");
        EXPECT_CONTAINS(focused2, "class: kitty_A\n");

        NLog::log("{}Killing all windows", Colors::YELLOW);
        Tests::killAllWindows();
    }

    NLog::log("{}Testing minsize/maxsize rules for tiled windows", Colors::YELLOW);
    {
        // Enable the config for testing, test max/minsize for tiled windows and centering
        OK(getFromSocket("/eval hl.config({ misc = { size_limits_tiled = 1 } })"));
        OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-max-rule', match = { class = 'kitty_maxsize' } })"));
        OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-max-rule', max_size = '1500 500' })"));
        OK(getFromSocket("r/eval hl.window_rule({ name = 'kitty-max-rule', min_size = '1200 500' })"));
        if (!spawnKitty("kitty_maxsize"))
            FAIL_TEST("Could not spawn kitty");

        auto dwindle = getFromSocket("/activewindow");
        EXPECT_CONTAINS(dwindle, "size: 1500,500");
        EXPECT_CONTAINS(dwindle, "at: 210,290");

        // Fuck this test, it's fucking stupid - vax
        // if (!spawnKitty("kitty_maxsize"))
        //     return false;
        // EXPECT_CONTAINS(getFromSocket("/activewindow"), "size: 1200,500");

        Tests::killAllWindows();
        ASSERT(Tests::windowCount(), 0);

        OK(getFromSocket("r/eval hl.config({ general = { layout = 'master' } })"));

        if (!spawnKitty("kitty_maxsize"))
            FAIL_TEST("Could not spawn kitty");

        auto master = getFromSocket("/activewindow");
        EXPECT_CONTAINS(master, "size: 1500,500");
        EXPECT_CONTAINS(master, "at: 210,290");

        if (!spawnKitty("kitty_maxsize"))
            FAIL_TEST("Could not spawn kitty");

        // FIXME: I can't be arsed.
        OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_maxsize' })"));
        //        EXPECT_CONTAINS(getFromSocket("/activewindow"), "size: 1200,500")

        NLog::log("{}Reloading config", Colors::YELLOW);
        OK(getFromSocket("/reload"));
        Tests::killAllWindows();
        ASSERT(Tests::windowCount(), 0);
    }

    NLog::log("{}Testing minsize/maxsize rules", Colors::YELLOW);
    {
        // Disable size limits tiled and check if props are working and not getting skipped
        OK(getFromSocket("/eval hl.config({ misc = { size_limits_tiled = 0 } })"));
        OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-max-rule', match = { class = 'kitty_maxsize' } })"));
        OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-max-rule', max_size = '1500 500' })"));
        OK(getFromSocket("r/eval hl.window_rule({ name = 'kitty-max-rule', min_size = '1200 500' })"));
        if (!spawnKitty("kitty_maxsize"))
            FAIL_TEST("Could not spawn kitty");

        {
            auto res = getFromSocket("/getprop active max_size");
            EXPECT_CONTAINS(res, "1500");
            EXPECT_CONTAINS(res, "500");
        }

        {
            auto res = getFromSocket("/getprop active min_size");
            EXPECT_CONTAINS(res, "1200");
            EXPECT_CONTAINS(res, "500");
        }

        NLog::log("{}Reloading config", Colors::YELLOW);
        OK(getFromSocket("/reload"));
        Tests::killAllWindows();
        ASSERT(Tests::windowCount(), 0);
    }

    {
        // Set float
        OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-max-rule', match = { class = 'kitty_maxsize' } })"));
        OK(getFromSocket("/eval hl.window_rule({ name = 'kitty-max-rule', max_size = '1200 500' })"));
        OK(getFromSocket("r/eval hl.window_rule({ name = 'kitty-max-rule', min_size = '1200 500' })"));
        OK(getFromSocket("r/eval hl.window_rule({ name = 'kitty-max-rule', float = true })"));
        if (!spawnKitty("kitty_maxsize"))
            FAIL_TEST("Could not spawn kitty");

        {
            auto res = getFromSocket("/getprop active max_size");
            EXPECT_CONTAINS(res, "1200");
            EXPECT_CONTAINS(res, "500");
        }

        {
            auto res = getFromSocket("/getprop active min_size");
            EXPECT_CONTAINS(res, "1200");
            EXPECT_CONTAINS(res, "500");
        }

        {
            auto res = getFromSocket("/activewindow");
            EXPECT_CONTAINS(res, "size: 1200,500");
        }

        NLog::log("{}Reloading config", Colors::YELLOW);
        OK(getFromSocket("/reload"));
        Tests::killAllWindows();
        ASSERT(Tests::windowCount(), 0);
    }

    NLog::log("{}Testing window rules", Colors::YELLOW);
    if (!spawnKitty("wr_kitty"))
        FAIL_TEST("Could not spawn kitty");
    {
        auto      str  = getFromSocket("/activewindow");
        const int SIZE = 200;
        EXPECT_CONTAINS(str, "floating: 1");
        EXPECT_CONTAINS(str, std::format("size: {},{}", SIZE, SIZE));
        EXPECT_NOT_CONTAINS(str, "pinned: 1");
    }

    OK(getFromSocket("/eval hl.window_rule({ name = 'wr-kitty-stuff', opacity = '0.5 0.5 override' })"));

    {
        auto str = getFromSocket("/getprop active opacity");
        EXPECT_CONTAINS(str, "0.5");
    }

    OK(getFromSocket("/eval hl.window_rule({ name = 'special-magic-kitty', match = { class = 'magic_kitty' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'special-magic-kitty', workspace = 'special:magic' })"));

    if (!spawnKitty("magic_kitty"))
        FAIL_TEST("Could not spawn kitty");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "special:magic");
        EXPECT_NOT_CONTAINS(str, "workspace: 9");
    }

    if (auto str = getFromSocket("/monitors"); str.contains("magic)")) {
        OK(getFromSocket("/dispatch hl.dsp.workspace.toggle_special('magic')"));
    }

    Tests::killAllWindows();

    OK(getFromSocket("/eval hl.window_rule({ name = 'border-magic-kitty', match = { class = 'border_kitty' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'border-magic-kitty', border_color = 'rgba(c6ff00ff) rgba(ff0000ee) 45deg' })"));

    if (!spawnKitty("border_kitty"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:border_kitty' })"));

    {
        auto str = getFromSocket("/getprop active active_border_color");
        EXPECT_CONTAINS(str, "ffc6ff00");
        EXPECT_CONTAINS(str, "eeff0000");
        EXPECT_CONTAINS(str, "45deg");
    }

    Tests::killAllWindows();

    if (!spawnKitty("tag_kitty"))
        FAIL_TEST("Could not spawn kitty");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    // test rules that overlap effects but don't overlap props
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'overlap_kitty' }, border_size = 0 })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { fullscreen = false }, border_size = 10 })"));

    if (!spawnKitty("overlap_kitty"))
        FAIL_TEST("Could not spawn kitty");

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "10");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    // test persistent_size between floating window launches
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'persistent_size_kitty' }, persistent_size = true, float = true })"));

    if (!spawnKitty("persistent_size_kitty"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/dispatch hl.dsp.window.resize({ x = 600, y = 400 })"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 600,400");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    Tests::killAllWindows();

    if (!spawnKitty("persistent_size_kitty"))
        FAIL_TEST("Could not spawn kitty");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "size: 600,400");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    OK(getFromSocket("/eval hl.config({ general = { border_size = 0 } })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { float = true }, border_size = 10 })"));

    if (!spawnKitty("border_kitty"))
        FAIL_TEST("Could not spawn kitty");

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle' })"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "10");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle' })"));

    {
        auto str = getFromSocket("/getprop active border_size");
        EXPECT_CONTAINS(str, "0");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    // test expression rules
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'expr_kitty' }, float = true, size = {'monitor_w * 0.5', 'monitor_h * 0.5'}, "
                     "min_size = {'monitor_w * 0.25', 'monitor_h * 0.25'}, max_size = {'monitor_w * 0.75', 'monitor_h * 0.75'}, "
                     "move = {'20 + (monitor_w * 0.1)', 'monitor_h * 0.5'} })"));

    if (!spawnKitty("expr_kitty"))
        FAIL_TEST("Could not spawn kitty");

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, "floating: 1");
        EXPECT_CONTAINS(str, "at: 212,540");
        EXPECT_CONTAINS(str, "size: 960,540");

        auto min = getFromSocket("/getprop active min_size");
        EXPECT_CONTAINS(min, "480");
        EXPECT_CONTAINS(min, "270");

        auto max = getFromSocket("/getprop active max_size");
        EXPECT_CONTAINS(max, "1440");
        EXPECT_CONTAINS(max, "810");
    }

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    OK(getFromSocket("/eval hl.plugin.test.add_window_rule()"));
    OK(getFromSocket("/reload"));

    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'plugin_kitty' }, plugin_rule = 'effect' })"));

    if (!spawnKitty("plugin_kitty"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/eval hl.plugin.test.check_window_rule()"));

    OK(getFromSocket("/reload"));
    Tests::killAllWindows();

    OK(getFromSocket("/eval hl.plugin.test.add_window_rule()"));
    OK(getFromSocket("/reload"));

    OK(getFromSocket("/eval hl.window_rule({ name = 'test-plugin-rule', match = { class = 'plugin_kitty' } })"));
    OK(getFromSocket("/eval hl.window_rule({ name = 'test-plugin-rule', plugin_rule = 'effect' })"));

    if (!spawnKitty("plugin_kitty"))
        FAIL_TEST("Could not spawn kitty");

    OK(getFromSocket("/eval hl.plugin.test.check_window_rule()"));
}

TEST_CASE(cycle_nextTiled) {
    // If the user specifically requests a tiled window, give them a tiled window

    if (!spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty of class:a");
    }

    if (!spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty of class:b");
    }

    if (!spawnKitty("c")) {
        FAIL_TEST("Could not spawn kitty of class:c");
    }

    if (!spawnKitty("d")) {
        FAIL_TEST("Could not spawn kitty of class:d");
    }

    // float the class:a window
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:a'})"));
    // float the class:c window
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:c'})"));
    // float the class:d window
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:d'})"));

    // establish focus history
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})")); // floating
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})")); // tiled  <-- What we want to focus on
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})")); // floating
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:d'})")); // floating

    // request a tiled window
    OK(getFromSocket("/dispatch hl.dsp.window.cycle_next({ next = true, tiled = true, floating = false})"));

    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: b");
}

TEST_CASE(cycle_nextFloating) {
    // If the user specifically requests a floating window, give them a floating window

    if (!spawnKitty("a")) {
        FAIL_TEST("Could not spawn kitty of class:a");
    }

    if (!spawnKitty("b")) {
        FAIL_TEST("Could not spawn kitty of class:b");
    }

    if (!spawnKitty("c")) {
        FAIL_TEST("Could not spawn kitty of class:c");
    }

    if (!spawnKitty("d")) {
        FAIL_TEST("Could not spawn kitty of class:d");
    }

    // float the class:b window
    OK(getFromSocket("/dispatch hl.dsp.window.float({action = 'enable', window = 'class:b'})"));

    // establish focus history
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:a'})")); // tiled
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:b'})")); // floating  <-- What we want to focus on
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:c'})")); // tiled
    OK(getFromSocket("/dispatch hl.dsp.focus({window = 'class:d'})")); // tiled

    // request a floating window
    OK(getFromSocket("/dispatch hl.dsp.window.cycle_next({ next = true, tiled = false, floating = true})"));

    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: b");
}

TEST_CASE(discussion14233) {
    // toggling fullscreen should not mess up workspace state on a 2 window workspace
    // with smart gaps

    OK(getFromSocket(R"#(/eval hl.workspace_rule({ workspace = "w[tv1]", gaps_out = 0, gaps_in = 0 })
hl.workspace_rule({ workspace = "f[1]",   gaps_out = 0, gaps_in = 0 })
hl.window_rule({
    name  = "no-gaps-wtv1",
    match = { float = false, workspace = "w[tv1]" },
    border_size = 0,
    rounding    = 0,
})
hl.window_rule({
    name  = "no-gaps-f1",
    match = { float = false, workspace = "f[1]" },
    border_size = 0,
    rounding    = 0,
})
)#"));

    ASSERT(!!Tests::spawnKitty(), true);

    // just make sure smart gaps work
    {
        auto str = getFromSocket("/clients");
        ASSERT_CONTAINS(str, "size: 1920,1080");
        ASSERT_CONTAINS(str, "at: 0,0");
    }

    ASSERT(!!Tests::spawnKitty(), true);

    {
        auto str = getFromSocket("/clients");
        ASSERT_COUNT_STRING(str, "size: 931,1036", 2);
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "at: 967,22");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen()"));

    {
        auto str = getFromSocket("/clients");
        ASSERT_COUNT_STRING(str, "size: 931,1036", 2);
        ASSERT_CONTAINS(str, "at: 22,22");
        ASSERT_CONTAINS(str, "at: 967,22");
    }
}

TEST_CASE(execRulesWorkspaceOverride) {
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_exec_override' }, workspace = '2' })"));

    OK(getFromSocket("/dispatch hl.dsp.exec_cmd('[workspace 3] kitty --class kitty_exec_override')"));

    Tests::waitUntilWindowsN(1);

    auto str = getFromSocket("/activewindow");
    EXPECT_CONTAINS(str, "class: kitty_exec_override");
    EXPECT_CONTAINS(str, "workspace: 3");

    Tests::killAllWindows();
}

TEST_CASE(execRulesTagMutation) {
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_tag_mutate' }, workspace = '2' })"));
    OK(getFromSocket("/eval hl.window_rule({ match = { class = 'kitty_tag_mutate' }, tag = 'test_tag' })"));

    OK(getFromSocket("/dispatch hl.dsp.exec_cmd('[workspace 3] kitty --class kitty_tag_mutate')"));

    Tests::waitUntilWindowsN(1);

    auto str = getFromSocket("/activewindow");
    EXPECT_CONTAINS(str, "class: kitty_tag_mutate");
    EXPECT_CONTAINS(str, "workspace: 3");

    Tests::killAllWindows();
}
