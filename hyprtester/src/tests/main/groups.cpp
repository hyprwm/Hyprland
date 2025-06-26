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

bool testGroups() {
    NLog::log("{}Testing groups", Colors::GREEN);

    // test on workspace "window"
    NLog::log("{}Dispatching workspace `groups`", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:groups");

    NLog::log("{}Spawning kittyProcA", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty();
    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 1 window", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 1);

    // check kitty properties. One kitty should take the entire screen, minus the gaps.
    NLog::log("{}Check kitty dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 1);
        EXPECT_COUNT_STRING(str, "size: 1876,1036", 1);
        EXPECT_COUNT_STRING(str, "fullscreen: 0", 1);
    }

    // group the kitty
    NLog::log("{}Enable group and groupbar", Colors::YELLOW);
    OK(getFromSocket("/dispatch togglegroup"));
    OK(getFromSocket("/keyword group:groupbar:enabled 1"));

    // check the height of the window now
    NLog::log("{}Recheck kitty dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 22,43");
        EXPECT_CONTAINS(str, "size: 1876,1015");
    }

    // disable the groupbar for ease of testing for now
    NLog::log("{}Disable groupbar", Colors::YELLOW);
    OK(getFromSocket("r/keyword group:groupbar:enabled 0"));

    // kill all
    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Spawn kitty again", Colors::YELLOW);
    kittyProcA = Tests::spawnKitty();
    if (!kittyProcA) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Group kitty", Colors::YELLOW);
    OK(getFromSocket("/dispatch togglegroup"));

    // check the height of the window now
    NLog::log("{}Check kitty dimensions 2", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT_CONTAINS(str, "at: 22,22");
        EXPECT_CONTAINS(str, "size: 1876,1036");
    }

    NLog::log("{}Spawn kittyProcB", Colors::YELLOW);
    auto kittyProcB = Tests::spawnKitty();
    if (!kittyProcB) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);

    size_t lastActiveKittyIdx = 0;

    NLog::log("{}Get last active kitty id", Colors::YELLOW);
    try {
        auto str           = getFromSocket("/activewindow");
        lastActiveKittyIdx = std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16);
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    // test cycling through

    NLog::log("{}Test cycling through grouped windows", Colors::YELLOW);
    OK(getFromSocket("/dispatch changegroupactive f"));

    try {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx != std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16), true);
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    getFromSocket("/dispatch changegroupactive f");

    try {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx, std::stoull(str.substr(7, str.find(" -> ") - 7), nullptr, 16));
    } catch (...) {
        NLog::log("{}Fail at getting prop", Colors::RED);
        ret = 1;
    }

    NLog::log("{}Disable autogrouping", Colors::YELLOW);
    OK(getFromSocket("/keyword group:auto_group false"));

    NLog::log("{}Spawn kittyProcC", Colors::YELLOW);
    auto kittyProcC = Tests::spawnKitty();
    if (!kittyProcC) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 3 windows 2", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 3);
    {
        auto str = getFromSocket("/clients");
        EXPECT_COUNT_STRING(str, "at: 22,22", 2);
    }

    OK(getFromSocket("/dispatch movefocus l"));
    OK(getFromSocket("/dispatch changegroupactive 1"));
    OK(getFromSocket("/keyword group:auto_group true"));
    OK(getFromSocket("/keyword group:insert_after_current false"));

    NLog::log("{}Spawn kittyProcD", Colors::YELLOW);
    auto kittyProcD = Tests::spawnKitty();
    if (!kittyProcD) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    NLog::log("{}Expecting 4 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 4);

    OK(getFromSocket("/dispatch changegroupactive 3"));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT_CONTAINS(str, std::format("pid: {}", kittyProcD->pid()));
    }

    // kill all
    NLog::log("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    NLog::log("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}
