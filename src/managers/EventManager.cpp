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

#include <string>

CEventManager::CEventManager() {
}

void CEventManager::startThread() {
    std::thread([&]() {
        const auto SOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

        if (SOCKET < 0) {
            Debug::log(ERR, "Couldn't start the Hyprland Socket 2. (1) IPC will not work.");
            return;
        }

        sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};
        std::string socketPath = "/tmp/hypr/" + g_pCompositor->m_szInstanceSignature + "/.socket2.sock";
        strcpy(SERVERADDRESS.sun_path, socketPath.c_str());

        bind(SOCKET, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS));

        // 10 max queued.
        listen(SOCKET, 10);

        char readBuf[1024] = {0};

        sockaddr_in clientAddress;
        socklen_t clientSize = sizeof(clientAddress);

        Debug::log(LOG, "Hypr socket 2 started at %s", socketPath.c_str());

        // set the socket nonblock
        int flags = fcntl(SOCKET, F_GETFL, 0);
        fcntl(SOCKET, F_SETFL, flags | O_NONBLOCK);

        while (1) {

            const auto ACCEPTEDCONNECTION = accept(SOCKET, (sockaddr*)&clientAddress, &clientSize);

            if (ACCEPTEDCONNECTION > 0) {
                // new connection!
                m_dAcceptedSocketFDs.push_back(ACCEPTEDCONNECTION);

                int flagsNew = fcntl(ACCEPTEDCONNECTION, F_GETFL, 0);
                fcntl(ACCEPTEDCONNECTION, F_SETFL, flagsNew | O_NONBLOCK);

                Debug::log(LOG, "Socket 2 accepted a new client at FD %d", ACCEPTEDCONNECTION);
            }

            // pong if all FDs valid
            for (auto it = m_dAcceptedSocketFDs.begin(); it != m_dAcceptedSocketFDs.end();) {
                auto sizeRead = recv(*it, &readBuf, 1024, 0);

                if (sizeRead != 0) {
                    it++;
                    continue;
                }

                // invalid!
                Debug::log(LOG, "Removed invalid socket (2) FD: %d", *it);
                it = m_dAcceptedSocketFDs.erase(it);
            }

            // valid FDs, check the queue
            // don't do anything if main thread is writing to the eventqueue
            eventQueueMutex.lock();

            if (m_dQueuedEvents.empty()){ // if queue empty, sleep and ignore
                eventQueueMutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // write all queued events
            for (auto& ev : m_dQueuedEvents) {
                std::string eventString = (ev.event + ">>" + ev.data).substr(0, 1022) + "\n";
                for (auto& fd : m_dAcceptedSocketFDs) {
                    write(fd, eventString.c_str(), eventString.length());
                }
            }

            m_dQueuedEvents.clear();

            eventQueueMutex.unlock();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        close(SOCKET);
    }).detach();
}

void CEventManager::postEvent(const SHyprIPCEvent event, bool force) {

    if (m_bIgnoreEvents && !force) {
        Debug::log(WARN, "Suppressed (ignoreevents true) event of type %s, content: %s",event.event.c_str(), event.data.c_str());
        return;
    }

    std::thread([&](const SHyprIPCEvent ev) {
        eventQueueMutex.lock();
        m_dQueuedEvents.push_back(ev);
        eventQueueMutex.unlock();
    }, event).detach();
}
