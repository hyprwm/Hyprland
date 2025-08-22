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

static bool test() {
    NLog::log("{}Testing gestures", Colors::GREEN);

    EXPECT(Tests::windowCount(), 0);

    // test on workspace "window"
    NLog::log("{}Switching to workspace 1", Colors::YELLOW);
    getFromSocket("/dispatch workspace 1"); // no OK: we might be on 1 already

    OK(getFromSocket("/dispatch plugin:test:gesture left,3"));

    // wait while kitty spawns
    int counter = 0;
    while (Tests::windowCount() != 1) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}Gesture didnt spawn kitty", Colors::RED);
            return false;
        }
    }

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
    OK(getFromSocket("/dispatch plugin:test:gesture down,3"));

    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "floating: 0");
    }

    OK(getFromSocket("/dispatch plugin:test:alt 0"));

    OK(getFromSocket("/dispatch plugin:test:gesture up,3"));

    counter = 0;
    while (Tests::windowCount() != 0) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}Gesture didnt close kitty", Colors::RED);
            return false;
        }
    }

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
