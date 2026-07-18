#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"
#include "build.hpp"
#include "tests.hpp"

#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprutils/os/Process.hpp>

#include <array>
#include <chrono>
#include <csignal>
#include <optional>
#include <string>
#include <sys/poll.h>
#include <thread>
#include <unordered_map>

using namespace Hyprutils::OS;
using namespace Hyprutils::Memory;

#define SP CSharedPointer

namespace {
    struct SClientStats {
        uint64_t commits           = 0;
        uint64_t frameRequested    = 0;
        uint64_t frameDone         = 0;
        uint64_t feedbackRequested = 0;
        uint64_t feedbackDone      = 0;
        uint64_t presented         = 0;
        uint64_t discarded         = 0;
        uint64_t configures        = 0;
    };

    static std::optional<SClientStats> parseStats(const std::string& line) {
        if (!line.starts_with("stats "))
            return std::nullopt;

        std::unordered_map<std::string, uint64_t> values;
        size_t                                    pos = 6;

        while (pos < line.size()) {
            const auto eq = line.find('=', pos);
            if (eq == std::string::npos)
                break;

            const auto end = line.find(' ', eq);
            try {
                values[line.substr(pos, eq - pos)] = std::stoull(line.substr(eq + 1, end - eq - 1));
            } catch (...) { return std::nullopt; }

            if (end == std::string::npos)
                break;
            pos = end + 1;
        }

        static constexpr std::array REQUIRED_KEYS = {
            "commits", "frame_requested", "frame_done", "feedback_requested", "feedback_done", "presented", "discarded", "configures",
        };

        for (const auto& key : REQUIRED_KEYS) {
            if (!values.contains(key))
                return std::nullopt;
        }

        return SClientStats{
            .commits           = values["commits"],
            .frameRequested    = values["frame_requested"],
            .frameDone         = values["frame_done"],
            .feedbackRequested = values["feedback_requested"],
            .feedbackDone      = values["feedback_done"],
            .presented         = values["presented"],
            .discarded         = values["discarded"],
            .configures        = values["configures"],
        };
    }

    class CStressClient {
      public:
        CStressClient(const std::string& appId);
        ~CStressClient();

        pid_t        pid() const;
        bool         burst(int frames, bool invalidDamage);
        bool         send(const std::string& command);
        SClientStats stats();

      private:
        std::optional<std::string> readLine(int timeoutMs);
        bool                       waitForLine(const std::string& prefix, int timeoutMs);

        SP<CProcess>               proc;
        CFileDescriptor            readFd, writeFd;
        struct pollfd              fds     = {};
        std::array<char, 1024>     readBuf = {};
        std::string                pendingOutput;
    };

    static SClientStats waitForSettledFeedback(CStressClient& client, int timeoutMs) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        auto       stats    = client.stats();

        while (stats.feedbackDone < stats.feedbackRequested && std::chrono::steady_clock::now() < deadline) {
            Tests::sync(2);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            stats = client.stats();
        }

        return stats;
    }
}

CStressClient::CStressClient(const std::string& appId) {
    proc = makeShared<CProcess>(binaryDir + "/fullscreen-scanout-stress", std::vector<std::string>{appId});
    proc->addEnv("WAYLAND_DISPLAY", WLDISPLAY);

    int pipeFds1[2], pipeFds2[2];
    if (pipe(pipeFds1) != 0 || pipe(pipeFds2) != 0)
        throw std::exception();

    writeFd = CFileDescriptor(pipeFds1[1]);
    proc->setStdinFD(pipeFds1[0]);

    readFd = CFileDescriptor(pipeFds2[0]);
    proc->setStdoutFD(pipeFds2[1]);

    const int countBefore = Tests::windowCount();
    proc->runAsync();

    close(pipeFds1[0]);
    close(pipeFds2[1]);

    fds = {.fd = readFd.get(), .events = POLLIN};

    if (!waitForLine("started", 5000))
        throw std::exception();

    int counter = 0;
    while (Tests::processAlive(proc->pid()) && Tests::windowCount() == countBefore) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (++counter > 50)
            throw std::exception();
    }

    if (!Tests::processAlive(proc->pid()))
        throw std::exception();
}

CStressClient::~CStressClient() {
    send("exit");
    kill(proc->pid(), SIGKILL);
    proc.reset();
}

