#include "Unix.hpp"

#include "../../Compositor.hpp"

#include <algorithm>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
using namespace Hyprutils::OS;

using namespace IPC;
using namespace IPC::Socket2;

static int onClientEvent(int fd, uint32_t mask, void* data) {
    return rc<CUnixImpl*>(data)->onClientEvent(fd, mask);
}

static int onServerEvent(int fd, uint32_t mask, void* data) {
    return rc<CUnixImpl*>(data)->onServerEvent(fd, mask);
}

CUnixPeer::CUnixPeer(Hyprutils::OS::CFileDescriptor&& fd, void* parent) :
    m_fd(std::move(fd)), m_eventSource(wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, m_fd.get(), 0, onClientEvent, parent)) {
    ;
}

CUnixPeer::~CUnixPeer() {
    if (m_eventSource)
        wl_event_source_remove(m_eventSource);
}

size_t CUnixPeer::id() const {
    return m_fd.isValid() ? sc<size_t>(m_fd.get()) : sc<size_t>(0);
}

bool CUnixPeer::flush() {
    while (!m_events.empty()) {
        const auto& event     = m_events.front();
        const auto  REMAINING = event->length() - m_writeOffset;
        const auto  WRITTEN   = write(m_fd.get(), event->c_str() + m_writeOffset, REMAINING);

        if (WRITTEN > 0) {
            m_writeOffset += sc<size_t>(WRITTEN);
            if (m_writeOffset < event->length())
                return true;

            m_writeOffset = 0;
            m_events.erase(m_events.begin());
            continue;
        }

        if (WRITTEN < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;

        if (WRITTEN < 0 && errno == EINTR)
            continue;

        Log::logger->log(Log::ERR, "[Socket2::UnixPeer] fd {} failed writing event: {}", m_fd.get(), WRITTEN < 0 ? strerror(errno) : "write returned 0");
        return false;
    }

    wl_event_source_fd_update(m_eventSource, 0);
    return true;
}

size_t CUnixPeer::queueSize() const {
    return m_events.size();
}

bool CUnixPeer::addEvent(const SP<std::string>& x) {
    m_events.emplace_back(x);

    if (!flush())
        return false;

    if (!m_events.empty() && queueSize() <= 1)
        wl_event_source_fd_update(m_eventSource, WL_EVENT_WRITABLE);

    return true;
}

CUnixImpl::CUnixImpl() : m_socket(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) {
    if (!m_socket.isValid()) {
        Log::logger->log(Log::ERR, "[Socket2::UnixImpl] Couldn't start the Hyprland Socket 2. (1) IPC will not work.");
        return;
    }

    sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};
    const auto  PATH          = std::format("{}/.socket2.sock", g_pCompositor->m_instancePath);
    if (PATH.length() > sizeof(SERVERADDRESS.sun_path) - 1) {
        Log::logger->log(Log::ERR, "[Socket2::UnixImpl] path is too long. (2) IPC will not work.");
        return;
    }

    strncpy(SERVERADDRESS.sun_path, PATH.c_str(), sizeof(SERVERADDRESS.sun_path) - 1);

    if (bind(m_socket.get(), rc<sockaddr*>(&SERVERADDRESS), SUN_LEN(&SERVERADDRESS)) < 0) {
        Log::logger->log(Log::ERR, "[Socket2::UnixImpl] Couldn't bind the Hyprland Socket 2. (3) IPC will not work.");
        return;
    }

    // 10 max queued.
    if (listen(m_socket.get(), 10) < 0) {
        Log::logger->log(Log::ERR, "[Socket2::UnixImpl] Couldn't listen on the Hyprland Socket 2. (4) IPC will not work.");
        return;
    }

    m_eventSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, m_socket.get(), WL_EVENT_READABLE, ::onServerEvent, this);
}

CUnixImpl::~CUnixImpl() {
    if (m_eventSource)
        wl_event_source_remove(m_eventSource);
}

int CUnixImpl::onServerEvent(int fd, uint32_t mask) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        Log::logger->log(Log::ERR, "[Socket2::UnixImpl] hangup?? IPC broke");

        wl_event_source_remove(m_eventSource);
        m_eventSource = nullptr;
        m_socket.reset();

        return 0;
    }

    sockaddr_in     clientAddress;
    socklen_t       clientSize = sizeof(clientAddress);
    CFileDescriptor acceptedConnection{accept4(m_socket.get(), rc<sockaddr*>(&clientAddress), &clientSize, SOCK_CLOEXEC | SOCK_NONBLOCK)};
    if (!acceptedConnection.isValid()) {
        if (errno != EAGAIN) {
            Log::logger->log(Log::ERR, "[Socket2::UnixImpl] failed receiving connection, errno: {}", errno);
            wl_event_source_remove(m_eventSource);
            m_eventSource = nullptr;
            m_socket.reset();
        }

        return 0;
    }

    Log::logger->log(Log::DEBUG, "[Socket2::UnixImpl] accepted a new client at FD {}", acceptedConnection.get());

    m_peers.emplace_back(makeShared<CUnixPeer>(std::move(acceptedConnection), this));

    return 0;
}

int CUnixImpl::onClientEvent(int fd, uint32_t mask) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        Log::logger->log(Log::DEBUG, "[Socket2::UnixImpl] fd {} hung up", fd);
        removeByFd(fd);
        return 0;
    }

    if (mask & WL_EVENT_WRITABLE) {
        const auto PCLIENT = findByFd(fd);

        if (PCLIENT && !PCLIENT->flush())
            std::erase(m_peers, PCLIENT);
    }

    return 0;
}

void CUnixImpl::removeByFd(int fd) {
    if (fd < 0)
        return;
    std::erase_if(m_peers, [&fd](const auto& e) {
        return e->id() == sc<size_t>(fd); /* This is fine because unix peer IDs are just fds */
    });
}

SP<CUnixPeer> CUnixImpl::findByFd(int fd) const {
    auto it = std::ranges::find_if(m_peers, [&fd](const auto& e) {
        return e->id() == sc<size_t>(fd); /* This is fine because unix peer IDs are just fds */
    });
    if (it == m_peers.end())
        return nullptr;
    return *it;
}

bool CUnixImpl::send(std::string&& x) {
    constexpr const size_t MAX_QUEUED_EVENTS = 64;

    auto                   p = makeShared<std::string>(std::move(x));

    for (auto it = m_peers.begin(); it != m_peers.end();) {
        const auto& PEER = *it;

        if (PEER->queueSize() >= MAX_QUEUED_EVENTS) {
            Log::logger->log(Log::ERR, "[Socket2::UnixImpl] fd {} overflowed event queue, removing", (*it)->id());
            it = m_peers.erase(it);
            continue;
        }

        if (!PEER->addEvent(p)) {
            it = m_peers.erase(it);
            continue;
        }

        ++it;
    }

    return true;
}