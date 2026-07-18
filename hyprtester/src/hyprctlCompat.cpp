#include "hyprctlCompat.hpp"
#include "HyprCtlSocket.hpp"
#include "shared.hpp"

#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <csignal>
#include <cerrno>
#include <print>
#include <hyprutils/memory/Casts.hpp>
using namespace Hyprutils::Memory;

static int getUID() {
    const auto UID   = getuid();
    const auto PWUID = getpwuid(UID);
    return PWUID ? PWUID->pw_uid : UID;
}

static std::string getRuntimeDir() {
    const auto XDG = getenv("XDG_RUNTIME_DIR");

    if (!XDG) {
        const std::string USERID = std::to_string(getUID());
        return "/run/user/" + USERID + "/hypr";
    }

    return std::string{XDG} + "/hypr";
}

std::vector<SInstanceData> instances() {
    std::vector<SInstanceData> result;

    try {
        if (!std::filesystem::exists(getRuntimeDir()))
            return {};
    } catch (std::exception& e) { return {}; }

    for (const auto& el : std::filesystem::directory_iterator(getRuntimeDir())) {
        if (!std::filesystem::exists(el.path() / "hyprland.lock"))
            continue;

        // read lock
        SInstanceData* data = &result.emplace_back();
        data->id            = el.path().filename().string();

        try {
            data->time = std::stoull(data->id.substr(data->id.find_first_of('_') + 1, data->id.find_last_of('_') - (data->id.find_first_of('_') + 1)));
        } catch (std::exception& e) { continue; }

        // read file
        std::ifstream ifs(el.path().string() + "/hyprland.lock");

        int           i = 0;
        for (std::string line; std::getline(ifs, line); ++i) {
            if (i == 0) {
                try {
                    data->pid = std::stoull(line);
                } catch (std::exception& e) { continue; }
            } else if (i == 1) {
                data->wlSocket = line;
            } else
                break;
        }

        ifs.close();
    }

    std::erase_if(result, [&](const auto& el) { return kill(el.pid, 0) != 0 && errno == ESRCH; });

    std::ranges::sort(result, [&](const auto& a, const auto& b) { return a.time < b.time; });

    return result;
}

std::string getFromSocket(const std::string& cmd) {
    using namespace std::chrono_literals;

    const std::string SOCKET_PATH = getRuntimeDir() + "/" + HIS + "/.socket.sock";
    const auto        RESULT      = NHyprTester::HyprCtlSocket::request(SOCKET_PATH, cmd, 5s);
    if (RESULT)
        return *RESULT;

    if (RESULT.error().code == ETIMEDOUT)
        std::println("Hyprland IPC didn't respond in time");

    switch (RESULT.error().stage) {
        case NHyprTester::HyprCtlSocket::eErrorStage::SOCKET: std::println("socket: Couldn't open a socket (1)"); break;
        case NHyprTester::HyprCtlSocket::eErrorStage::CONNECT: std::println("Couldn't connect to {}. (3)", SOCKET_PATH); break;
        case NHyprTester::HyprCtlSocket::eErrorStage::WRITE: std::println("Couldn't write (4)"); break;
        case NHyprTester::HyprCtlSocket::eErrorStage::READ: std::println("Couldn't read (5)"); break;
    }

    return "";
}
