#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <sys/poll.h>
#include <unistd.h>
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

static bool waitForWindow(SP<CProcess> proc, int windowsBefore) {
    int counter = 0;
    while (Tests::processAlive(proc->pid()) && Tests::windowCount() == windowsBefore) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50)
            return false;
    }

    NLog::log("{}Waited {} milliseconds for window to open", Colors::YELLOW, counter * 100);
    return Tests::processAlive(proc->pid());
}

static bool startClient(SClient& client) {
    NLog::log("{}Attempting to start child-window client", Colors::YELLOW);

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

    if (!client.proc->runAsync()) {
        NLog::log("{}Failed to run client", Colors::RED);
        return false;
    }

    close(procInPipeFd[0]);
    close(procOutPipeFd[1]);

    if (!waitForWindow(client.proc, Tests::windowCount())) {
        NLog::log("{}Window took too long to open", Colors::RED);
        return false;
    }

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

    if (!waitForWindow(client.proc, Tests::windowCount()))
        NLog::log("{}Child window took too long to open", Colors::RED);

    if (getFromSocket("/dispatch focuswindow class:child-test-child") != "ok") {
        NLog::log("{}Failed to focus child window", Colors::RED);
        return false;
    }

    return true;
}

static bool test() {
    SClient client;

    if (!startClient(client))
        return false;
    OK(getFromSocket("/dispatch setfloating class:child-test-parent"));
    OK(getFromSocket("/dispatch pin class:child-test-parent"));

    createChild(client);
    EXPECT(Tests::windowCount(), 2)
    EXPECT_COUNT_STRING(getFromSocket("/clients"), "pinned: 1", 2);

    stopClient(client);
    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_CLIENT_TEST_FN(test);
