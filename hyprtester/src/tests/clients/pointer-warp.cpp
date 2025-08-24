#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <sys/poll.h>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

struct SClient {
    SP<CProcess>           proc;
    std::array<char, 1024> readBuf;
    CFileDescriptor        readFd, writeFd;
    struct pollfd          fds;
};

//
static bool startClient(SClient& client) {
    client.proc = makeShared<CProcess>(binaryDir + "/pointer-warp", std::vector<std::string>{});

    int pipeFds1[2];
    if (pipe(pipeFds1) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        return false;
    }

    client.writeFd = CFileDescriptor(pipeFds1[1]);
    client.proc->setStdinFD(pipeFds1[0]);

    int pipeFds2[2];
    if (pipe(pipeFds2) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        return false;
    }

    client.readFd = CFileDescriptor(pipeFds2[0]);
    client.proc->setStdoutFD(pipeFds2[1]);

    client.proc->runAsync();
    NLog::log("{}Launched pointer-warp client", Colors::YELLOW);

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    client.readBuf.fill(0);
    client.fds = {.fd = client.readFd.get(), .events = POLLIN};
    if (poll(&client.fds, 1, 1000) != 1 || !(client.fds.revents & POLLIN))
        return false;

    std::string ret = std::string{client.readBuf.data()};
    if (ret.find("started") == std::string::npos) {
        NLog::log("{}Failed to start pointer-warp client, read {}", Colors::RED, ret);
        return false;
    }

    NLog::log("{}Started pointer-warp client", Colors::YELLOW);

    return true;
}

// format is like below
// "warp 20 20\n" would ask to warp cursor to x=20,y=20 in surface local coords
static bool testWarp(SClient& client, int x, int y) {
    std::string cmd = std::format("warp {} {}\n", x, y);
    NLog::log("{}pointer-warp: sending client command \"{}\"", Colors::YELLOW, cmd);
    if ((size_t)write(client.writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    // we just want to know that the request went through, data isn't important
    if (poll(&client.fds, 1, 500) != 1 || !(client.fds.revents & POLLIN))
        return false;
    if (read(client.fds.fd, client.readBuf.data(), 1023) == -1)
        return false;

    NLog::log("{}pointer-warp: client recieved command", Colors::YELLOW);

    std::string res = getFromSocket("/cursorpos");
    if (res == "error")
        return false;

    auto it = res.find_first_of(' ');
    if (res.at(it - 1) != ',')
        return false;

    int resX = std::stoi(res.substr(0, it - 1));
    int resY = std::stoi(res.substr(it + 1));

    return resX == x && resY == y;
}

static bool test() {
    SClient client;

    if (!startClient(client))
        return false;

    int ret;

    EXPECT(testWarp(client, 100, 100), true);

    return true;
}

REGISTER_CLIENT_TEST_FN(test);
