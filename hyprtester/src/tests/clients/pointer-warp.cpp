#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <sys/poll.h>
#include <csignal>
#include <thread>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

struct SClient {
    SP<CProcess>           proc;
    std::array<char, 1024> readBuf;
    CFileDescriptor        readFd, writeFd;
    struct pollfd          fds;
};

static int  ret = 0;

static bool startClient(SClient& client) {
    client.proc = makeShared<CProcess>(binaryDir + "/pointer-warp", std::vector<std::string>{});

    client.proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int pipeFds1[2], pipeFds2[2];
    if (pipe(pipeFds1) != 0 || pipe(pipeFds2) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        return false;
    }

    client.writeFd = CFileDescriptor(pipeFds1[1]);
    client.proc->setStdinFD(pipeFds1[0]);

    client.readFd = CFileDescriptor(pipeFds2[0]);
    client.proc->setStdoutFD(pipeFds2[1]);

    client.proc->runAsync();

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    client.fds = {.fd = client.readFd.get(), .events = POLLIN};
    if (poll(&client.fds, 1, 1000) != 1 || !(client.fds.revents & POLLIN))
        return false;

    client.readBuf.fill(0);
    if (read(client.readFd.get(), client.readBuf.data(), client.readBuf.size() - 1) == -1)
        return false;

    std::string ret = std::string{client.readBuf.data()};
    if (ret.find("started") == std::string::npos) {
        NLog::log("{}Failed to start pointer-warp client, read {}", Colors::RED, ret);
        return false;
    }

    // wait for window to appear
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    if (getFromSocket(std::format("/dispatch setprop pid:{} no_anim 1", client.proc->pid())) != "ok") {
        NLog::log("{}Failed to disable animations for client window", Colors::RED, ret);
        return false;
    }

    if (getFromSocket(std::format("/dispatch focuswindow pid:{}", client.proc->pid())) != "ok") {
        NLog::log("{}Failed to focus pointer-warp client", Colors::RED, ret);
        return false;
    }

    NLog::log("{}Started pointer-warp client", Colors::YELLOW);

    return true;
}

static void stopClient(SClient& client) {
    std::string cmd = "exit\n";
    write(client.writeFd.get(), cmd.c_str(), cmd.length());

    kill(client.proc->pid(), SIGKILL);
    client.proc.reset();
}

// format is like below
// "warp 20 20\n" would ask to warp cursor to x=20,y=20 in surface local coords
static bool sendWarp(SClient& client, int x, int y) {
    std::string cmd = std::format("warp {} {}\n", x, y);
    if ((size_t)write(client.writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    if (poll(&client.fds, 1, 1500) != 1 || !(client.fds.revents & POLLIN))
        return false;
    ssize_t bytesRead = read(client.fds.fd, client.readBuf.data(), 1023);
    if (bytesRead == -1)
        return false;

    client.readBuf[bytesRead] = 0;
    std::string recieved      = std::string{client.readBuf.data()};
    recieved.pop_back();

    return true;
}

static bool isCursorPos(int x, int y) {
    // TODO: add a better way to do this using test plugin?
    std::string res = getFromSocket("/cursorpos");
    if (res == "error") {
        NLog::log("{}Cursorpos err'd: {}", Colors::RED, res);
        return false;
    }

    auto it = res.find_first_of(' ');
    if (res.at(it - 1) != ',') {
        NLog::log("{}Cursorpos err'd: {}", Colors::RED, res);
        return false;
    }

    int cursorX = std::stoi(res.substr(0, it - 1));
    int cursorY = std::stoi(res.substr(it + 1));

    // somehow this is always gives 1 less than surfbox->pos()??
    res = getFromSocket("/activewindow");
    it  = res.find("at: ") + 4;
    res = res.substr(it, res.find_first_of('\n', it) - it);

    it          = res.find_first_of(',');
    int clientX = cursorX - std::stoi(res.substr(0, it)) + 1;
    int clientY = cursorY - std::stoi(res.substr(it + 1)) + 1;

    return clientX == x && clientY == y;
}

static bool test() {
    SClient client;

    if (!startClient(client))
        return false;

    EXPECT(sendWarp(client, 100, 100), true);
    EXPECT(isCursorPos(100, 100), true);

    EXPECT(sendWarp(client, 0, 0), true);
    EXPECT(isCursorPos(0, 0), true);

    EXPECT(sendWarp(client, 200, 200), true);
    EXPECT(isCursorPos(200, 200), true);

    EXPECT(sendWarp(client, 100, -100), true);
    EXPECT(isCursorPos(200, 200), true);

    EXPECT(sendWarp(client, 234, 345), true);
    EXPECT(isCursorPos(234, 345), true);

    EXPECT(sendWarp(client, -1, -1), true);
    EXPECT(isCursorPos(234, 345), true);

    EXPECT(sendWarp(client, 1, -1), true);
    EXPECT(isCursorPos(234, 345), true);

    EXPECT(sendWarp(client, 13, 37), true);
    EXPECT(isCursorPos(13, 37), true);

    EXPECT(sendWarp(client, -100, 100), true);
    EXPECT(isCursorPos(13, 37), true);

    EXPECT(sendWarp(client, -1, 1), true);
    EXPECT(isCursorPos(13, 37), true);

    stopClient(client);

    NLog::log("{}Reloading the config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_CLIENT_TEST_FN(test);
