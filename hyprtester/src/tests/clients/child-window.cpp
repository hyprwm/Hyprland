#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <optional>
#include <sys/poll.h>
#include <unistd.h>
#include <csignal>
#include <thread>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

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

namespace {
    class CClient {
        SP<CProcess>           proc;
        std::array<char, 1024> readBuf;
        CFileDescriptor        readFd, writeFd;
        struct pollfd          fds;

      public:
        CClient();
        ~CClient();
        bool createChild();
    };
}

CClient::CClient() {
    NLog::log("{}Attempting to start child-window client", Colors::YELLOW);

    this->proc = makeShared<CProcess>(binaryDir + "/child-window", std::vector<std::string>{});

    this->proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int procInPipeFd[2], procOutPipeFd[2];
    if (pipe(procInPipeFd) != 0 || pipe(procOutPipeFd) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        throw std::exception();
    }

    this->writeFd = CFileDescriptor(procInPipeFd[1]);
    this->proc->setStdinFD(procInPipeFd[0]);

    this->readFd = CFileDescriptor(procOutPipeFd[0]);
    this->proc->setStdoutFD(procOutPipeFd[1]);

    if (!this->proc->runAsync()) {
        NLog::log("{}Failed to run client", Colors::RED);
        throw std::exception();
    }

    close(procInPipeFd[0]);
    close(procOutPipeFd[1]);

    if (!waitForWindow(this->proc, Tests::windowCount())) {
        NLog::log("{}Window took too long to open", Colors::RED);
        throw std::exception();
    }

    NLog::log("{}Started child-window client", Colors::YELLOW);
}

CClient::~CClient() {
    std::string cmd = "exit\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());

    kill(this->proc->pid(), SIGKILL);
    this->proc.reset();
}

bool CClient::createChild() {
    std::string cmd = "toplevel\n";
    if ((size_t)write(this->writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    if (!waitForWindow(this->proc, Tests::windowCount()))
        NLog::log("{}Child window took too long to open", Colors::RED);

    if (getFromSocket("/dispatch hl.dsp.focus({ window = 'class:child-test-child' })") != "ok") {
        NLog::log("{}Failed to focus child window", Colors::RED);
        return false;
    }

    return true;
}

static bool test() {
    {
        std::optional<CClient> client;
        try {
            client.emplace();
        } catch (...) { return false; }

        OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'set', window = 'class:child-test-parent' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.pin({ action = 'set', window = 'class:child-test-parent' })"));

        client->createChild();
        EXPECT(Tests::windowCount(), 2)
        EXPECT_COUNT_STRING(getFromSocket("/clients"), "pinned: 1", 2);
    }

    NLog::log("{}Reloading config", Colors::YELLOW);
    OK(getFromSocket("/reload"));
    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    // test that child windows (shouldBeFloated) are not auto-grouped
    NLog::log("{}Test child windows are not auto-grouped", Colors::GREEN);
    auto kitty = Tests::spawnKitty();
    if (!kitty) {
        NLog::log("{}Error: kitty did not spawn", Colors::RED);
        return false;
    }

    // create group and enable auto-grouping
    OK(getFromSocket("/dispatch hl.dsp.group.toggle()"));
    OK(getFromSocket("/eval hl.config({ group = { auto_group = true } })"));

    {
        std::optional<CClient> client2;
        try {
            client2.emplace();
        } catch (...) { return false; }

        EXPECT(Tests::windowCount(), 2);
        client2->createChild();
        EXPECT(Tests::windowCount(), 3);

        // child has set_parent so shouldBeFloated returns true, it should not be auto-grouped
        EXPECT_COUNT_STRING(getFromSocket("/clients"), "grouped: 0", 1);
    }

    Tests::killAllWindows();
    EXPECT(Tests::windowCount(), 0);

    return !ret;
}

REGISTER_CLIENT_TEST_FN(test);
