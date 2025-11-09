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

static int ret = 0;

static bool startClient(SClient& client) {
    client.proc = makeShared<CProcess>(binaryDir + "/child-window", std::vector<std::string>{});

    client.proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int procInPipeFd[2], procOutPipeFd[2];
    if (pipe(procInPipeFd) != 0 || pipe(procOutPipeFd) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        return false;
    }

    client.writeFd = CFileDescriptor(procInPipeFd[1]);
    client.proc->setStdinFD(procInPipeFd[0]);

    client.readFd = CFileDescriptor(procOutPipeFd[0]);
    client.proc->setStdoutFD(procOutPipeFd[1]);

    client.proc->runAsync();

    close(procInPipeFd[0]);
    close(procOutPipeFd[1]);

    client.fds = {.fd = client.readFd.get(), .events = POLLIN};
    if (poll(&client.fds, 1, 1000) != 1 || !(client.fds.revents & POLLIN))
        return false;

    client.readBuf.fill(0);
    if (read(client.readFd.get(), client.readBuf.data(), client.readBuf.size() - 1) == -1)
        return false;

    std::string startStr = std::string{client.readBuf.data()};
    if (startStr.find("started") == std::string::npos) {
        NLog::log("{}Failed to start child-window client, read {}", Colors::RED, startStr);
        return false;
    }

    // wait for window to appear
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    NLog::log("{}Started child-window client", Colors::YELLOW);
    return true;
}

static void stopClient(SClient& client) {
    std::string cmd = "exit\n";
    write(client.writeFd.get(), cmd.c_str(), cmd.length());

    kill(client.proc->pid(), SIGKILL);
    client.proc.reset();
}

static bool createChild(SClient& client) {
    std::string cmd = "toplevel\n";
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

    // wait for window to appear
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    if (getFromSocket("/dispatch focuswindow class:child-test-child") != "ok") {
        NLog::log("{}Failed to focus child window", Colors::RED);
        return false;
    }


    return true;
}

static bool test() {
    SClient client;

    OK(getFromSocket("/keyword windowrule float, pin, class:child-test-parent"));
    if (!startClient(client))
        return false;

    createChild(client);
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "pinned: 1");

    stopClient(client);
    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_CLIENT_TEST_FN(test);