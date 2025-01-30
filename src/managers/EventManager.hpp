#pragma once
#include <vector>

#include "../defines.hpp"
#include "../helpers/memory/Memory.hpp"

struct SHyprIPCEvent {
    std::string event;
    std::string data;
};

class CEventManager {
  public:
    CEventManager();
    ~CEventManager();

    void postEvent(const SHyprIPCEvent& event);

  private:
    std::string formatEvent(const SHyprIPCEvent& event) const;

    static int  onServerEvent(int fd, uint32_t mask, void* data);
    static int  onClientEvent(int fd, uint32_t mask, void* data);

    int         onServerEvent(int fd, uint32_t mask);
    int         onClientEvent(int fd, uint32_t mask);

    struct SClient {
        int                          fd = -1;
        std::vector<SP<std::string>> events;
        wl_event_source*             eventSource = nullptr;
    };

    std::vector<SClient>::iterator findClientByFD(int fd);
    std::vector<SClient>::iterator removeClientByFD(int fd);

  private:
    int                  m_iSocketFD    = -1;
    wl_event_source*     m_pEventSource = nullptr;

    std::vector<SClient> m_vClients;
};

inline UP<CEventManager> g_pEventManager;
