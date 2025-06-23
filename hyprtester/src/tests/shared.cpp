#include "shared.hpp"
#include <csignal>
#include <cerrno>
#include "../shared.hpp"
#include "../hyprctlCompat.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

CUniquePointer<CProcess> Tests::spawnKitty() {
    CUniquePointer<CProcess> kitty = makeUnique<CProcess>("/bin/sh", std::vector<std::string>{"-c", std::format("WAYLAND_DISPLAY={} kitty >/dev/null 2>&1", WLDISPLAY)});
    kitty->runAsync();
    return kitty;
}

bool Tests::processAlive(pid_t pid) {
    kill(pid, 0);
    return errno != ESRCH;
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
