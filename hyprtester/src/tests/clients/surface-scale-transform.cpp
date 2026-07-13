#include "../../hyprctlCompat.hpp"
#include "../../Log.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"
#include "build.hpp"
#include "tests.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <format>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/poll.h>
#include <unistd.h>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;
using namespace Hyprutils::Utils;

#define SP CSharedPointer

namespace {
    class CClient {
      public:
        CClient();
        ~CClient();

        std::string command(const std::string& command);

      private:
        SP<CProcess>           m_proc;
        std::array<char, 2048> m_readBuf;
        CFileDescriptor        m_readFd, m_writeFd;
        struct pollfd          m_fds = {};
    };
}

static bool waitForClientWindow(pid_t pid, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    const auto needle   = std::format("pid: {}", pid);

    while (Tests::processAlive(pid)) {
        if (getFromSocket("/clients").contains(needle))
            return true;

        if (std::chrono::steady_clock::now() >= deadline)
            return false;
    }

    return false;
}

CClient::CClient() {
    m_proc = makeShared<CProcess>(binaryDir + "/surface-scale-transform", std::vector<std::string>{});
    m_proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int pipeFds1[2], pipeFds2[2];
    if (pipe(pipeFds1) != 0 || pipe(pipeFds2) != 0)
        throw std::runtime_error(std::format("pipe failed: {}", strerror(errno)));

    m_writeFd = CFileDescriptor(pipeFds1[1]);
    m_proc->setStdinFD(pipeFds1[0]);

    m_readFd = CFileDescriptor(pipeFds2[0]);
    m_proc->setStdoutFD(pipeFds2[1]);

    const int COUNT_BEFORE = Tests::windowCount();
    m_proc->runAsync();

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    m_fds         = {.fd = m_readFd.get(), .events = POLLIN};
    m_fds.revents = 0;
    int pollRet   = 0;
    do {
        pollRet = poll(&m_fds, 1, 30000);
    } while (pollRet == -1 && errno == EINTR);

    if (pollRet != 1 || !(m_fds.revents & POLLIN))
        throw std::runtime_error(std::format("startup stdout poll failed: ret={} revents={} alive={} pid={} binary={}", pollRet, m_fds.revents, Tests::processAlive(m_proc->pid()),
                                             m_proc->pid(), binaryDir + "/surface-scale-transform"));

    m_readBuf.fill(0);
    const ssize_t bytesRead = read(m_readFd.get(), m_readBuf.data(), m_readBuf.size() - 1);
    if (bytesRead <= 0)
        throw std::runtime_error(std::format("startup stdout read failed: bytes={} errno={} ({}) revents={} alive={} pid={}", bytesRead, errno, strerror(errno), m_fds.revents,
                                             Tests::processAlive(m_proc->pid()), m_proc->pid()));

    const std::string ret = std::string{m_readBuf.data()};
    if (!ret.contains("started")) {
        NLog::log("{}Failed to start surface-scale-transform client, read {}", Colors::RED, ret);
        throw std::runtime_error(std::format("client reported '{}'", ret));
    }

    if (!waitForClientWindow(m_proc->pid(), 10000)) {
        NLog::log("{}Timed out waiting for surface-scale-transform window. count before: {}, count after: {}", Colors::RED, COUNT_BEFORE, Tests::windowCount());
        throw std::runtime_error(std::format("window did not appear for pid {}, count before {}, count after {}", m_proc->pid(), COUNT_BEFORE, Tests::windowCount()));
    }
}

CClient::~CClient() {
    const std::string cmd = "exit\n";
    write(m_writeFd.get(), cmd.c_str(), cmd.length());

    if (m_proc)
        kill(m_proc->pid(), SIGKILL);
}

std::string CClient::command(const std::string& command) {
    const std::string cmd = command + "\n";
    if ((size_t)write(m_writeFd.get(), cmd.c_str(), cmd.length()) != cmd.length())
        return "";

    m_fds.revents = 0;
    if (poll(&m_fds, 1, 10000) != 1 || !(m_fds.revents & POLLIN))
        return "";

    const ssize_t bytesRead = read(m_fds.fd, m_readBuf.data(), m_readBuf.size() - 1);
    if (bytesRead <= 0)
        return "";

    m_readBuf[bytesRead] = 0;
    std::string ret      = std::string{m_readBuf.data()};
    if (!ret.empty() && ret.back() == '\n')
        ret.pop_back();
    return ret;
}

static std::string waitCommandContains(CClient& client, const std::string& command, const std::string& needle) {
    std::string ret;
    const auto  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

    while (std::chrono::steady_clock::now() < deadline) {
        ret = client.command(command);
        if (ret.contains(needle))
            return ret;

        getFromSocket("/version");
    }

    return ret;
}

static std::string waitCommandDiffers(CClient& client, const std::string& command, const std::string& previous) {
    std::string ret;
    const auto  deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

    while (std::chrono::steady_clock::now() < deadline) {
        ret = client.command(command);
        if (ret != previous)
            return ret;

        getFromSocket("/version");
    }

    return ret;
}

TEST_CASE(surfaceScaleTransform) {
    CScopeGuard guard = {[&]() {
        OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1', transform = 0 })"));
        Tests::killAllWindows();
    }};

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1.5', transform = 0 })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ monitor = 'HEADLESS-2' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '260' })"));

    std::optional<CClient> client;
    try {
        client.emplace();
    } catch (const std::exception& e) { FAIL_TEST("Couldn't start the surface scale/transform client: {}", e.what()); }

    ASSERT_CONTAINS(client->command("report"), "root_scale=2");
    ASSERT_CONTAINS(client->command("report"), "root_fraction=180");
    ASSERT_CONTAINS(client->command("report"), "root_transform=0");

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '0x0', scale = '1.5', transform = 1 })"));
    ASSERT_CONTAINS(waitCommandContains(*client, "report", "root_transform=1"), "root_transform=1");

    const auto beforeUnmap = client->command("report");
    ASSERT_CONTAINS(beforeUnmap, "root_scale_count=");

    ASSERT(client->command("unmap"), "ok");
    ASSERT(client->command("remap"), "ok");
    const auto afterRemap = waitCommandDiffers(*client, "report", beforeUnmap);
    EXPECT_CONTAINS(afterRemap, "root_scale=2");
    EXPECT_CONTAINS(afterRemap, "root_fraction=180");
    EXPECT_NOT(afterRemap, beforeUnmap);

    ASSERT(client->command("subsurface"), "ok");
    const auto withChild = client->command("report");
    EXPECT_CONTAINS(withChild, "child_scale=2");
    EXPECT_CONTAINS(withChild, "child_fraction=180");
    EXPECT_CONTAINS(withChild, "child_transform=1");
}
