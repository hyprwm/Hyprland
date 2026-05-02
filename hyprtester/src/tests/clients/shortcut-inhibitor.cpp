#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <optional>
#include <sys/poll.h>
#include <csignal>
#include <thread>
#include <filesystem>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

namespace {

    class CClient {
        SP<CProcess>           proc;
        std::array<char, 1024> readBuf;
        CFileDescriptor        readFd, writeFd;
        struct pollfd          fds;

      public:
        CClient();
        ~CClient();
    };
}

CClient::CClient() {
    Tests::killAllWindows();
    this->proc = makeShared<CProcess>(binaryDir + "/shortcut-inhibitor", std::vector<std::string>{});

    this->proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int pipeFds1[2], pipeFds2[2];
    if (pipe(pipeFds1) != 0 || pipe(pipeFds2) != 0) {
        NLog::log("{}Unable to open pipe to client", Colors::RED);
        throw std::exception();
    }

    this->writeFd = CFileDescriptor(pipeFds1[1]);
    this->proc->setStdinFD(pipeFds1[0]);

    this->readFd = CFileDescriptor(pipeFds2[0]);
    this->proc->setStdoutFD(pipeFds2[1]);

    const int COUNT_BEFORE = Tests::windowCount();
    this->proc->runAsync();

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    this->fds = {.fd = this->readFd.get(), .events = POLLIN};
    if (poll(&this->fds, 1, 1000) != 1 || !(this->fds.revents & POLLIN)) {
        NLog::log("{}shortcut-inhibitor client failed poll", Colors::RED);
        throw std::exception();
    }

    this->readBuf.fill(0);
    if (read(this->readFd.get(), this->readBuf.data(), this->readBuf.size() - 1) == -1) {
        NLog::log("{}shortcut-inhibitor client read failed", Colors::RED);
        throw std::exception();
    }

    std::string ret = std::string{this->readBuf.data()};
    if (ret.find("started") == std::string::npos) {
        NLog::log("{}Failed to start shortcut-inhibitor client, read {}", Colors::RED, ret);
        throw std::exception();
    }

    // wait for window to appear
    int counter = 0;
    while (Tests::processAlive(this->proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}shortcut-inhibitor client took too long to open", Colors::RED);
            throw std::exception();
        }
    }

    if (!Tests::processAlive(this->proc->pid())) {
        NLog::log("{}shortcut-inhibitor client not alive", Colors::RED);
        throw std::exception();
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", this->proc->pid())) != "ok") {
        NLog::log("{}Failed to focus shortcut-inhibitor client", Colors::RED, ret);
        throw std::exception();
    }

    std::string command = "on\n";
    if (write(this->writeFd.get(), command.c_str(), command.length()) == -1) {
        NLog::log("{}shortcut-inhibitor client write failed", Colors::RED);
        throw std::exception();
    }

    this->readBuf.fill(0);
    if (read(this->readFd.get(), this->readBuf.data(), this->readBuf.size() - 1) == -1)
        throw std::exception();

    ret = std::string{this->readBuf.data()};
    if (ret.find("inhibiting") == std::string::npos) {
        NLog::log("{}shortcut-inhibitor client didn't return inhibiting", Colors::RED);
        throw std::exception();
    }

    NLog::log("{}Started shortcut-inhibitor client", Colors::YELLOW);
}

CClient::~CClient() {
    std::string cmd = "off\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());

    kill(this->proc->pid(), SIGKILL);
    this->proc.reset();
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

TEST_CASE(shortcutInhibitor) {
    std::optional<CClient> client;

    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    NLog::log("{}Testing keybinds", Colors::GREEN);
    //basic keybind test
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'))"), "ok");
    OK(getFromSocket("/eval hl.plugin.test.keybind(1, 7, 29)"));
    EXPECT(attemptCheckFlag(20, 50), false);
    OK(getFromSocket("/eval hl.plugin.test.keybind(0, 0, 29)"));
    EXPECT(getFromSocket("/eval hl.unbind('SUPER + Y')"), "ok");

    //keybind bypass flag test
    EXPECT(checkFlag(), false);
    EXPECT(getFromSocket("/eval hl.bind('SUPER + Y', hl.dsp.exec_cmd('touch " + flagFile + "'), { dont_inhibit = true })"), "ok");
    OK(getFromSocket("/eval hl.plugin.test.keybind(1, 7, 29)"));
    EXPECT(attemptCheckFlag(20, 50), true);
    OK(getFromSocket("/eval hl.plugin.test.keybind(0, 0, 29)"));
    EXPECT(getFromSocket("/eval hl.unbind('SUPER + Y')"), "ok");

    NLog::log("{}Testing gestures", Colors::GREEN);
    //basic gesture test
    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 3)"));
    EXPECT_NOT_CONTAINS(getFromSocket("/activewindow"), "floating: 1");

    //gesture bypass flag test
    OK(getFromSocket("/eval hl.plugin.test.gesture('right', 2)"));
    EXPECT_CONTAINS(getFromSocket("/activewindow"), "floating: 1");
}
