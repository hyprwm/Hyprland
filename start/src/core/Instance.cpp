#include "Instance.hpp"
#include "State.hpp"
#include "../helpers/Logger.hpp"

#include <cstdlib>
#include <cstring>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <ranges>
#include <string_view>

#include <hyprutils/os/Process.hpp>

using namespace Hyprutils::OS;
using namespace std::string_literals;

//
void CHyprlandInstance::runHyprlandThread(bool safeMode) {
    std::vector<std::string> argsStd;
    argsStd.emplace_back("--watchdog-fd");
    argsStd.emplace_back(std::format("{}", m_toHlPid.get()));
    if (safeMode)
        argsStd.emplace_back("--safe-mode");

    for (const auto& a : g_state->rawArgvNoBinPath) {
        argsStd.emplace_back(a);
    }

    // spawn a process manually. Hyprutils' Async is detached, while Sync redirects stdout
    // TODO: make Sync respect fds?

    std::vector<char*> args = {strdup(g_state->customPath.value_or("Hyprland").c_str())};
    for (const auto& a : argsStd) {
        args.emplace_back(strdup(a.c_str()));
    }
    args.emplace_back(nullptr);

    int forkRet = fork();
    if (forkRet == 0) {
        // Make hyprland die on our SIGKILL
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        execvp(g_state->customPath.value_or("Hyprland").c_str(), args.data());

        g_logger->log(Hyprutils::CLI::LOG_ERR, "fork(): execvp failed: {}", strerror(errno));
        std::fflush(stdout);
        exit(1);
    } else
        m_hlPid = forkRet;

    m_hlThread = std::thread([this] {
        while (true) {
            int status = 0;
            int ret    = waitpid(m_hlPid, &status, 0);
            if (ret == -1) {
                g_logger->log(Hyprutils::CLI::LOG_ERR, "Couldn't waitpid for hyprland: {}", strerror(errno));
                break;
            }

            if (WIFEXITED(status))
                break;
        }

        write(m_wakeupWrite.get(), "vax", 3);

        std::fflush(stdout);
        std::fflush(stderr);
    });
}

void CHyprlandInstance::forceQuit() {
    m_hyprlandExiting = true;
    kill(m_hlPid, SIGTERM); // gracefully, can get stuck but it's unlikely
}

void CHyprlandInstance::clearFd(const Hyprutils::OS::CFileDescriptor& fd) {
    static std::array<char, 1024> buf;
    read(fd.get(), buf.data(), 1023);
}

void CHyprlandInstance::dispatchHyprlandEvent() {
    std::string                   recvd = "";
    static std::array<char, 4096> buf;
    ssize_t                       n = read(m_fromHlPid.get(), buf.data(), 4096);
    if (n < 0) {
        g_logger->log(Hyprutils::CLI::LOG_ERR, "Failed dispatching hl events");
        return;
    }

    recvd.append(buf.data(), n);

    if (recvd.empty())
        return;

    for (const auto& s : std::views::split(recvd, '\n')) {
        const std::string_view sv = std::string_view{s};
        if (sv == "vax") {
            // init passed
            m_hyprlandInitialized = true;
            continue;
        }

        if (sv == "end") {
            // exiting
            m_hyprlandExiting = true;
            continue;
        }
    }
}

bool CHyprlandInstance::run(bool safeMode) {
    int pipefds[2];
    pipe(pipefds);

    m_fromHlPid = CFileDescriptor{pipefds[0]};
    m_toHlPid   = CFileDescriptor{pipefds[1]};

    pipe(pipefds);

    m_wakeupRead  = CFileDescriptor{pipefds[0]};
    m_wakeupWrite = CFileDescriptor{pipefds[1]};

    runHyprlandThread(safeMode);

    pollfd pollfds[2] = {
        {
            .fd      = m_wakeupRead.get(),
            .events  = POLLIN,
            .revents = 0,
        },
        {
            .fd      = m_fromHlPid.get(),
            .events  = POLLIN,
            .revents = 0,
        },
    };

    while (true) {
        int ret = poll(pollfds, 2, -1);

        if (ret < 0) {
            g_logger->log(Hyprutils::CLI::LOG_ERR, "poll() failed, exiting");
            exit(1);
        }

        if (pollfds[1].revents & POLLIN) {
            g_logger->log(Hyprutils::CLI::LOG_DEBUG, "got an event from hyprland");
            dispatchHyprlandEvent();
            continue;
        }

        if (pollfds[0].revents & POLLIN) {
            g_logger->log(Hyprutils::CLI::LOG_DEBUG, "hyprland exit, breaking poll, checking state");
            clearFd(m_wakeupRead);
            break;
        }
    }

    m_hlThread.join();

    return !m_hyprlandInitialized || m_hyprlandExiting;
}