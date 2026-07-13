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
        uint32_t getLockedMods();
        pid_t    pid();
    };
}

CClient::CClient() {
    Tests::killAllWindows();
    this->proc = makeShared<CProcess>(binaryDir + "/keyboard-modifiers", std::vector<std::string>{});

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
        NLog::log("{}keyboard-modifiers client failed poll", Colors::RED);
        throw std::exception();
    }

    this->readBuf.fill(0);
    if (read(this->readFd.get(), this->readBuf.data(), this->readBuf.size() - 1) == -1) {
        NLog::log("{}keyboard-modifiers client read failed", Colors::RED);
        throw std::exception();
    }

    std::string ret = std::string{this->readBuf.data()};
    if (ret.find("started") == std::string::npos) {
        NLog::log("{}Failed to start keyboard-modifiers client, read {}", Colors::RED, ret);
        throw std::exception();
    }

    int counter = 0;
    while (Tests::processAlive(this->proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}keyboard-modifiers client took too long to open", Colors::RED);
            throw std::exception();
        }
    }

    if (!Tests::processAlive(this->proc->pid())) {
        NLog::log("{}keyboard-modifiers client not alive", Colors::RED);
        throw std::exception();
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", this->proc->pid())) != "ok") {
        NLog::log("{}Failed to focus keyboard-modifiers client", Colors::RED);
        throw std::exception();
    }

    NLog::log("{}Started keyboard-modifiers client", Colors::YELLOW);
}

CClient::~CClient() {
    getFromSocket("/eval hl.plugin.test.set_mods(0, 0, 0, 0, 0)");
    getFromSocket("/eval hl.plugin.test.set_mods(1, 0, 0, 0, 0)");

    std::string cmd = "exit\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());

    kill(this->proc->pid(), SIGKILL);
    this->proc.reset();
}

uint32_t CClient::getLockedMods() {
    std::string cmd = "get\n";
    if ((size_t)write(this->writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    if (poll(&this->fds, 1, 1500) != 1 || !(this->fds.revents & POLLIN))
        return false;
    ssize_t bytesRead = read(this->fds.fd, this->readBuf.data(), 1023);
    if (bytesRead == -1)
        return false;

    this->readBuf[bytesRead] = 0;
    std::string received     = std::string{this->readBuf.data()};
    received.pop_back();

    try {
        return std::stoul(received);
    } catch (...) { return 0; }
}

pid_t CClient::pid() {
    return this->proc->pid();
}

TEST_CASE(keyboardModifiersMergedOnFocus) {
    NLog::log("{}Testing keyboard modifiers merged on focus", Colors::GREEN);

    std::optional<CClient> client;

    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    EXPECT(client->getLockedMods(), 0u);

    OK(getFromSocket("/eval hl.plugin.test.nullfocus()"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    OK(getFromSocket("/eval hl.plugin.test.set_mods(0, 0, 0, 2, 0)"));
    OK(getFromSocket("/eval hl.plugin.test.set_mods(1, 0, 0, 16, 0)"));

    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", client->pid())) != "ok") {
        FAIL_TEST("Failed to refocus keyboard-modifiers client");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const uint32_t locked = client->getLockedMods();
    NLog::log("{}Client reports locked mods: {}", Colors::BLUE, locked);
    EXPECT(locked, 18u);
}