pid_t CStressClient::pid() const {
    return proc->pid();
}

bool CStressClient::send(const std::string& command) {
    const std::string line = command + "\n";
    return write(writeFd.get(), line.c_str(), line.size()) == sc<ssize_t>(line.size());
}

bool CStressClient::burst(int frames, bool invalidDamage) {
    if (!send(std::format("burst {} {}", frames, invalidDamage ? "invalid" : "valid")))
        return false;

    return waitForLine(std::format("burst_done {}", frames), 5000);
}

std::optional<std::string> CStressClient::readLine(int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        if (const auto newline = pendingOutput.find('\n'); newline != std::string::npos) {
            std::string line = pendingOutput.substr(0, newline);
            pendingOutput.erase(0, newline + 1);
            return line;
        }

        const int remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (poll(&fds, 1, std::max(remaining, 1)) != 1 || !(fds.revents & POLLIN))
            continue;

        const ssize_t bytesRead = read(fds.fd, readBuf.data(), readBuf.size() - 1);
        if (bytesRead <= 0)
            continue;

        readBuf[bytesRead] = 0;
        pendingOutput += readBuf.data();
    }

    return std::nullopt;
}

bool CStressClient::waitForLine(const std::string& prefix, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const int  remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        const auto line      = readLine(std::max(remaining, 1));
        if (line && line->starts_with(prefix))
            return true;
    }

    return false;
}

SClientStats CStressClient::stats() {
    send("stats");

    for (int i = 0; i < 10; ++i) {
        const auto line = readLine(1000);
        if (!line)
            continue;

        if (auto stats = parseStats(*line); stats)
            return *stats;
    }

    return {};
}

TEST_CASE(fullscreenScanoutStressTransitions) {
    OK(getFromSocket("/eval hl.config({ animations = { enabled = false } })"));
    // Hyprtester clients are SHM-backed under the headless backend, so this covers the
    // fullscreen/direct-scanout transition and fallback paths, not hardware scanout.
    OK(getFromSocket("/eval hl.config({ render = { direct_scanout = 1 } })"));
    OK(getFromSocket("/eval hl.config({ debug = { damage_tracking = 2 } })"));

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
    std::optional<CStressClient> background;
    try {
        background.emplace("stress-bg");
    } catch (...) { FAIL_TEST("Couldn't start background stress client"); }

    OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
    std::optional<CStressClient> game;
    try {
        game.emplace("stress-game");
    } catch (...) { FAIL_TEST("Couldn't start fullscreen stress client"); }

    OK(getFromSocket(std::format("/dispatch hl.dsp.focus({{ window = 'pid:{}' }})", game->pid())));
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    Tests::sync();

    {
        const auto active = getFromSocket("/activewindow");
        ASSERT_CONTAINS(active, "class: stress-game");
        ASSERT_CONTAINS(active, "fullscreen: 2");
    }

    OK(getFromSocket("/eval hl.plugin.test.reset_render_stats()"));
    ASSERT(background->burst(80, true), true);
    ASSERT(game->burst(64, true), true);
    Tests::sync(8);

    OK(getFromSocket("/eval hl.plugin.test.expect_window_render_count_max('stress-bg', 0)"));

    const auto coalesced = waitForSettledFeedback(*game, 3000);
    EXPECT(coalesced.feedbackRequested >= 64, true);
    EXPECT(coalesced.feedbackRequested - coalesced.feedbackDone <= 1, true);
    EXPECT(coalesced.discarded > 0, true);
    EXPECT(coalesced.presented < coalesced.feedbackRequested, true);

    for (int i = 0; i < 8; ++i) {
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '2' })"));
        ASSERT(background->burst(12, true), true);
        OK(getFromSocket("/dispatch hl.dsp.focus({ workspace = '1' })"));
        ASSERT(game->burst(12, true), true);
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
        OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'fullscreen' })"));
    }

    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', disabled = true })"));
    OK(getFromSocket("/eval hl.monitor({ output = 'HEADLESS-2', mode = '1920x1080@60', position = '1920x0', scale = '1' })"));

    ASSERT(game->send("destroy"), true);
    ASSERT(background->send("destroy"), true);
    Tests::waitUntilWindowsN(0);
    Tests::sync(10);

    OK(getFromSocket("/eval hl.plugin.test.expect_presentation_pending_max(0, 0)"));
}
