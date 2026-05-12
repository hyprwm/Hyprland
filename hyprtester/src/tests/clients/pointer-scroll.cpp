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

namespace {
    class CClient {
        SP<CProcess>           proc;
        std::array<char, 1024> readBuf;
        CFileDescriptor        readFd, writeFd;
        struct pollfd          fds;

      public:
        CClient();
        ~CClient();
        int getLastDelta();
    };
}

CClient::CClient() {
    this->proc = makeShared<CProcess>(binaryDir + "/pointer-scroll", std::vector<std::string>{});

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
    if (poll(&this->fds, 1, 1000) != 1 || !(this->fds.revents & POLLIN))
        throw std::exception();

    this->readBuf.fill(0);
    if (read(this->readFd.get(), this->readBuf.data(), this->readBuf.size() - 1) == -1)
        throw std::exception();

    std::string ret = std::string{this->readBuf.data()};
    if (ret.find("started") == std::string::npos) {
        NLog::log("{}Failed to start pointer-scroll client, read {}", Colors::RED, ret);
        throw std::exception();
    }

    // wait for window to appear
    int counter = 0;
    while (Tests::processAlive(this->proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}pointer-scroll client took too long to open", Colors::RED);
            throw std::exception();
        }
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.window.set_prop({{ window = 'pid:{}', prop = 'no_anim', value = '1' }})", this->proc->pid())) != "ok") {
        NLog::log("{}Failed to disable animations for client window", Colors::RED, ret);
        throw std::exception();
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", this->proc->pid())) != "ok") {
        NLog::log("{}Failed to focus pointer-scroll client", Colors::RED, ret);
        throw std::exception();
    }

    NLog::log("{}Started pointer-scroll client", Colors::YELLOW);
}

CClient::~CClient() {
    std::string cmd = "exit\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());

    kill(this->proc->pid(), SIGKILL);
    this->proc.reset();
}

int CClient::getLastDelta() {
    std::string cmd = "hypr";
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
        return std::stoi(received);
    } catch (...) { return -1; }
}

static bool sendScroll(int delta) {
    return getFromSocket(std::format("/eval hl.plugin.test.scroll({})", delta)) == "ok";
}

TEST_CASE(pointerScroll) {
    NLog::log("{}Skipping pointerScroll test (unstable in CI / headless environments)", Colors::YELLOW);
    return;

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    EXPECT(getFromSocket("r/eval hl.config({ input = { emulate_discrete_scroll = 0 } })"), "ok");

    EXPECT(sendScroll(10), true);
    EXPECT(client->getLastDelta(), 10);

    EXPECT(getFromSocket("r/eval hl.config({ input = { scroll_factor = 2 } })"), "ok");
    EXPECT(sendScroll(10), true);
    EXPECT(client->getLastDelta(), 20);

    EXPECT(getFromSocket("r/eval hl.device({ name = 'test-mouse-1', scroll_factor = 3 })"), "ok");
    EXPECT(sendScroll(10), true);
    EXPECT(client->getLastDelta(), 30);

    EXPECT(getFromSocket("r/dispatch hl.dsp.window.set_prop({ window = 'active', prop = 'scroll_mouse', value = '4' })"), "ok");
    EXPECT(sendScroll(10), true);
    EXPECT(client->getLastDelta(), 40);

    NLog::log("{}Reloading the config", Colors::YELLOW);
    OK(getFromSocket("/reload"));
}
