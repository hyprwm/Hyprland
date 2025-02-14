#include "window.hpp"
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

bool testWindows() {
    std::println("{}Testing windows", Colors::GREEN);

    // test on workspace "window"
    getFromSocket("/dispatch workspace name:window");

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

    // check kitty properties. One kitty should take the entire screen, as this is smart gaps
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("at: 0,0"), true);
        EXPECT(str.contains("size: 1920,1080"), true);
        EXPECT(str.contains("fullscreen: 0"), true);
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

    // open xeyes
    getFromSocket("/dispatch exec xeyes");
    counter = 0;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (Tests::windowCount() != 3) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 20) {
            EXPECT(Tests::windowCount(), 3);
            return !ret;
        }
    }

    EXPECT(Tests::windowCount(), 3);

    // check some window props of xeyes, try to tile them
    {
        auto str = getFromSocket("/clients");
        EXPECT(str.contains("floating: 1"), true);
        getFromSocket("/dispatch settiled class:XEyes");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        str = getFromSocket("/clients");
        EXPECT(str.contains("floating: 1"), false);
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