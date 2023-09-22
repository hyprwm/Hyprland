#pragma once

#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "../common.hpp"

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
  private:
    void                                         flushEvents();

    std::mutex                                   eventQueueMutex;
    std::deque<SHyprIPCEvent>                    m_dQueuedEvents;

    std::deque<std::pair<int, wl_event_source*>> m_dAcceptedSocketFDs;
};

inline std::unique_ptr<CEventManager> g_pEventManager;
