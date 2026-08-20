#include <thread>
#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

static void awaitKittyPrompt(const std::string& name) {
    // wait until we see the shell prompt, meaning it's ready for test inputs
    for (int i = 0; i < 10; i++) {
        std::string output = Tests::execAndGet(std::format("kitten @ --to unix:/tmp/kitty_{}.sock get-text --extent all", name));
        if (output.rfind('$') == std::string::npos) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }
        return;
    }
    NLog::log("{}Error: timed out waiting for kitty prompt", Colors::RED);
}

static CUniquePointer<CProcess> spawnRemoteControlKitty(const std::string& name) {
    auto kittyProc =
        Tests::spawnKitty("kitty_swallowee", {"-o", "allow_remote_control=yes", "--listen-on", std::format("unix:/tmp/kitty_{}.sock", name), "--config", "NONE", "/bin/sh"});
    // wait a bit to ensure shell prompt is sent, we are going to read the text after it
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (kittyProc)
        awaitKittyPrompt(name);
    return kittyProc;
}

static bool spawnSwallower(const std::string& parent, const std::string& name) {
    auto cmd    = std::format("kitten @ --to unix:/tmp/kitty_{}.sock launch --type=background "
                              "kitty -o allow_remote_control=yes --class kitty_{} --listen-on unix:/tmp/kitty_{}.sock --config NONE /bin/sh",
                              parent, name, name);
    auto result = Tests::execAndGet(cmd);
    if (result == "error")
        return false;

    // wait a bit to ensure shell prompt is sent, we are going to read the text after it
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    awaitKittyPrompt(name);
    return true;
}

