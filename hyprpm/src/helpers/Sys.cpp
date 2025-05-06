#include "Sys.hpp"
#include "Die.hpp"
#include "StringUtils.hpp"

#include <pwd.h>
#include <unistd.h>
#include <print>
#include <filesystem>
#include <optional>

#include <hyprutils/os/Process.hpp>
#include <hyprutils/string/VarList.hpp>

using namespace Hyprutils::OS;
using namespace Hyprutils::String;

inline constexpr std::array<std::string_view, 3> SUPERUSER_BINARIES = {
    "sudo",
    "doas",
    "run0",
};

static std::string fetchSuperuserBins() {
    std::ostringstream oss;
    auto               it = SUPERUSER_BINARIES.begin();
    if (it != SUPERUSER_BINARIES.end()) {
        oss << *it++;
        for (; it != SUPERUSER_BINARIES.end(); ++it)
            oss << ", " << *it;
    }

    return oss.str();
}

static bool executableExistsInPath(const std::string& exe) {
    const char* PATHENV = std::getenv("PATH");
    if (!PATHENV)
        return false;

    CVarList        paths(PATHENV, 0, ':', true);
    std::error_code ec;

    for (const auto& PATH : paths) {
        std::filesystem::path candidate = std::filesystem::path(PATH) / exe;
        if (!std::filesystem::exists(candidate, ec) || ec)
            continue;
        if (!std::filesystem::is_regular_file(candidate, ec) || ec)
            continue;
        auto perms = std::filesystem::status(candidate, ec).permissions();
        if (ec)
            continue;
        if ((perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none)
            return true;
    }

    return false;
}

static std::optional<std::pair<std::string, int>> execAndGet(std::string_view cmd, bool noRedirect = false) {
    std::string command = std::string{cmd};
    if (!noRedirect)
        command += " 2>&1";

    CProcess proc("/bin/sh", {"-c", command});
    if (!proc.runSync())
        // optional handles nullopt gracefully
        return std::nullopt;

    return {{proc.stdOut(), proc.exitCode()}};
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
    return getuid() != geteuid() || geteuid() == 0;
}

std::string NSys::runAsSuperuser(const std::string& cmd) {
    for (const auto& BIN : SUPERUSER_BINARIES) {
        if (!executableExistsInPath(std::string{BIN}))
            continue;

        const auto result = execAndGet(std::string{BIN} + " /bin/sh -c \"" + cmd + "\"", true);
        if (!result.has_value() || result->second != 0)
            Debug::die("Failed to run a command as sudo. This could be due to an invalid password, or a hyprpm bug.");

        return result->first;
    }

    Debug::die("{} {}", "Failed to find a superuser binary. Supported: ", fetchSuperuserBins());
    return "";
}

void NSys::cacheSudo() {
    // "caches" the sudo so that the prompt later doesn't pop up in a weird spot
    // sudo will not ask us again for a moment
    runAsSuperuser("echo e > /dev/null");
}

void NSys::dropSudo() {
    for (const auto& BIN : SUPERUSER_BINARIES) {
        if (!executableExistsInPath(std::string{BIN}))
            continue;

        if (BIN == "sudo")
            execAndGet("sudo -k");
        else {
            // note the superuser binary that is being dropped
            std::println("{}", infoString("Don't know how to drop timestamp for '{}', ignoring.", BIN));
        }
        return;
    }
}
