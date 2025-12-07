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
    client.proc = makeShared<CProcess>(binaryDir + "/pointer-scroll", std::vector<std::string>{});

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
        NLog::log("{}Failed to start pointer-scroll client, read {}", Colors::RED, ret);
        return false;
    }

    // wait for window to appear
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    if (getFromSocket(std::format("/dispatch setprop pid:{} no_anim 1", client.proc->pid())) != "ok") {
        NLog::log("{}Failed to disable animations for client window", Colors::RED, ret);
        return false;
    }

    if (getFromSocket(std::format("/dispatch focuswindow pid:{}", client.proc->pid())) != "ok") {
        NLog::log("{}Failed to focus pointer-scroll client", Colors::RED, ret);
        return false;
    }

    NLog::log("{}Started pointer-scroll client", Colors::YELLOW);

    return true;
}

static void stopClient(SClient& client) {
    std::string cmd = "exit\n";
    write(client.writeFd.get(), cmd.c_str(), cmd.length());

    kill(client.proc->pid(), SIGKILL);
    client.proc.reset();
}

static int getLastDelta(SClient& client) {
    std::string cmd = "hypr";
    if ((size_t)write(client.writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    if (poll(&client.fds, 1, 1500) != 1 || !(client.fds.revents & POLLIN))
        return false;
    ssize_t bytesRead = read(client.fds.fd, client.readBuf.data(), 1023);
    if (bytesRead == -1)
        return false;

    client.readBuf[bytesRead] = 0;
    std::string received      = std::string{client.readBuf.data()};
    received.pop_back();

    try {
        return std::stoi(received);
    } catch (...) { return -1; }
}

static bool sendScroll(int delta) {
    return getFromSocket(std::format("/dispatch plugin:test:scroll {}", delta)) == "ok";
}

static bool test() {
    SClient client;

    if (!startClient(client))
        return false;

    EXPECT(getFromSocket("/keyword input:emulate_discrete_scroll 0"), "ok");

    EXPECT(sendScroll(10), true);
    EXPECT(getLastDelta(client), 10);

    EXPECT(getFromSocket("/keyword input:scroll_factor 2"), "ok");
    EXPECT(sendScroll(10), true);
    EXPECT(getLastDelta(client), 20);

    EXPECT(getFromSocket("r/keyword device[test-mouse-1]:scroll_factor 3"), "ok");
    EXPECT(sendScroll(10), true);
    EXPECT(getLastDelta(client), 30);

    EXPECT(getFromSocket("r/dispatch setprop active scroll_mouse 4"), "ok");
    EXPECT(sendScroll(10), true);
    EXPECT(getLastDelta(client), 40);

    stopClient(client);

    NLog::log("{}Reloading the config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_CLIENT_TEST_FN(test);
