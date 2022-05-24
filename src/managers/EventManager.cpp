#include "EventManager.hpp"

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

        unlink("/tmp/hypr/.socket2.sock");

        sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};
        strcpy(SERVERADDRESS.sun_path, "/tmp/hypr/.socket2.sock");

        bind(SOCKET, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS));

        // 10 max queued.
        listen(SOCKET, 10);

        char readBuf[1024] = {0};

        sockaddr_in clientAddress;
        socklen_t clientSize = sizeof(clientAddress);

        Debug::log(LOG, "Hypr socket 2 started.");

        // set the socket nonblock
        int flags = fcntl(SOCKET, F_GETFL, 0);
        fcntl(SOCKET, F_SETFL, flags | O_NONBLOCK);

        while (1) {
            m_bCanWriteEventQueue = true;

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
            while (!m_bCanReadEventQueue) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            // if we got here, we'll be reading the queue, let's disallow writing
            m_bCanWriteEventQueue = false;

            if (m_dQueuedEvents.empty()){ // if queue empty, sleep and ignore
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // write all queued events
            for (auto& ev : m_dQueuedEvents) {
                std::string eventString = ev.event + ">>" + ev.data + "\n";
                for (auto& fd : m_dAcceptedSocketFDs) {
                    write(fd, eventString.c_str(), eventString.length());
                }
            }

            m_dQueuedEvents.clear();

            m_bCanWriteEventQueue = true;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        close(SOCKET);
    }).detach();
}

void CEventManager::postEvent(const SHyprIPCEvent event) {

    m_bCanReadEventQueue = false;
    if (!m_bCanWriteEventQueue) {
        // if we can't write rn, make a thread to write whenever possible, don't block calling events.
        std::thread([&](const SHyprIPCEvent ev) {
            while(!m_bCanWriteEventQueue) {
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }

            m_dQueuedEvents.push_back(ev);
            m_bCanReadEventQueue = true;
        }, event).detach();
    } else {
        m_dQueuedEvents.push_back(event);
        m_bCanReadEventQueue = true;
    }
}