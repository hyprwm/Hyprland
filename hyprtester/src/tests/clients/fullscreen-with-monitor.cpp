// add to includes
#include "hyprtester/src/hyprctlCompat.hpp"
#include "hyprtester/src/shared.hpp"
#include "hyprtester/src/tests/shared.hpp"
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>
#include <optional>
#include <sys/poll.h>
#include <csignal>
#include <thread>
#include "tests.hpp"
#include "build.hpp"

namespace {
    using namespace Hyprutils::OS;
    using namespace Hyprutils::Memory;
#define SP CSharedPointer

    class CFsMonClient {
        SP<CProcess>           proc;
        std::array<char, 1024> readBuf;
        CFileDescriptor        readFd, writeFd;
        struct pollfd          fds;

      public:
        CFsMonClient();
        ~CFsMonClient();
        bool  isFullscreen();
        void  requestFullscreen();
        void  requestUnFullscreen();
        pid_t pid();
    };

#undef SP
}

CFsMonClient::CFsMonClient() {
    Tests::killAllWindows();
    this->proc = makeShared<CProcess>(std::format("{}/fullscreen-with-monitor", binaryDir), std::vector<std::string>{});
    this->proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int pipeFds1[2], pipeFds2[2];
    if (pipe(pipeFds1) != 0 || pipe(pipeFds2) != 0) {
        NLog::log("{}CFsMonClient: pipe() failed", Colors::RED);
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

    // wait for "started\n" from the client
    if (poll(&this->fds, 1, 2000) != 1 || !(this->fds.revents & POLLIN)) {
        NLog::log("{}CFsMonClient: timed out waiting for start", Colors::RED);
        throw std::exception();
    }
    this->readBuf.fill(0);
    if (read(this->readFd.get(), this->readBuf.data(), this->readBuf.size() - 1) == -1) {
        NLog::log("{}CFsMonClient: read failed", Colors::RED);
        throw std::exception();
    }
    if (!std::string_view{this->readBuf.data()}.contains("started")) {
        NLog::log("{}CFsMonClient: unexpected startup output: {}", Colors::RED, this->readBuf.data());
        throw std::exception();
    }

    // wait for the window to actually appear in the compositor
    int counter = 0;
    while (Tests::processAlive(this->proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (++counter > 50) {
            NLog::log("{}CFsMonClient: window never appeared", Colors::RED);
            throw std::exception();
        }
    }
    if (!Tests::processAlive(this->proc->pid())) {
        NLog::log("{}CFsMonClient: process died before window appeared", Colors::RED);
        throw std::exception();
    }

    NLog::log("{}CFsMonClient ready", Colors::YELLOW);
}

CFsMonClient::~CFsMonClient() {
    std::string cmd = "exit\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());
    kill(this->proc->pid(), SIGKILL);
    this->proc.reset();
}

void CFsMonClient::requestFullscreen() {
    std::string cmd = "fullscreen\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());
    // give the round-trip time to complete:
    // client -> compositor (set_fullscreen with output) -> compositor sends configure -> client acks
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

void CFsMonClient::requestUnFullscreen() {
    std::string cmd = "unfullscreen\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

bool CFsMonClient::isFullscreen() {
    std::string cmd = "get\n";
    if ((size_t)write(this->writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    if (poll(&this->fds, 1, 1500) != 1 || !(this->fds.revents & POLLIN))
        return false;

    this->readBuf.fill(0);
    ssize_t n = read(this->fds.fd, this->readBuf.data(), this->readBuf.size() - 1);
    if (n <= 0)
        return false;

    this->readBuf[n] = 0;
    return std::string{this->readBuf.data()}.contains('1');
}

TEST_CASE(fullscreenWithExplicitMonitor) {
    NLog::log("{}Testing xdg_toplevel_set_fullscreen with explicit wl_output", Colors::GREEN);

    // move to a special workspace for the test
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = 'special:A', follow = true })"));

    std::optional<CFsMonClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Failed to start fullscreen-with-monitor client"); }

    // sanity: window should not be fullscreen before we ask
    EXPECT(client->isFullscreen(), false);

    client->requestFullscreen();

    // client parsed the fullscreen state from the configure wl_array
    EXPECT(client->isFullscreen(), true);

    // unFullscreen
    client->requestUnFullscreen();
    EXPECT(client->isFullscreen(), false);

    // expect the client to be in the special workspace still

    EXPECT_CONTAINS(getFromSocket("/clients"), "workspace: special:A (special:A)")
}
