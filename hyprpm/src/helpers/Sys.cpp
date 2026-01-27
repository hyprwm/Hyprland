#include "Sys.hpp"
#include "Die.hpp"
#include "StringUtils.hpp"

#include <pwd.h>
#include <unistd.h>
#include <sstream>
#include <print>
#include <filesystem>
#include <algorithm>
#include <sstream>

#include <hyprutils/os/Process.hpp>
#include <hyprutils/string/VarList.hpp>

using namespace Hyprutils::OS;
using namespace Hyprutils::String;

inline constexpr std::array<std::string_view, 3> SUPERUSER_BINARIES = {
    "sudo",
    "doas",
    "run0",
};

static std::string validSubinsAsStr() {
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

static std::string subin() {
    static std::string bin;
    static bool        once = true;
    if (!once)
        return bin;

    for (const auto& BIN : SUPERUSER_BINARIES) {
        if (!executableExistsInPath(std::string{BIN}))
            continue;

        bin = BIN;
        break;
    }

    once = false;

    if (bin.empty())
        Debug::die("{}", failureString("No valid superuser binary present. Supported: {}", validSubinsAsStr()));

    return bin;
}

static bool verifyStringValid(const std::string& s) {
    return std::ranges::none_of(s, [](const char& c) { return c == '`' || c == '$' || c == '(' || c == ')' || c == '\'' || c == '"'; });
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

void NSys::root::cacheSudo() {
    // "caches" the sudo so that the prompt later doesn't pop up in a weird spot
    // sudo will not ask us again for a moment
    CProcess proc(subin(), {"echo", "hyprland"});
    proc.runSync();
}

void NSys::root::dropSudo() {
    if (subin() != "sudo") {
        std::println("{}", infoString("Don't know how to drop timestamp for '{}', ignoring.", subin()));
        return;
    }

    CProcess proc(subin(), {"-k"});
    proc.runSync();
}

bool NSys::root::createDirectory(const std::string& path, const std::string& mode) {
    if (!verifyStringValid(path))
        return false;

    if (!std::ranges::all_of(mode, [](const char& c) { return c >= '0' && c <= '9'; }))
        return false;

    CProcess proc(subin(), {"mkdir", "-p", "-m", mode, path});

    return proc.runSync() && proc.exitCode() == 0;
}

bool NSys::root::removeRecursive(const std::string& path) {
    if (!verifyStringValid(path))
        return false;

    std::error_code   ec;
    const std::string PATH_ABSOLUTE = std::filesystem::canonical(path, ec);

    if (ec)
        return false;

    if (!PATH_ABSOLUTE.starts_with("/var/cache/hyprpm"))
        return false;

    CProcess proc(subin(), {"rm", "-fr", PATH_ABSOLUTE});

    return proc.runSync() && proc.exitCode() == 0;
}

bool NSys::root::install(const std::string& what, const std::string& where, const std::string& mode) {
    if (!verifyStringValid(what) || !verifyStringValid(where))
        return false;

    if (!std::ranges::all_of(mode, [](const char& c) { return c >= '0' && c <= '9'; }))
        return false;

    CProcess proc(subin(), {"install", "-m" + mode, "-o", "0", "-g", "0", what, where});

    return proc.runSync() && proc.exitCode() == 0;
}

std::string NSys::root::runAsSuperuserUnsafe(const std::string& cmd) {
    CProcess proc(subin(), {"/bin/sh", "-c", cmd});

    if (!proc.runSync())
        return "";

    return proc.stdOut();
}
