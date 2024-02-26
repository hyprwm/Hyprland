#include "EventManager.hpp"
#include "../Compositor.hpp"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <string>

CEventManager::CEventManager() {}

int fdHandleWrite(int fd, uint32_t mask, void* data) {
    const auto PEVMGR = (CEventManager*)data;
    return PEVMGR->onFDWrite(fd, mask);
}

int socket2HandleWrite(int fd, uint32_t mask, void* data) {
    const auto PEVMGR = (CEventManager*)data;
    return PEVMGR->onSocket2Write(fd, mask);
}

void CEventManager::startThread() {

    m_iSocketFD = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (m_iSocketFD < 0) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket 2. (1) IPC will not work.");
        return;
    }

    sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};
    std::string socketPath    = "/tmp/hypr/" + g_pCompositor->m_szInstanceSignature + "/.socket2.sock";
    strncpy(SERVERADDRESS.sun_path, socketPath.c_str(), sizeof(SERVERADDRESS.sun_path) - 1);

    bind(m_iSocketFD, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS));

    // 10 max queued.
    listen(m_iSocketFD, 10);

    m_pEventSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, m_iSocketFD, WL_EVENT_READABLE, socket2HandleWrite, this);
}

int CEventManager::onSocket2Write(int fd, uint32_t mask) {

    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        Debug::log(ERR, "Socket2 hangup?? IPC broke");
        wl_event_source_remove(m_pEventSource);
        close(fd);
        return 0;
    }

    sockaddr_in clientAddress;
    socklen_t   clientSize         = sizeof(clientAddress);
    const auto  ACCEPTEDCONNECTION = accept4(m_iSocketFD, (sockaddr*)&clientAddress, &clientSize, SOCK_CLOEXEC | SOCK_NONBLOCK);

    if (ACCEPTEDCONNECTION > 0) {
        Debug::log(LOG, "Socket2 accepted a new client at FD {}", ACCEPTEDCONNECTION);

        // add to event loop so we can close it when we need to
        m_dAcceptedSocketFDs.push_back(
            std::make_pair<>(ACCEPTEDCONNECTION, wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, ACCEPTEDCONNECTION, WL_EVENT_READABLE, fdHandleWrite, this)));
    } else {
        Debug::log(ERR, "Socket2 failed receiving connection, errno: {}", errno);
        close(fd);
    }

    return 0;
}

int CEventManager::onFDWrite(int fd, uint32_t mask) {
    auto removeFD = [this](int fd) -> void {
        for (auto it = m_dAcceptedSocketFDs.begin(); it != m_dAcceptedSocketFDs.end();) {
            if (it->first == fd) {
                wl_event_source_remove(it->second); // remove this fd listener
                it = m_dAcceptedSocketFDs.erase(it);
            } else {
                it++;
            }
        }

        close(fd);
    };

    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        // remove, hanged up
        removeFD(fd);
        return 0;
    }

    int availableBytes;
    if (ioctl(fd, FIONREAD, &availableBytes) == -1) {
        Debug::log(ERR, "fd {} sent invalid data (1)", fd);
        removeFD(fd);
        return 0;
    }

    char       buf[availableBytes];
    const auto RECEIVED = recv(fd, buf, availableBytes, 0);
    if (RECEIVED == -1) {
        Debug::log(ERR, "fd {} sent invalid data (2)", fd);
        removeFD(fd);
        return 0;
    }

    return 0;
}

void CEventManager::flushEvents() {
    eventQueueMutex.lock();

    for (auto& ev : m_dQueuedEvents) {
        std::string eventString = (ev.event + ">>" + ev.data).substr(0, 1022) + "\n";
        for (auto& fd : m_dAcceptedSocketFDs) {
            try {
                write(fd.first, eventString.c_str(), eventString.length());
            } catch (...) {}
        }
    }

    m_dQueuedEvents.clear();

    eventQueueMutex.unlock();
}

void CEventManager::postEvent(const SHyprIPCEvent event) {

    if (g_pCompositor->m_bIsShuttingDown) {
        Debug::log(WARN, "Suppressed (ignoreevents true / shutting down) event of type {}, content: {}", event.event, event.data);
        return;
    }

    std::thread(
        [&](const SHyprIPCEvent ev) {
            eventQueueMutex.lock();
            m_dQueuedEvents.push_back(ev);
            eventQueueMutex.unlock();

            flushEvents();
        },
        event)
        .detach();
}
