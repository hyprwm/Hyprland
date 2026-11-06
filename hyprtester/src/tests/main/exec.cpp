#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"

#include <pwd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <chrono>
#include <format>
#include <thread>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include "../shared.hpp"

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define UP CUniquePointer
#define SP CSharedPointer

const static auto SLEEP_DURATIONS = std::array{1, 10};

TEST_CASE(processSpawning) {
    for (const auto duration : SLEEP_DURATIONS) {
        // Note: POSIX sleep does not support fractional seconds, so
        // can't sleep for less than 1 second.
        OK(getFromSocket(std::format("/dispatch hl.dsp.exec_cmd('sleep {}')", duration)));

        // Ensure that sleep is our child
        const std::string sleepPidS = Tests::execAndGet("pgrep sleep");
        pid_t             sleepPid;
        try {
            sleepPid = std::stoull(sleepPidS);
        } catch (...) {
            NLog::log("{}Sleep was not spawned or several sleeps are running: pgrep returned '{}'", Colors::RED, sleepPidS);
            continue;
        }

        const std::string sleepParentComm = Tests::execAndGet(std::format("cat \"/proc/$(ps -o ppid:1= -p {})/comm\"", sleepPidS));
        NLog::log("{}Expecting that sleep's parent is Hyprland", Colors::YELLOW);
        EXPECT_CONTAINS(sleepParentComm, "Hyprland");

        std::this_thread::sleep_for(std::chrono::seconds(duration));

        // Ensure that sleep did not become a zombie
        EXPECT(Tests::processAlive(sleepPid), false);

        // Test succeeded
        return;
    }

    FAIL_TEST_SILENT();
}

TEST_CASE(luaProcessSpawning) {
    const auto DURATION = 1;

    const auto COMMAND = std::format("/repl os.execute('sleep {}')", DURATION);

    // Custom socket stuff, copied and stripped down from getFromSocket()
    // We need this so we can check ps while HL is blocked, before the socket response comes back
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    const auto SOCK_TIMEOUT = 5;
    static_assert(SOCK_TIMEOUT > DURATION);
    auto t = timeval{.tv_sec = 5, .tv_usec = 0};
    setsockopt(SERVERSOCKET, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval));

    std::string reply = "";

    if (SERVERSOCKET < 0) {
        std::println("socket: Couldn't open a socket (1)");
        reply = "";
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    const auto RUNTIMEDIR = [] {
        const auto XDG = getenv("XDG_RUNTIME_DIR");
        if (!XDG) {
            const auto UID        = getuid();
            const auto PWUID      = getpwuid(UID);
            const auto ACTUAL_UID = PWUID ? PWUID->pw_uid : UID;
            return std::format("/run/user/{}/hypr", ACTUAL_UID);
        }
        return std::format("{}/hypr", XDG);
    }();
    std::string socketPath = std::format("{}/{}/.socket.sock", RUNTIMEDIR, HIS);

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, rc<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        std::println("Couldn't connect to {}. (3)", socketPath);
        reply = "";
    }

    auto sizeWritten = write(SERVERSOCKET, COMMAND.c_str(), COMMAND.length());

    if (sizeWritten < 0) {
        std::println("Couldn't write (4)");
        reply = "";
    }

    // Do stuff here, before we try to read the response

    // Ensure that sleep is our child
    const std::string sleepPidS = Tests::execAndGet("pgrep sleep");
    pid_t             sleepPid;
    try {
        sleepPid = std::stoull(sleepPidS);
    } catch (...) { NLog::log("{}Sleep was not spawned or several sleeps are running: pgrep returned '{}'", Colors::RED, sleepPidS); }

    const std::string sleepParentComm = Tests::execAndGet(std::format("cat \"/proc/$(ps -o ppid:1= -p {})/comm\"", sleepPidS));
    NLog::log("{}Expecting that sleep's parent is Hyprland", Colors::YELLOW);
    EXPECT_CONTAINS(sleepParentComm, "Hyprland");

    // Now read
    char buffer[8192] = {0};
    sizeWritten       = read(SERVERSOCKET, buffer, 8192);

    if (sizeWritten < 0) {
        if (errno == EWOULDBLOCK)
            std::println("Hyprland IPC didn't respond in time");
        std::println("Couldn't read (5)");
        reply = "";
    }

    reply += std::string(buffer, sizeWritten);

    while (sizeWritten == 8192) {
        sizeWritten = read(SERVERSOCKET, buffer, 8192);
        if (sizeWritten < 0) {
            std::println("Couldn't read (5)");
            reply = "";
        }
        reply += std::string(buffer, sizeWritten);
    }

    close(SERVERSOCKET);
    // End custom socket stuff

    ASSERT(reply, "true\texit\t0");

    // Ensure that sleep did not become a zombie
    EXPECT(Tests::processAlive(sleepPid), false);
}
