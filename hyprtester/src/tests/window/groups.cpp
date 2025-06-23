#include "groups.hpp"
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
    std::println("{}Testing groups", Colors::GREEN);

    // test on workspace "window"
    std::println("{}Dispatching workspace `groups`", Colors::YELLOW);
    getFromSocket("/dispatch workspace name:groups");

    std::println("{}Spawning kittyProcA", Colors::YELLOW);
    auto kittyProcA = Tests::spawnKitty();
    if (!kittyProcA) {
        std::println("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    std::println("{}Expecting 1 window", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 1);

    // check kitty properties. One kitty should take the entire screen, minus the gaps.
    std::println("{}Check kitty dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT(Tests::countOccurrences(str, "at: 22,22"), 1);
        EXPECT(Tests::countOccurrences(str, "size: 1876,1036"), 1);
        EXPECT(Tests::countOccurrences(str, "fullscreen: 0"), 1);
    }

    // group the kitty
    std::println("{}Enable group and groupbar", Colors::YELLOW);
    EXPECT(getFromSocket("/dispatch togglegroup"), "ok");
    EXPECT(getFromSocket("/keyword group:groupbar:enabled 1"), "ok");

    // check the height of the window now
    std::println("{}Recheck kitty dimensions", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,43"), true);
        EXPECT(str.contains("size: 1876,1015"), true);
    }

    // disable the groupbar for ease of testing for now
    std::println("{}Disable groupbar", Colors::YELLOW);
    EXPECT(getFromSocket("r/keyword group:groupbar:enabled 0"), "ok");

    // kill all
    std::println("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    std::println("{}Spawn kitty again", Colors::YELLOW);
    kittyProcA = Tests::spawnKitty();
    if (!kittyProcA) {
        std::println("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    std::println("{}Group kitty", Colors::YELLOW);
    EXPECT(getFromSocket("/dispatch togglegroup"), "ok");

    // check the height of the window now
    std::println("{}Check kitty dimensions 2", Colors::YELLOW);
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,22"), true);
        EXPECT(str.contains("size: 1876,1036"), true);
    }

    std::println("{}Spawn kittyProcB", Colors::YELLOW);
    auto kittyProcB = Tests::spawnKitty();
    if (!kittyProcB) {
        std::println("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    std::println("{}Expecting 2 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 2);

    size_t lastActiveKittyIdx = 0;

    std::println("{}Get last active kitty id", Colors::YELLOW);
    try {
        auto str           = getFromSocket("/activewindow");
        lastActiveKittyIdx = std::stoull(str.substr(7, str.find(" -> ") - 7));
    } catch (...) { ; }

    // test cycling through

    std::println("{}Test cycling through grouped windows", Colors::YELLOW);
    getFromSocket("/dispatch changegroupactive f");
    std::println("{}Weird 25ms thing", Colors::YELLOW);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx != std::stoull(str.substr(7, str.find(" -> ") - 7)), true);
    } catch (...) { ; }

    getFromSocket("/dispatch changegroupactive f");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    try {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx == std::stoull(str.substr(7, str.find(" -> ") - 7)), true);
    } catch (...) { ; }

    std::println("{}Disable autogrouping", Colors::YELLOW);
    EXPECT(getFromSocket("/keyword group:auto_group false"), "ok");

    std::println("{}Spawn kittyProcC", Colors::YELLOW);
    auto kittyProcC = Tests::spawnKitty();
    if (!kittyProcC) {
        std::println("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    std::println("{}Expecting 3 windows 2", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 3);
    {
        auto str = getFromSocket("/clients");
        EXPECT(Tests::countOccurrences(str, "at: 22,22"), 2);
    }

    EXPECT(getFromSocket("/dispatch movefocus l"), "ok");
    EXPECT(getFromSocket("/dispatch changegroupactive 1"), "ok");
    EXPECT(getFromSocket("/keyword group:auto_group true"), "ok");
    EXPECT(getFromSocket("/keyword group:insert_after_current false"), "ok");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::println("{}Spawn kittyProcD", Colors::YELLOW);
    auto kittyProcD = Tests::spawnKitty();
    if (!kittyProcD) {
        std::println("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    std::println("{}Expecting 4 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 4);

    EXPECT(getFromSocket("/dispatch changegroupactive 3"), "ok");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains(std::format("pid: {}", kittyProcD->pid())), true);
    }

    // kill all
    std::println("{}Kill windows", Colors::YELLOW);
    Tests::killAllWindows();

    std::println("{}Expecting 0 windows", Colors::YELLOW);
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}