static bool spawnKittyOsWindow(const std::string& parent) {
    auto cmd    = std::format("kitten @ --to unix:/tmp/kitty_{}.sock launch --type=os-window", parent);
    auto result = Tests::execAndGet(cmd);

    if (result == "error")
        return false;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

static int swallowingCount() {
    int notSwallowing = Tests::countOccurrences(getFromSocket("/clients"), "swallowing: 0\n");
    return Tests::windowCount() - notSwallowing;
}

static std::string getActiveWindowID() {
    std::string            activeWindow = getFromSocket("/activewindow");
    std::string::size_type start        = 7; // Length of "Window "
    std::string::size_type end          = activeWindow.find(" ->");
    if (end == std::string::npos) {
        return "error";
    }
    return activeWindow.substr(start, end - start);
}

TEST_CASE(swallow) {
    NLog::log("{}Testing window swallowing", Colors::GREEN);

    // test on workspace "swallow"
    NLog::log("{}Switching to workspace `swallow`", Colors::YELLOW);
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'name:swallow' })"));

    ASSERT(Tests::windowCount(), 0);

    OK(getFromSocket("/eval hl.config({ misc = { enable_swallow = true } })"));
    OK(getFromSocket("/eval hl.config({ misc = { swallow_regex = '^(kitty_swallowee)$' } })"));

    // Initial kitty window that will be swallowed
    spawnRemoteControlKitty("swallowee");

    ASSERT(Tests::windowCount(), 1);
    ASSERT(swallowingCount(), 0);

    // Get the window ID
    std::string swalloweeID = getActiveWindowID();
    if (swalloweeID == "error") {
        FAIL_TEST("Could not get window ID");
    }
    NLog::log("{}Got swalloweeID: {}", Colors::YELLOW, swalloweeID);

    // Spawn a child process that should swallow the initial kitty window
    ASSERT(spawnSwallower("swallowee", "swallower"), true);

    {
        // Verify that the initial window is swallowed
        std::string clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 1);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", swalloweeID), 1);

        std::string workspaces = getFromSocket("/workspaces");
        ASSERT_CONTAINS(workspaces, "windows: 1\n");
    }

    // Un-swallow the initial window
    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));

    {
        // Verify that the initial window is un-swallowed
        std::string clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 1);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", swalloweeID), 1);

        std::string workspaces = getFromSocket("/workspaces");
        ASSERT_CONTAINS(workspaces, "windows: 2\n");
    }

    // Open new window of the swallower kitty
    ASSERT(spawnKittyOsWindow("swallower"), true);

    {
        // Verify that the initial has NOT been re-swallowed
        std::string clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 2);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", swalloweeID), 1);

        std::string workspaces = getFromSocket("/workspaces");
        ASSERT_CONTAINS(workspaces, "windows: 3\n");
    }

    // Re-swallow the initial window
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_swallowee' })"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: kitty_swallowee");
    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: kitty_swallower");

    {
        // Verify that the initial has been re-swallowed
        std::string clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 2);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", swalloweeID), 1);

        std::string workspaces = getFromSocket("/workspaces");
        ASSERT_CONTAINS(workspaces, "windows: 2\n");
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    NLog::log("{}Testing chained window swallowing", Colors::GREEN);
    OK(getFromSocket("/eval hl.config({ misc = { swallow_regex = '^(kitty_swallowee|kitty_chain_middle|kitty_chain_removed_middle)$' } })"));

    spawnRemoteControlKitty("chain_first");
    const std::string chainFirstID = getActiveWindowID();
    if (chainFirstID == "error")
        FAIL_TEST("Could not get first chain window ID");

    ASSERT(spawnSwallower("chain_first", "chain_middle"), true);
    const std::string chainMiddleID = getActiveWindowID();
    if (chainMiddleID == "error")
        FAIL_TEST("Could not get middle chain window ID");

    ASSERT(spawnSwallower("chain_middle", "chain_last"), true);
    {
        const auto clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 1);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", chainFirstID), 1);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", chainMiddleID), 1);
        ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: kitty_chain_last");
        ASSERT_CONTAINS(getFromSocket("/workspaces"), "windows: 1\n");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:kitty_chain_last' })"));
    Tests::waitUntilWindowsN(2);
    {
        const auto clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 1);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", chainFirstID), 1);
        ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: kitty_chain_middle");
        ASSERT_CONTAINS(getFromSocket("/workspaces"), "windows: 1\n");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:kitty_chain_middle' })"));
    Tests::waitUntilWindowsN(1);
    ASSERT_COUNT_STRING(getFromSocket("/clients"), "swallowing: 0\n", 1);
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: kitty_swallowee");
    ASSERT_CONTAINS(getFromSocket("/workspaces"), "windows: 1\n");

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    spawnRemoteControlKitty("chain_removed_first");
    ASSERT(spawnSwallower("chain_removed_first", "chain_removed_middle"), true);
    ASSERT(spawnSwallower("chain_removed_middle", "chain_removed_last"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:kitty_chain_removed_middle' })"));
    Tests::waitUntilWindowsN(2);
    ASSERT_COUNT_STRING(getFromSocket("/clients"), "swallowing: 0\n", 2);
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "class: kitty_chain_removed_last");
    ASSERT_CONTAINS(getFromSocket("/workspaces"), "windows: 2\n");

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
    OK(getFromSocket("/eval hl.config({ misc = { swallow_regex = '^(kitty_swallowee)$' } })"));

    NLog::log("{}Testing grouped window swallowing", Colors::GREEN);
    OK(getFromSocket("/eval hl.config({ group = { auto_group = true } })"));

    spawnRemoteControlKitty("grouped_swallowee");
    const std::string groupedSwalloweeID = getActiveWindowID();
    if (groupedSwalloweeID == "error")
        FAIL_TEST("Could not get grouped swallowee ID");
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    ASSERT(!!Tests::spawnKitty("swallow_group_peer"), true);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_swallowee' })"));

    {
        const auto clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "grouped: 0\n", 0);
    }

    ASSERT(spawnSwallower("grouped_swallowee", "grouped_swallower"), true);

    {
        const auto clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "grouped: 0\n", 1);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", groupedSwalloweeID), 1);
    }

    // Only internal fullscreen follows the group slot; each endpoint keeps its own client mode.
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 1, action = 'set', layout_aware = false })"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "fullscreen: 2");
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "fullscreenClient: 1");

    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));
    {
        const auto active = getFromSocket("/activewindow");
        ASSERT_CONTAINS(active, "class: kitty_swallowee");
        ASSERT_CONTAINS(active, "fullscreen: 2");
        ASSERT_CONTAINS(active, "fullscreenClient: 0");
    }
    ASSERT_COUNT_STRING(getFromSocket("/clients"), std::format("swallowing: {}\n", groupedSwalloweeID), 1);
    ASSERT_COUNT_STRING(getFromSocket("/clients"), "fullscreenClient: 1\n", 1);

    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));
    {
        const auto active = getFromSocket("/activewindow");
        ASSERT_CONTAINS(active, "class: kitty_grouped_swallower");
        ASSERT_CONTAINS(active, "fullscreen: 2");
        ASSERT_CONTAINS(active, "fullscreenClient: 1");
    }

    // Internal-only slot transfers preserve the endpoint's client maximize restoration state.
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 1, client = 1, action = 'set', layout_aware = false })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set', layout_aware = false })"));
    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));
    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));

    // A transfer must not carry an old echo suppression into a later genuine client request.
    OK(getFromSocket("/eval hl.plugin.test.expect_no_maximize_echo()"));

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 2, action = 'set', layout_aware = false })"));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', layout_aware = false })"));
    {
        const auto active = getFromSocket("/activewindow");
        ASSERT_CONTAINS(active, "class: kitty_grouped_swallower");
        ASSERT_CONTAINS(active, "fullscreen: 0");
        ASSERT_CONTAINS(active, "fullscreenClient: 1");
    }

    // Clear the restored client mode before resetting the state used by the close test.
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 0, client = 0, action = 'set', layout_aware = false })"));
    ASSERT_CONTAINS(getFromSocket("/activewindow"), "fullscreenClient: 0");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 1, action = 'set', layout_aware = false })"));

    // Closing the source restores the swallowee and the default-handled internal fullscreen state.
    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:kitty_grouped_swallower' })"));
    Tests::waitUntilWindowsN(2);
    ASSERT(Tests::windowCount(), 2);
    {
        const auto clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "grouped: 0\n", 0);
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 2);
    }
    {
        const auto active = getFromSocket("/activewindow");
        ASSERT_CONTAINS(active, "class: kitty_swallowee");
        ASSERT_CONTAINS(active, "fullscreen: 2");
        ASSERT_CONTAINS(active, "fullscreenClient: 0");
        ASSERT_CONTAINS(active, "fullscreenHandler: default");
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    // Destroying the revealed target restores the source to the group slot and clears both reservations.
    spawnRemoteControlKitty("target_first_swallowee");
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    ASSERT(!!Tests::spawnKitty("target_first_group_peer"), true);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_swallowee' })"));
    ASSERT(spawnSwallower("target_first_swallowee", "target_first_swallower"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));
    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:kitty_swallowee' })"));
    Tests::waitUntilWindowsN(2);
    ASSERT(Tests::windowCount(), 2);
    {
        const auto clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "grouped: 0\n", 0);
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 2);
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);

    // Layout-managed fullscreen follows the group slot and survives closing the active swallower too.
    OK(getFromSocket("/eval hl.config({ general = { layout = 'scrolling' } })"));
    spawnRemoteControlKitty("layout_swallowee");
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    ASSERT(!!Tests::spawnKitty("layout_swallow_group_peer"), true);
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:kitty_swallowee' })"));
    ASSERT(spawnSwallower("layout_swallowee", "layout_swallower"), true);
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen_state({ internal = 2, client = 1, action = 'set', layout_aware = true })"));

    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));
    {
        const auto active = getFromSocket("/activewindow");
        ASSERT_CONTAINS(active, "class: kitty_swallowee");
        ASSERT_CONTAINS(active, "fullscreen: 2");
        ASSERT_CONTAINS(active, "fullscreenClient: 0");
        ASSERT_CONTAINS(active, "fullscreenHandler: scrolling");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));
    OK(getFromSocket("/dispatch hl.dsp.window.kill({ window = 'class:kitty_layout_swallower' })"));
    Tests::waitUntilWindowsN(2);
    ASSERT(Tests::windowCount(), 2);
    {
        const auto active = getFromSocket("/activewindow");
        ASSERT_CONTAINS(active, "class: kitty_swallowee");
        ASSERT_CONTAINS(active, "fullscreen: 2");
        ASSERT_CONTAINS(active, "fullscreenClient: 0");
        ASSERT_CONTAINS(active, "fullscreenHandler: scrolling");
    }

    Tests::killAllWindows();
    ASSERT(Tests::windowCount(), 0);
    OK(getFromSocket("/eval hl.config({ general = { layout = 'dwindle' } })"));

    // A locked candidate group must remain intact and must not be swallowed.
    spawnRemoteControlKitty("locked_swallowee");
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/dispatch hl.dsp.group.lock_active({ action = 'set' })"));
    ASSERT(spawnSwallower("locked_swallowee", "locked_swallower"), true);
    ASSERT(swallowingCount(), 0);
    ASSERT_COUNT_STRING(getFromSocket("/clients"), "grouped: 0\n", 1);

    OK(getFromSocket("/eval hl.config({ group = { auto_group = false } })"));
    Tests::killAllWindows();
}
