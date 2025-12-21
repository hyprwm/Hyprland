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
#include <filesystem>

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
    Tests::killAllWindows();
    client.proc = makeShared<CProcess>(binaryDir + "/shortcut-inhibitor", std::vector<std::string>{});

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

    const int COUNT_BEFORE = Tests::windowCount();
    client.proc->runAsync();

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    client.fds = {.fd = client.readFd.get(), .events = POLLIN};
    if (poll(&client.fds, 1, 1000) != 1 || !(client.fds.revents & POLLIN)) {
        NLog::log("{}shortcut-inhibitor client failed poll", Colors::RED);
        return false;
    }

    client.readBuf.fill(0);
    if (read(client.readFd.get(), client.readBuf.data(), client.readBuf.size() - 1) == -1) {
        NLog::log("{}shortcut-inhibitor client read failed", Colors::RED);
        return false;
    }

    std::string ret = std::string{client.readBuf.data()};
    if (ret.find("started") == std::string::npos) {
        NLog::log("{}Failed to start shortcut-inhibitor client, read {}", Colors::RED, ret);
        return false;
    }

    // wait for window to appear
    int counter = 0;
    while (Tests::processAlive(client.proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}shortcut-inhibitor client took too long to open", Colors::RED);
            return false;
        }
    }

    if (!Tests::processAlive(client.proc->pid())) {
        NLog::log("{}shortcut-inhibitor client not alive", Colors::RED);
        return false;
    }

    if (getFromSocket(std::format("/dispatch focuswindow pid:{}", client.proc->pid())) != "ok") {
        NLog::log("{}Failed to focus shortcut-inhibitor client", Colors::RED, ret);
        return false;
    }

    std::string command = "on\n";
    if (write(client.writeFd.get(), command.c_str(), command.length()) == -1) {
        NLog::log("{}shortcut-inhibitor client write failed", Colors::RED);
        return false;
    }

    client.readBuf.fill(0);
    if (read(client.readFd.get(), client.readBuf.data(), client.readBuf.size() - 1) == -1)
        return false;

    ret = std::string{client.readBuf.data()};
    if (ret.find("inhibiting") == std::string::npos) {
        NLog::log("{}shortcut-inhibitor client didn't return inhibiting", Colors::RED);
        return false;
    }

    NLog::log("{}Started shortcut-inhibitor client", Colors::YELLOW);

    return true;
}

static void stopClient(SClient& client) {
    std::string cmd = "off\n";
    write(client.writeFd.get(), cmd.c_str(), cmd.length());

    kill(client.proc->pid(), SIGKILL);
    client.proc.reset();
}

static std::string flagFile = "/tmp/hyprtester-keybinds.txt";

static bool        checkFlag() {
    bool exists = std::filesystem::exists(flagFile);
    std::filesystem::remove(flagFile);
    return exists;
}

static bool attemptCheckFlag(int attempts, int intervalMs) {
    for (int i = 0; i < attempts; i++) {
        if (checkFlag())
            return true;

        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }

    return false;
}

static bool test() {
    SClient client;
    if (!startClient(client))
        return false;

    NLog::log("{}Testing keybinds", Colors::GREEN);
    //basic keybind test
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bind SUPER,Y,exec,touch " + flagFile), "ok");
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    EXPECT(attemptCheckFlag(20, 50), false);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");

    //keybind bypass flag test
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/keyword bindp SUPER,Y,exec,touch " + flagFile), "ok");
    OK(getFromSocket("/dispatch plugin:test:keybind 1,7,29"));
    EXPECT(attemptCheckFlag(20, 50), true);
    OK(getFromSocket("/dispatch plugin:test:keybind 0,0,29"));
    EXPECT(getFromSocket("/keyword unbind SUPER,Y"), "ok");

    NLog::log("{}Testing gestures", Colors::GREEN);
    //basic gesture test
    OK(getFromSocket("/dispatch plugin:test:gesture right,3"));
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), "floating: 1");

    //gesture bypass flag test
    OK(getFromSocket("/dispatch plugin:test:gesture right,2"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "floating: 1");

    stopClient(client);

    NLog::log("{}Reloading the config", Colors::YELLOW);
    OK(getFromSocket("/reload"));

    return !ret;
}

REGISTER_CLIENT_TEST_FN(test);
