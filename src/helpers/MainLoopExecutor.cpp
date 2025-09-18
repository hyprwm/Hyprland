#include "MainLoopExecutor.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../macros.hpp"

static int onDataRead(int fd, uint32_t mask, void* data) {
    ((CMainLoopExecutor*)data)->onFired();
    return 0;
}

CMainLoopExecutor::CMainLoopExecutor(std::function<void()>&& callback) : m_fn(std::move(callback)) {

    int fds[2];
    pipe(fds);

    RASSERT(fds[0] != 0, "CMainLoopExecutor: failed to open a pipe");
    RASSERT(fds[1] != 0, "CMainLoopExecutor: failed to open a pipe");

    m_event = wl_event_loop_add_fd(g_pEventLoopManager->m_wayland.loop, fds[0], WL_EVENT_READABLE, ::onDataRead, this);

    m_readFd  = Hyprutils::OS::CFileDescriptor(fds[0]);
    m_writeFd = Hyprutils::OS::CFileDescriptor(fds[1]);
}

CMainLoopExecutor::~CMainLoopExecutor() {
    if (m_event) // FIXME: potential race in case of a weird destroy on a worker thread
        wl_event_source_remove(m_event);
}

void CMainLoopExecutor::signal() {
    const char* amogus = "h";
    write(m_writeFd.get(), amogus, 1);
}

void CMainLoopExecutor::onFired() {
    if (!m_fn)
        return;

    m_fn();
    m_fn = nullptr;

    // we need to remove the event here because we're on the main thread
    wl_event_source_remove(m_event);
    m_event = nullptr;

    m_readFd.reset();
    m_writeFd.reset();
}
