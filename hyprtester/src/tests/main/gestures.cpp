#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include <print>
#include <thread>
#include <chrono>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <csignal>
#include <cerrno>
#include "../shared.hpp"

static int ret = 0;

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

static bool waitForWindowCount(int expectedWindowCnt, std::string_view expectation, int waitMillis = 100, int maxWaitCnt = 50) {
    int counter = 0;
    while (Tests::windowCount() != expectedWindowCnt) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(waitMillis));

        if (counter > maxWaitCnt) {
            NLog::log("{}Unmet expectation: {}", Colors::RED, expectation);
            return false;
        }
    }
    return true;
}

static bool test() {
    NLog::log("{}Testing gestures", Colors::GREEN);

    EXPECT(Tests::windowCount(), 0);

    // test on workspace "window"
    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    getFromSocket("/dispatch workspace 1"); // no OK: we might be on 1 already

    Tests::spawnKitty();
    EXPECT(Tests::windowCount(), 1);

    // Give the shell a moment to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    OK(getFromSocket("/dispatch plugin:test:gesture up,4"));

    EXPECT(waitForWindowCount(0, "Gesture sent ctrl+d to kitty"), true);

    EXPECT(Tests::windowCount(), 0);

    OK(getFromSocket("/dispatch plugin:test:gesture left,3"));

    EXPECT(waitForWindowCount(1, "Gesture spawned kitty"), true);

    EXPECT(Tests::windowCount(), 1);

    OK(getFromSocket("/dispatch plugin:test:gesture right,3"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "floating: 1");
    }

    OK(getFromSocket("/dispatch plugin:test:gesture down,3"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "fullscreen: 2");
    }

    OK(getFromSocket("/dispatch plugin:test:gesture down,3"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "fullscreen: 0");
    }

    OK(getFromSocket("/dispatch plugin:test:alt 1"));

    OK(getFromSocket("/dispatch plugin:test:gesture left,3"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/dispatch plugin:test:gesture right,3"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
    }

    // check for crashes
    OK(getFromSocket("/dispatch plugin:test:gesture right,3"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/keyword gestures:workspace_swipe_invert 0"));

    OK(getFromSocket("/dispatch plugin:test:gesture right,3"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/dispatch plugin:test:gesture left,3"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
    }

    OK(getFromSocket("/keyword gestures:workspace_swipe_invert 1"));
    OK(getFromSocket("/keyword gestures:workspace_swipe_create_new 0"));

    OK(getFromSocket("/dispatch plugin:test:gesture left,3"));

    {
        auto str = getFromSocket("/workspaces");
        EXPECT_NOT_CONTAINS(str, "ID 2 (2)");
        EXPECT_CONTAINS(str, "ID 1 (1)");
    }

    OK(getFromSocket("/dispatch plugin:test:gesture down,3"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "floating: 0");
    }

    OK(getFromSocket("/dispatch plugin:test:alt 0"));

    OK(getFromSocket("/dispatch plugin:test:gesture up,3"));

    EXPECT(waitForWindowCount(0, "Gesture closed kitty"), true);

    EXPECT(Tests::windowCount(), 0);

    // kill all
    NLog::log("{}Killing all windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    // reload cfg
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_TEST_FN(test)
