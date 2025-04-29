#include "Sys.hpp"
#include <pwd.h>
#include <unistd.h>
#include <print>

#include <hyprutils/os/Process.hpp>
using namespace Hyprutils::OS;

static std::string execAndGet(std::string cmd, bool noRedirect = false) {
    if (!noRedirect)
        cmd += " 2>&1";

    CProcess proc("/bin/sh", {"-c", cmd});

    if (!proc.runSync())
        return "error";

    return proc.stdOut();
}

int NSys::getUID() {
    const auto UID   = getuid();
    const auto PWUID = getpwuid(UID);
    return PWUID ? PWUID->pw_uid : UID;
}

int NSys::getEUID() {
    const auto UID   = geteuid();
    const auto PWUID = getpwuid(UID);
    return PWUID ? PWUID->pw_uid : UID;
}

bool NSys::isSuperuser() {
    return getuid() != geteuid() || !geteuid();
}

std::string NSys::runAsSuperuser(const std::string& cmd) {
    return execAndGet("sudo /bin/sh -c \"" + cmd + "\"", true);
}