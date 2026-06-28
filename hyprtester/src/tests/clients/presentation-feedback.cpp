#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "tests.hpp"
#include "build.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <array>
#include <csignal>
#include <exception>
#include <optional>
#include <string>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

namespace {
    class CClient {
      public:
        CClient();
        ~CClient();

        std::string runBurst();
        std::string runDestroy();

      private:
        CSharedPointer<CProcess> m_proc;
        CFileDescriptor          m_readFd, m_writeFd;
        struct pollfd            m_pollFd  = {};
        std::array<char, 4096>   m_readBuf = {};
        std::string              m_output;

        std::string              readUntil(const std::string& needle, int timeoutMs);
    };
}

CClient::CClient() {
    m_proc = makeShared<CProcess>(binaryDir + "/presentation-feedback", std::vector<std::string>{});
    m_proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int pipeFds1[2] = {-1, -1};
    int pipeFds2[2] = {-1, -1};
    if (pipe(pipeFds1) != 0 || pipe(pipeFds2) != 0) {
        NLog::log("{}Unable to open pipe to presentation-feedback client", Colors::RED);
        throw std::exception();
    }

    m_writeFd = CFileDescriptor(pipeFds1[1]);
    m_proc->setStdinFD(pipeFds1[0]);

    m_readFd = CFileDescriptor(pipeFds2[0]);
    m_proc->setStdoutFD(pipeFds2[1]);

    const int COUNT_BEFORE = Tests::windowCount();
    m_proc->runAsync();

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    m_pollFd = {.fd = m_readFd.get(), .events = POLLIN};

    const auto STARTED = readUntil("started", 3000);
    if (!STARTED.contains("started")) {
        NLog::log("{}Failed to start presentation-feedback client, read {}", Colors::RED, STARTED);
        throw std::exception();
    }

    int counter = 0;
    while (Tests::processAlive(m_proc->pid()) && Tests::windowCount() == COUNT_BEFORE) {
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (counter > 50) {
            NLog::log("{}presentation-feedback client took too long to open", Colors::RED);
            throw std::exception();
        }
    }

    if (getFromSocket(std::format("/dispatch hl.dsp.window.set_prop({{ window = 'pid:{}', prop = 'no_anim', value = '1' }})", m_proc->pid())) != "ok") {
        NLog::log("{}Failed to disable animations for presentation-feedback client", Colors::RED);
        throw std::exception();
    }

    NLog::log("{}Started presentation-feedback client", Colors::YELLOW);
}

CClient::~CClient() {
    const std::string CMD = "exit\n";
    write(m_writeFd.get(), CMD.c_str(), CMD.length());

    if (m_proc)
        kill(m_proc->pid(), SIGKILL);
}

std::string CClient::readUntil(const std::string& needle, int timeoutMs) {
    const auto DEADLINE = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (!m_output.contains(needle) && std::chrono::steady_clock::now() < DEADLINE) {
        if (poll(&m_pollFd, 1, 100) != 1 || !(m_pollFd.revents & POLLIN))
            continue;

        const ssize_t BYTES = read(m_readFd.get(), m_readBuf.data(), m_readBuf.size());
        if (BYTES <= 0)
            continue;

        m_output.append(m_readBuf.data(), static_cast<size_t>(BYTES));
    }

    return m_output;
}

std::string CClient::runBurst() {
    const std::string CMD = "run\n";
    if (write(m_writeFd.get(), CMD.c_str(), CMD.length()) != static_cast<ssize_t>(CMD.length()))
        return m_output;

    return readUntil("summary", 5000);
}

std::string CClient::runDestroy() {
    const std::string CMD = "destroy\n";
    if (write(m_writeFd.get(), CMD.c_str(), CMD.length()) != static_cast<ssize_t>(CMD.length()))
        return m_output;

    return readUntil("D ", 5000);
}

TEST_CASE(presentationFeedbackCommitOwnership) {
    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the presentation-feedback client"); }

    const auto OUTPUT = client->runBurst();

    EXPECT(OUTPUT.contains("A discarded"), true);
    EXPECT(OUTPUT.contains("B discarded"), true);
    EXPECT(OUTPUT.contains("C presented"), true);
    EXPECT(OUTPUT.contains("summary discarded=2 presented=1"), true);
}

TEST_CASE(presentationFeedbackSurfaceDestroy) {
    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (...) { FAIL_TEST("Couldn't start the presentation-feedback client"); }

    const auto OUTPUT = client->runDestroy();

    EXPECT(OUTPUT.contains("D discarded"), true);
    EXPECT(OUTPUT.contains("D presented"), false);
}
