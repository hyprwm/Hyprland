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

    // Un-swallow the intial window
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

    // Re-swallow the intial window
    OK(getFromSocket("/dispatch hl.dsp.focus({ last = true })"));
    OK(getFromSocket("/dispatch hl.dsp.window.toggle_swallow()"));

    {
        // Verify that the initial has been re-swallowed
        std::string clients = getFromSocket("/clients");
        ASSERT_COUNT_STRING(clients, "swallowing: 0\n", 2);
        ASSERT_COUNT_STRING(clients, std::format("swallowing: {}\n", swalloweeID), 1);

        std::string workspaces = getFromSocket("/workspaces");
        ASSERT_CONTAINS(workspaces, "windows: 2\n");
    }
}
