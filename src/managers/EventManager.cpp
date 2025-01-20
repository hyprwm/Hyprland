#include "EventManager.hpp"
#include "../Compositor.hpp"

#include <algorithm>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
using namespace Hyprutils::OS;

CEventManager::CEventManager() : m_iSocketFD(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) {
    if (!m_iSocketFD.isValid()) {
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

    if (bind(m_iSocketFD.get(), (sockaddr*)&SERVERADDRESS, SUN_LEN(&SERVERADDRESS)) < 0) {
        Debug::log(ERR, "Couldn't bind the Hyprland Socket 2. (3) IPC will not work.");
        return;
    }

    // 10 max queued.
    if (listen(m_iSocketFD.get(), 10) < 0) {
        Debug::log(ERR, "Couldn't listen on the Hyprland Socket 2. (4) IPC will not work.");
        return;
    }

    m_pEventSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, m_iSocketFD.get(), WL_EVENT_READABLE, onClientEvent, nullptr);
}

CEventManager::~CEventManager() {
    for (const auto& client : m_vClients) {
        wl_event_source_remove(client.eventSource);
    }

    if (m_pEventSource != nullptr)
        wl_event_source_remove(m_pEventSource);
}

int CEventManager::onServerEvent(int fd, uint32_t mask, void* data) {
    return g_pEventManager->onClientEvent(fd, mask);
}

int CEventManager::onClientEvent(int fd, uint32_t mask, void* data) {
    return g_pEventManager->onServerEvent(fd, mask);
}

int CEventManager::onServerEvent(int fd, uint32_t mask) {
    if (mask & WL_EVENT_ERROR || mask & WL_EVENT_HANGUP) {
        Debug::log(ERR, "Socket2 hangup?? IPC broke");

        wl_event_source_remove(m_pEventSource);
        m_pEventSource = nullptr;
        m_iSocketFD.reset();

        return 0;
    }

    sockaddr_in     clientAddress;
    socklen_t       clientSize = sizeof(clientAddress);
    CFileDescriptor ACCEPTEDCONNECTION{accept4(m_iSocketFD.get(), (sockaddr*)&clientAddress, &clientSize, SOCK_CLOEXEC | SOCK_NONBLOCK)};
    if (!ACCEPTEDCONNECTION.isValid()) {
        if (errno != EAGAIN) {
            Debug::log(ERR, "Socket2 failed receiving connection, errno: {}", errno);
            wl_event_source_remove(m_pEventSource);
            m_pEventSource = nullptr;
            m_iSocketFD.reset();
        }

        return 0;
    }

    Debug::log(LOG, "Socket2 accepted a new client at FD {}", ACCEPTEDCONNECTION.get());

    // add to event loop so we can close it when we need to
    auto* eventSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, ACCEPTEDCONNECTION.get(), 0, onServerEvent, nullptr);
    m_vClients.emplace_back(SClient{
        std::move(ACCEPTEDCONNECTION),
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

    if (mask & WL_EVENT_WRITABLE) {
        const auto CLIENTIT = findClientByFD(fd);

        // send all queued events
        while (!CLIENTIT->events.empty()) {
            const auto& event = CLIENTIT->events.front();
            if (write(CLIENTIT->fd.get(), event->c_str(), event->length()) < 0)
                break;

            CLIENTIT->events.erase(CLIENTIT->events.begin());
        }

        // stop polling when we sent all events
        if (CLIENTIT->events.empty())
            wl_event_source_fd_update(CLIENTIT->eventSource, 0);
    }

    return 0;
}

std::vector<CEventManager::SClient>::iterator CEventManager::findClientByFD(int fd) {
    return std::find_if(m_vClients.begin(), m_vClients.end(), [fd](const auto& client) { return client.fd.get() == fd; });
}

std::vector<CEventManager::SClient>::iterator CEventManager::removeClientByFD(int fd) {
    const auto CLIENTIT = findClientByFD(fd);
    wl_event_source_remove(CLIENTIT->eventSource);

    return m_vClients.erase(CLIENTIT);
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
    for (auto it = m_vClients.begin(); it != m_vClients.end();) {
        // try to send the event immediately if the queue is empty
        const auto QUEUESIZE = it->events.size();
        if (QUEUESIZE > 0 || write(it->fd.get(), sharedEvent->c_str(), sharedEvent->length()) < 0) {
            if (QUEUESIZE >= MAX_QUEUED_EVENTS) {
                // too many events queued, remove the client
                Debug::log(ERR, "Socket2 fd {} overflowed event queue, removing", it->fd.get());
                it = removeClientByFD(it->fd.get());
                continue;
            }

            // queue it to send later if failed
            it->events.push_back(sharedEvent);

            // poll for write if queue was empty
            if (QUEUESIZE == 0)
                wl_event_source_fd_update(it->eventSource, WL_EVENT_WRITABLE);
        }

        ++it;
    }
}
