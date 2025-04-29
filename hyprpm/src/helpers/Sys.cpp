#include "Sys.hpp"
#include <pwd.h>
#include <unistd.h>
#include <print>
#include <filesystem>

#include <hyprutils/os/Process.hpp>
#include <hyprutils/string/VarList.hpp>
using namespace Hyprutils::OS;
using namespace Hyprutils::String;

static const std::vector<const char*> SUPERUSER_BINARIES = {
    "sudo",
    "doas",
    "run0",
};

static bool executableExistsInPath(const std::string& exe) {
    if (!getenv("PATH"))
        return false;

    static CVarList paths(getenv("PATH"), 0, ':', true);

    for (auto& p : paths) {
        std::string     path = p + std::string{"/"} + exe;
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec)
            continue;

        if (!std::filesystem::is_regular_file(path, ec) || ec)
            continue;

        auto stat = std::filesystem::status(path, ec);
        if (ec)
            continue;

        auto perms = stat.permissions();

        return std::filesystem::perms::none != (perms & std::filesystem::perms::others_exec);
    }

    return false;
}

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
    for (const auto& SB : SUPERUSER_BINARIES) {
        if (!executableExistsInPath(SB))
            continue;

        return execAndGet(std::string{SB} + " /bin/sh -c \"" + cmd + "\"", true);
    }

    return "no superuser binary installed";
}