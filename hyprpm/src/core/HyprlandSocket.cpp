#include "HyprlandSocket.hpp"
#include <cerrno>
#include <pwd.h>
#include <sys/socket.h>
#include "../helpers/StringUtils.hpp"
#include <print>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <hyprutils/memory/Casts.hpp>

using namespace Hyprutils::Memory;

static bool writeAll(const int fd, std::string_view data) {
    size_t totalWritten = 0;
    while (totalWritten < data.size()) {
        const auto written = write(fd, data.data() + totalWritten, data.size() - totalWritten);
        if (written > 0) {
            totalWritten += sc<size_t>(written);
            continue;
        }

        if (written < 0 && errno == EINTR)
            continue;

        return false;
    }

    return true;
}

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

std::string NHyprlandSocket::send(const std::string& cmd) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        std::println("{}", failureString("Couldn't open a socket (1)"));
        return "";
    }

    const auto HIS = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!HIS) {
        std::println("{}", failureString("HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?) (3)"));
        return "";
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + HIS + "/.socket.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, rc<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        std::println("{}", failureString("Couldn't connect to " + socketPath + ". (4)"));
        return "";
    }

    std::string framedCmd{cmd};
    framedCmd.push_back('\0');

    if (!writeAll(SERVERSOCKET, framedCmd)) {
        std::println("{}", failureString("Couldn't write (5)"));
        return "";
    }

    std::string      reply               = "";
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    while (true) {
        const auto sizeRead = read(SERVERSOCKET, buffer, BUFFER_SIZE);
        if (sizeRead < 0) {
            if (errno == EINTR)
                continue;

            std::println("{}", failureString("Couldn't read (6)"));
            return "";
        }

        if (sizeRead == 0)
            break;

        reply.append(buffer, sc<size_t>(sizeRead));
    }

    close(SERVERSOCKET);

    return reply;
}
