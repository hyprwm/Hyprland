#pragma once
#include <deque>
#include <fstream>
#include <mutex>

#include "../defines.hpp"
#include "../helpers/MiscFunctions.hpp"

struct SHyprIPCEvent {
    std::string event;
    std::string data;
};

class CEventManager {
  public:
    CEventManager();

    void        postEvent(const SHyprIPCEvent event);

    void        startThread();

    std::thread m_tThread;

    int         m_iSocketFD = -1;

    int         onSocket2Write(int fd, uint32_t mask);
    int         onFDWrite(int fd, uint32_t mask);

  private:
    void                                         flushEvents();

    std::mutex                                   eventQueueMutex;
    std::deque<SHyprIPCEvent>                    m_dQueuedEvents;

    std::deque<std::pair<int, wl_event_source*>> m_dAcceptedSocketFDs;

    wl_event_source*                             m_pEventSource = nullptr;
};

inline std::unique_ptr<CEventManager> g_pEventManager;
