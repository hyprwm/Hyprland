#include "hyprctlCompat.hpp"
#include "shared.hpp"

#include <pwd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <csignal>
#include <cerrno>
#include <format>
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

    if (!XDG)
        return std::format("/run/user/{}/hypr", getUID());

    return std::format("{}/hypr", XDG);
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
        std::ifstream ifs(std::format("{}/hyprland.lock", el.path().string()));

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
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    auto       t = timeval{.tv_sec = 5, .tv_usec = 0};
    setsockopt(SERVERSOCKET, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval));

    if (SERVERSOCKET < 0) {
        std::println("socket: Couldn't open a socket (1)");
        return "";
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = std::format("{}/{}/.socket.sock", getRuntimeDir(), HIS);

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, rc<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        std::println("Couldn't connect to {}. (3)", socketPath);
        return "";
    }

    auto sizeWritten = write(SERVERSOCKET, cmd.c_str(), cmd.length());

    if (sizeWritten < 0) {
        std::println("Couldn't write (4)");
        return "";
    }

    std::string reply        = "";
    char        buffer[8192] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, 8192);

    if (sizeWritten < 0) {
        if (errno == EWOULDBLOCK)
            std::println("Hyprland IPC didn't respond in time");
        std::println("Couldn't read (5)");
        return "";
    }

    reply += std::string(buffer, sizeWritten);

    while (sizeWritten == 8192) {
        sizeWritten = read(SERVERSOCKET, buffer, 8192);
        if (sizeWritten < 0) {
            std::println("Couldn't read (5)");
            return "";
        }
        reply += std::string(buffer, sizeWritten);
    }

    close(SERVERSOCKET);

    return reply;
}
