#pragma once
#include <deque>
#include <fstream>

#include "../defines.hpp"
#include "../helpers/MiscFunctions.hpp"

struct SHyprIPCEvent {
    std::string event;
    std::string data;
};

class CEventManager {
public:
    CEventManager();

    void postEvent(const SHyprIPCEvent event);

    void startThread();

private:

    bool m_bCanReadEventQueue = true;
    bool m_bCanWriteEventQueue = true;
    std::deque<SHyprIPCEvent> m_dQueuedEvents;

    std::deque<int> m_dAcceptedSocketFDs;
};

inline std::unique_ptr<CEventManager> g_pEventManager;