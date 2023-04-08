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

    auto removeFD = [&](int fd) -> void {
        const auto ACCEPTEDFDS = (std::deque<std::pair<int, wl_event_source*>>*)data;
        for (auto it = ACCEPTEDFDS->begin(); it != ACCEPTEDFDS->end();) {
            if (it->first == fd) {
                wl_event_source_remove(it->second); // remove this fd listener
                it = ACCEPTEDFDS->erase(it);
            } else {
                it++;
            }
        }
    };

    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        // remove, hanged up
        removeFD(fd);
        return 0;
    }

    int availableBytes;
    if (ioctl(fd, FIONREAD, &availableBytes) == -1) {
        Debug::log(ERR, "fd %d sent invalid data (1)", fd);
        removeFD(fd);
        return 0;
    }

    char       buf[availableBytes];
    const auto RECEIVED = recv(fd, buf, availableBytes, 0);
    if (RECEIVED == -1) {
        Debug::log(ERR, "fd %d sent invalid data (2)", fd);
        removeFD(fd);
        return 0;
    }

    return 0;
}

void CEventManager::startThread() {
    m_tThread = std::thread([&]() {
        const auto SOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the Hyprland Socket 2. (1) IPC will not work.");
            return;
        }

        sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};
        std::string socketPath    = "/tmp/hypr/" + g_pCompositor->m_szInstanceSignature + "/.socket2.sock";
        strcpy(SERVERADDRESS.sun_path, socketPath.c_str());

        bind(SOCKET, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS));

        // 10 max queued.
        listen(SOCKET, 10);

        sockaddr_in clientAddress;
        socklen_t   clientSize = sizeof(clientAddress);

        Debug::log(LOG, "Hypr socket 2 started at %s", socketPath.c_str());

        while (1) {
            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);

            if (ACCEPTEDCONNECTION > 0) {
                // new connection!

                int flagsNew = fcntl(ACCEPTEDCONNECTION, F_GETFL, 0);
                fcntl(ACCEPTEDCONNECTION, F_SETFL, flagsNew | O_NONBLOCK);

                Debug::log(LOG, "Socket 2 accepted a new client at FD %d", ACCEPTEDCONNECTION);

                // add to event loop so we can close it when we need to
                m_dAcceptedSocketFDs.push_back(
                    {ACCEPTEDCONNECTION, wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, ACCEPTEDCONNECTION, WL_EVENT_READABLE, fdHandleWrite, &m_dAcceptedSocketFDs)});
            }
        }

        close(SOCKET);
    });

    m_tThread.detach();
}

void CEventManager::flushEvents() {
    eventQueueMutex.lock();

    for (auto& ev : m_dQueuedEvents) {
        std::string eventString = (ev.event + ">>" + ev.data).substr(0, 1022) + "\n";
        for (auto& fd : m_dAcceptedSocketFDs) {
            write(fd.first, eventString.c_str(), eventString.length());
        }
    }

    m_dQueuedEvents.clear();

    eventQueueMutex.unlock();
}

void CEventManager::postEvent(const SHyprIPCEvent event, bool force) {

    if ((m_bIgnoreEvents && !force) || g_pCompositor->m_bIsShuttingDown) {
        Debug::log(WARN, "Suppressed (ignoreevents true / shutting down) event of type %s, content: %s", event.event.c_str(), event.data.c_str());
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
