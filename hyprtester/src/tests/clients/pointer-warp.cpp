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
        bool sendWarp(int x, int y);
    };
}

CClient::CClient() {
    this->proc = makeShared<CProcess>(binaryDir + "/pointer-warp", std::vector<std::string>{});

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
        NLog::log("{}Failed to start pointer-warp client, read {}", Colors::RED, ret);
        throw std::exception();
    }

    // wait for window to appear
    int counter = 0;
    while (Tests::processAlive(this->proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 100) {
            NLog::log("{}pointer-warp client took too long to open, continuing", Colors::YELLOW);
            throw std::exception();
        }
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.window.set_prop({{ window = 'pid:{}', prop = 'no_anim', value = '1' }})", this->proc->pid())) != "ok") {
        NLog::log("{}Failed to disable animations for client window", Colors::RED, ret);
        throw std::exception();
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", this->proc->pid())) != "ok") {
        NLog::log("{}Failed to focus pointer-warp client", Colors::RED, ret);
        throw std::exception();
    }

    NLog::log("{}Started pointer-warp client", Colors::YELLOW);
}

CClient::~CClient() {
    std::string cmd = "exit\n";
    write(this->writeFd.get(), cmd.c_str(), cmd.length());

    kill(this->proc->pid(), SIGKILL);
    this->proc.reset();
}

// format is like below
// "warp 20 20\n" would ask to warp cursor to x=20,y=20 in surface local coords
bool CClient::sendWarp(int x, int y) {
    std::string cmd = std::format("warp {} {}\n", x, y);
    if ((size_t)write(this->writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return false;

    if (poll(&this->fds, 1, 1500) != 1 || !(this->fds.revents & POLLIN))
        return false;
    ssize_t bytesRead = read(this->fds.fd, this->readBuf.data(), 1023);
    if (bytesRead == -1)
        return false;

    this->readBuf[bytesRead] = 0;
    std::string recieved     = std::string{this->readBuf.data()};
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

TEST_CASE(pointerWarp) {
    NLog::log("{}Skipping pointerWarp test (unstable in CI / headless environments)", Colors::YELLOW);
    return;

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the client"); }

    EXPECT(client->sendWarp(100, 100), true);
    EXPECT(isCursorPos(100, 100), true);

    EXPECT(client->sendWarp(0, 0), true);
    EXPECT(isCursorPos(0, 0), true);

    EXPECT(client->sendWarp(200, 200), true);
    EXPECT(isCursorPos(200, 200), true);

    EXPECT(client->sendWarp(100, -100), true);
    EXPECT(isCursorPos(200, 200), true);

    EXPECT(client->sendWarp(234, 345), true);
    EXPECT(isCursorPos(234, 345), true);

    EXPECT(client->sendWarp(-1, -1), true);
    EXPECT(isCursorPos(234, 345), true);

    EXPECT(client->sendWarp(1, -1), true);
    EXPECT(isCursorPos(234, 345), true);

    EXPECT(client->sendWarp(13, 37), true);
    EXPECT(isCursorPos(13, 37), true);

    EXPECT(client->sendWarp(-100, 100), true);
    EXPECT(isCursorPos(13, 37), true);

    EXPECT(client->sendWarp(-1, 1), true);
    EXPECT(isCursorPos(13, 37), true);
}
