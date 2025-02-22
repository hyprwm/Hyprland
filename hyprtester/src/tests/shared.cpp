#include "shared.hpp"
#include <csignal>
#include <cerrno>
#include "../shared.hpp"
#include "../hyprctlCompat.hpp"

using namespace Hyprutils::OS;

CProcess Tests::spawnKitty() {
    CProcess kitty{"/bin/bash", {"-c", std::format("WAYLAND_DISPLAY={} kitty", WLDISPLAY)}};
    kitty.runAsync();
    return kitty;
}

bool Tests::processAlive(pid_t pid) {
    kill(pid, 0);
    return errno != ESRCH;
}

int Tests::windowCount() {
    return countOccurrences(getFromSocket("/clients"), "focusHistoryID: ");
}

int                     Tests::countOccurrences(const std::string& in, const std::string& what) {
    int  cnt = 0;
    auto pos = in.find(what);
    while (pos != std::string::npos) {
        cnt++;
        pos = in.find(what, pos + what.length() - 1);
    }

    return cnt;
}
