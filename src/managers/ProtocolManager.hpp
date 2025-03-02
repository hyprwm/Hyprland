#pragma once

#include "../defines.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"
#include <unordered_map>

class CProtocolManager {
  public:
    CProtocolManager();

    bool isGlobalPrivileged(const wl_global* global);

  private:
    std::unordered_map<std::string, CHyprSignalListener> m_mModeChangeListeners;

    void                                                 onMonitorModeChange(PHLMONITOR pMonitor);
};

inline UP<CProtocolManager> g_pProtocolManager;
