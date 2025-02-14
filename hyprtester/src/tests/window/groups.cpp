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
    getFromSocket("/dispatch workspace name:groups");

    auto kittyProcA = Tests::spawnKitty();
    int  counter    = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (Tests::processAlive(kittyProcA.pid()) && Tests::windowCount() != 1) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 20) {
            EXPECT(Tests::windowCount(), 1);
            return !ret;
        }
    }

    EXPECT(Tests::windowCount(), 1);

    // check kitty properties. One kitty should take the entire screen, minus the gaps.
    {
        auto str = getFromSocket("/clients");
        EXPECT(Tests::countOccurrences(str, "at: 22,22"), 1);
        EXPECT(Tests::countOccurrences(str, "size: 1876,1036"), 1);
        EXPECT(Tests::countOccurrences(str, "fullscreen: 0"), 1);
    }

    // group the kitty
    EXPECT(getFromSocket("/dispatch togglegroup"), "ok");
    EXPECT(getFromSocket("/keyword group:groupbar:enabled 1"), "ok");

    // check the height of the window now
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,45"), true);
        EXPECT(str.contains("size: 1876,1013"), true);
    }

    // disable the groupbar for ease of testing for now
    EXPECT(getFromSocket("r/keyword group:groupbar:enabled 0"), "ok");

    // kill all
    {
        auto str = getFromSocket("/clients");
        auto pos = str.find("Window ");
        while (pos != std::string::npos) {
            auto pos2 = str.find(" -> ", pos);
            getFromSocket("/dispatch killwindow address:0x" + str.substr(pos + 7, pos2 - pos - 7));
            pos = str.find("Window ", pos + 5);
        }
    }

    kittyProcA = Tests::spawnKitty();
    counter    = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (Tests::processAlive(kittyProcA.pid()) && Tests::windowCount() != 1) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 20) {
            EXPECT(Tests::windowCount(), 1);
            return !ret;
        }
    }

    EXPECT(getFromSocket("/dispatch togglegroup"), "ok");

    // check the height of the window now
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 22,22"), true);
        EXPECT(str.contains("size: 1876,1036"), true);
    }

    auto kittyProcB = Tests::spawnKitty();
    counter         = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (Tests::processAlive(kittyProcB.pid()) && Tests::windowCount() != 2) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 20) {
            EXPECT(Tests::windowCount(), 2);
            return !ret;
        }
    }

    EXPECT(Tests::windowCount(), 2);

    size_t lastActiveKittyIdx = 0;

    {
        auto str           = getFromSocket("/activewindow");
        lastActiveKittyIdx = std::stoull(str.substr(7, str.find(" -> ") - 7));
    }

    // test cycling through

    getFromSocket("/dispatch changegroupactive f");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx != std::stoull(str.substr(7, str.find(" -> ") - 7)), true);
    }

    getFromSocket("/dispatch changegroupactive f");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(lastActiveKittyIdx == std::stoull(str.substr(7, str.find(" -> ") - 7)), true);
    }

    EXPECT(getFromSocket("/keyword group:auto_group false"), "ok");

    auto kittyProcC = Tests::spawnKitty();
    counter         = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (Tests::processAlive(kittyProcC.pid()) && Tests::windowCount() != 3) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 20) {
            EXPECT(Tests::windowCount(), 3);
            return !ret;
        }
    }

    EXPECT(Tests::windowCount(), 3);
    {
        auto str = getFromSocket("/clients");
        EXPECT(Tests::countOccurrences(str, "at: 22,22"), 2);
    }

    EXPECT(getFromSocket("/dispatch movefocus l"), "ok");
    EXPECT(getFromSocket("/dispatch changegroupactive 1"), "ok");
    EXPECT(getFromSocket("/keyword group:auto_group true"), "ok");
    EXPECT(getFromSocket("/keyword group:insert_after_current false"), "ok");
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    auto kittyProcD = Tests::spawnKitty();
    counter         = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (Tests::processAlive(kittyProcD.pid()) && Tests::windowCount() != 4) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 20) {
            EXPECT(Tests::windowCount(), 4);
            return !ret;
        }
    }

    EXPECT(Tests::windowCount(), 4);

    EXPECT(getFromSocket("/dispatch changegroupactive 3"), "ok");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        auto str = getFromSocket("/activewindow");
        EXPECT(str.contains(std::format("pid: {}", kittyProcD.pid())), true);
    }

    // kill all
    {
        auto str = getFromSocket("/clients");
        auto pos = str.find("Window ");
        while (pos != std::string::npos) {
            auto pos2 = str.find(" -> ", pos);
            getFromSocket("/dispatch killwindow address:0x" + str.substr(pos + 7, pos2 - pos - 7));
            pos = str.find("Window ", pos + 5);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT(Tests::windowCount(), 0);

    return !ret;
}