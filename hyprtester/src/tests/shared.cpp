#include "shared.hpp"
#include <csignal>
#include <cerrno>
#include <thread>
#include <print>
#include "../shared.hpp"
#include "../hyprctlCompat.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

CUniquePointer<CProcess> Tests::spawnKitty(const std::string& class_) {
    const auto               COUNT_BEFORE = windowCount();

    CUniquePointer<CProcess> kitty = makeUnique<CProcess>("kitty", class_.empty() ? std::vector<std::string>{} : std::vector<std::string>{"--class", class_});
    kitty->addEnv("WAYLAND_DISPLAY", WLDISPLAY);
    kitty->runAsync();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // wait while kitty spawns
    int counter = 0;
    while (processAlive(kitty->pid()) && windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50)
            return nullptr;
    }

    if (!processAlive(kitty->pid()))
        return nullptr;

    return kitty;
}

bool Tests::processAlive(pid_t pid) {
    errno   = 0;
    int ret = kill(pid, 0);
    return ret != -1 || errno != ESRCH;
}

int Tests::windowCount() {
    return countOccurrences(getFromSocket("/clients"), "focusHistoryID: ");
}

int Tests::countOccurrences(const std::string& in, const std::string& what) {
    int  cnt = 0;
    auto pos = in.find(what);
    while (pos != std::string::npos) {
        cnt++;
        pos = in.find(what, pos + what.length() - 1);
    }

    return cnt;
}

bool Tests::killAllWindows() {
    auto str = getFromSocket("/clients");
    auto pos = str.find("Window ");
    while (pos != std::string::npos) {
        auto pos2 = str.find(" -> ", pos);
        getFromSocket("/dispatch killwindow address:0x" + str.substr(pos + 7, pos2 - pos - 7));
        pos = str.find("Window ", pos + 5);
    }

    int counter = 0;
    while (Tests::windowCount() != 0) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            std::println("{}Timed out waiting for windows to close", Colors::RED);
            return false;
        }
    }

    return true;
}

void Tests::waitUntilWindowsN(int n) {
    int counter = 0;
    while (Tests::windowCount() != n) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            std::println("{}Timed out waiting for windows", Colors::RED);
            return;
        }
    }
}
