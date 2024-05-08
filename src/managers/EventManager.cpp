#include "EventManager.hpp"
#include "../Compositor.hpp"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

CEventManager::CEventManager() {
    m_iSocketFD = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (m_iSocketFD < 0) {
        Debug::log(ERR, "Couldn't start the Hyprland Socket 2. (1) IPC will not work.");
        return;
    }

    sockaddr_un SERVERADDRESS = {.sun_family = AF_UNIX};
    const auto  PATH          = g_pCompositor->m_szInstancePath + "/.socket2.sock";
    if (PATH.length() > sizeof(SERVERADDRESS.sun_path) - 1) {
        Debug::log(ERR, "Socket2 path is too long. (2) IPC will not work.");
        return;
    }

    strncpy(SERVERADDRESS.sun_path, PATH.c_str(), sizeof(SERVERADDRESS.sun_path) - 1);

    if (bind(m_iSocketFD, (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS)) < 0) {
        Debug::log(ERR, "Couldn't bind the Hyprland Socket 2. (3) IPC will not work.");
        return;
    }

    // 10 max queued.
    if (listen(m_iSocketFD, 10) < 0) {
        Debug::log(ERR, "Couldn't listen on the Hyprland Socket 2. (4) IPC will not work.");
        return;
    }

    m_pEventSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, m_iSocketFD, WL_EVENT_READABLE, onClientEvent, this);
}

CEventManager::~CEventManager() {
    for (const auto& [fd, client] : m_mClients) {
        wl_event_source_remove(client.eventSource);
        close(fd);
    }

    if (m_pEventSource != nullptr)
        wl_event_source_remove(m_pEventSource);

    if (m_iSocketFD >= 0)
        close(m_iSocketFD);
}

int CEventManager::onServerEvent(int fd, uint32_t mask, void* data) {
    auto* const PEVMGR = (CEventManager*)data;
    return PEVMGR->onClientEvent(fd, mask);
}

int CEventManager::onClientEvent(int fd, uint32_t mask, void* data) {
    auto* const PEVMGR = (CEventManager*)data;
    return PEVMGR->onServerEvent(fd, mask);
}

int CEventManager::onServerEvent(int fd, uint32_t mask) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        Debug::log(ERR, "Socket2 hangup?? IPC broke");

        wl_event_source_remove(m_pEventSource);
        m_pEventSource = nullptr;
        close(fd);
        m_iSocketFD = -1;

        return 0;
    }

    sockaddr_in clientAddress;
    socklen_t   clientSize         = sizeof(clientAddress);
    const auto  ACCEPTEDCONNECTION = accept4(m_iSocketFD, (sockaddr*)&clientAddress, &clientSize, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if (ACCEPTEDCONNECTION < 0) {
        Debug::log(ERR, "Socket2 failed receiving connection, errno: {}", errno);
        wl_event_source_remove(m_pEventSource);
        close(fd);
        return 0;
    }

    Debug::log(LOG, "Socket2 accepted a new client at FD {}", ACCEPTEDCONNECTION);

    // add to event loop so we can close it when we need to
    auto* eventSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, ACCEPTEDCONNECTION, WL_EVENT_READABLE, onServerEvent, this);
    m_mClients.emplace(ACCEPTEDCONNECTION,
                       SClient{
                           {},
                           eventSource,
                       });

    return 0;
}

int CEventManager::onClientEvent(int fd, uint32_t mask) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        Debug::log(LOG, "Socket2 fd {} hung up", fd);
        removeClientByFD(fd);
        return 0;
    }

    if (mask & WL_EVENT_READABLE) {
        char       buf[1024];
        const auto RECEIVED = recv(fd, buf, sizeof(buf), 0);
        if (RECEIVED < 0) {
            Debug::log(ERR, "Socket2 fd {} sent invalid data", fd);
            removeClientByFD(fd);
            return 0;
        }
    }

    if (mask & WL_EVENT_WRITABLE) {
        const auto IT = m_mClients.find(fd);

        // send all queued events
        const auto& FD     = IT->first;
        auto&       client = IT->second;
        while (!client.events.empty()) {
            const auto& event = client.events.front();
            if (write(FD, event->c_str(), event->length()) < 0)
                break;

            client.events.pop_front();
        }

        // stop polling when we sent all events
        if (client.events.empty())
            wl_event_source_fd_update(client.eventSource, WL_EVENT_READABLE);
    }

    return 0;
}

std::unordered_map<int, CEventManager::SClient>::iterator CEventManager::removeClientByFD(int fd) {
    const auto IT = m_mClients.find(fd);
    wl_event_source_remove(IT->second.eventSource);
    close(fd);

    return m_mClients.erase(IT);
}

std::string CEventManager::formatEvent(const SHyprIPCEvent& event) const {
    std::string_view data        = event.data;
    auto             eventString = std::format("{}>>{}\n", event.event, data.substr(0, 1024));
    std::replace(eventString.begin() + event.event.length() + 2, eventString.end() - 1, '\n', ' ');
    return eventString;
}

void CEventManager::postEvent(const SHyprIPCEvent& event) {
    if (g_pCompositor->m_bIsShuttingDown) {
        Debug::log(WARN, "Suppressed (shutting down) event of type {}, content: {}", event.event, event.data);
        return;
    }

    const size_t MAX_QUEUED_EVENTS = 64;
    auto         sharedEvent       = makeShared<std::string>(formatEvent(event));
    for (auto it = m_mClients.begin(); it != m_mClients.end();) {
        const auto FD     = it->first;
        auto&      client = it->second;

        // try to send the event immediately
        if (write(FD, sharedEvent->c_str(), sharedEvent->length()) < 0) {
            const auto QUEUESIZE = client.events.size();
            if (QUEUESIZE >= MAX_QUEUED_EVENTS) {
                // too many events queued, remove the client
                it = removeClientByFD(FD);
                continue;
            }

            // queue it to send later if failed
            client.events.push_back(sharedEvent);

            // poll for write if queue was empty
            if (QUEUESIZE == 0)
                wl_event_source_fd_update(client.eventSource, WL_EVENT_READABLE | WL_EVENT_WRITABLE);
        }

        ++it;
    }
}
