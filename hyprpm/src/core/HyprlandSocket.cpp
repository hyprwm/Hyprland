#include "HyprlandSocket.hpp"
#include <pwd.h>
#include <sys/socket.h>
#include "../helpers/StringUtils.hpp"
#include <print>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

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

    if (connect(SERVERSOCKET, reinterpret_cast<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        std::println("{}", failureString("Couldn't connect to " + socketPath + ". (4)"));
        return "";
    }

    auto sizeWritten = write(SERVERSOCKET, cmd.c_str(), cmd.length());

    if (sizeWritten < 0) {
        std::println("{}", failureString("Couldn't write (5)"));
        return "";
    }

    std::string      reply               = "";
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, BUFFER_SIZE);

    if (sizeWritten < 0) {
        std::println("{}", failureString("Couldn't read (6)"));
        return "";
    }

    reply += std::string(buffer, sizeWritten);

    while (sizeWritten == BUFFER_SIZE) {
        sizeWritten = read(SERVERSOCKET, buffer, BUFFER_SIZE);
        if (sizeWritten < 0) {
            std::println("{}", failureString("Couldn't read (7)"));
            return "";
        }
        reply += std::string(buffer, sizeWritten);
    }

    close(SERVERSOCKET);

    return reply;
}
