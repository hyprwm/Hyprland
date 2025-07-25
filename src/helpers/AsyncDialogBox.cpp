#include "AsyncDialogBox.hpp"
#include "./fs/FsUtils.hpp"
#include <csignal>
#include <algorithm>
#include <unistd.h>
#include "../managers/eventLoop/EventLoopManager.hpp"

using namespace Hyprutils::OS;

static std::vector<std::pair<pid_t, WP<CAsyncDialogBox>>> asyncDialogBoxes;

//
SP<CAsyncDialogBox> CAsyncDialogBox::create(const std::string& title, const std::string& description, std::vector<std::string> buttons) {
    if (!NFsUtils::executableExistsInPath("hyprland-dialog")) {
        Debug::log(ERR, "CAsyncDialogBox: cannot create, no hyprland-dialog");
        return nullptr;
    }

    auto dialog = SP<CAsyncDialogBox>(new CAsyncDialogBox(title, description, buttons));

    dialog->m_selfWeakReference = dialog;

    return dialog;
}

bool CAsyncDialogBox::isAsyncDialogBox(pid_t pid) {
    return std::ranges::find_if(asyncDialogBoxes, [pid](const auto& e) { return e.first == pid; }) != asyncDialogBoxes.end();
}

bool CAsyncDialogBox::isPriorityDialogBox(pid_t pid) {
    for (const auto& [p, db] : asyncDialogBoxes) {
        if (p != pid)
            continue;

        return db && db->m_priority;
    }

    return false;
}

CAsyncDialogBox::CAsyncDialogBox(const std::string& title, const std::string& description, std::vector<std::string> buttons) :
    m_title(title), m_description(description), m_buttons(buttons) {
    ;
}

static int onFdWrite(int fd, uint32_t mask, void* data) {
    auto box = (CAsyncDialogBox*)data;

    // lock the box to prevent a UAF
    auto lock = box->lockSelf();

    box->onWrite(fd, mask);

    return 0;
}

void CAsyncDialogBox::onWrite(int fd, uint32_t mask) {
    if (mask & WL_EVENT_READABLE) {
        std::array<char, 1024> buf;
        int                    ret = 0;

        // make the FD nonblock for a moment
        // TODO: can we avoid this without risking a blocking read()?
        int fdFlags = fcntl(fd, F_GETFL, 0);
        if (fcntl(fd, F_SETFL, fdFlags | O_NONBLOCK) < 0) {
            Debug::log(ERR, "CAsyncDialogBox::onWrite: fcntl 1 failed!");
            return;
        }

        while ((ret = read(m_pipeReadFd.get(), buf.data(), 1023)) > 0) {
            m_stdout += std::string_view{(char*)buf.data(), (size_t)ret};
        }

        // restore the flags (otherwise libwayland won't give us a hangup)
        if (fcntl(fd, F_SETFL, fdFlags) < 0) {
            Debug::log(ERR, "CAsyncDialogBox::onWrite: fcntl 2 failed!");
            return;
        }
    }

    if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
        Debug::log(LOG, "CAsyncDialogBox: dialog {:x} hung up, closed.");

        m_promiseResolver->resolve(m_stdout);
        std::erase_if(asyncDialogBoxes, [this](const auto& e) { return e.first == m_dialogPid; });

        wl_event_source_remove(m_readEventSource);
        m_selfReference.reset();
        return;
    }
}

SP<CPromise<std::string>> CAsyncDialogBox::open() {
    std::string buttonsString = "";
    for (auto& b : m_buttons) {
        buttonsString += b + ";";
    }
    if (!buttonsString.empty())
        buttonsString.pop_back();

    CProcess proc("hyprland-dialog", std::vector<std::string>{"--title", m_title, "--text", m_description, "--buttons", buttonsString});

    int      outPipe[2];
    if (pipe(outPipe)) {
        Debug::log(ERR, "CAsyncDialogBox::open: failed to pipe()");
        return nullptr;
    }

    m_pipeReadFd = CFileDescriptor(outPipe[0]);

    proc.setStdoutFD(outPipe[1]);

    m_readEventSource = wl_event_loop_add_fd(g_pEventLoopManager->m_wayland.loop, m_pipeReadFd.get(), WL_EVENT_READABLE, ::onFdWrite, this);

    if (!m_readEventSource) {
        Debug::log(ERR, "CAsyncDialogBox::open: failed to add read fd to loop");
        return nullptr;
    }

    m_selfReference = m_selfWeakReference.lock();

    if (!proc.runAsync()) {
        Debug::log(ERR, "CAsyncDialogBox::open: failed to run async");
        wl_event_source_remove(m_readEventSource);
        return nullptr;
    }

    m_dialogPid = proc.pid();
    asyncDialogBoxes.emplace_back(std::make_pair<>(m_dialogPid, m_selfWeakReference));

    // close the write fd, only the dialog owns it now
    close(outPipe[1]);

    auto promise = CPromise<std::string>::make([this](SP<CPromiseResolver<std::string>> r) { m_promiseResolver = r; });

    return promise;
}

void CAsyncDialogBox::kill() {
    if (m_dialogPid <= 0)
        return;

    ::kill(m_dialogPid, SIGKILL);
}

bool CAsyncDialogBox::isRunning() const {
    return m_readEventSource;
}

SP<CAsyncDialogBox> CAsyncDialogBox::lockSelf() {
    return m_selfWeakReference.lock();
}
