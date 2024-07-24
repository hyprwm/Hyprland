#pragma once

#include "../defines.hpp"
#include "../protocols/ToplevelExport.hpp"
#include "../protocols/TextInputV1.hpp"
#include "../protocols/GlobalShortcuts.hpp"
#include "../protocols/Screencopy.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"
#include <unordered_map>

class CProtocolManager {
  public:
    CProtocolManager();
    ~CProtocolManager();

    // TODO: rewrite to use the new protocol framework
    std::unique_ptr<CToplevelExportProtocolManager>  m_pToplevelExportProtocolManager;
    std::unique_ptr<CTextInputV1ProtocolManager>     m_pTextInputV1ProtocolManager;
    std::unique_ptr<CGlobalShortcutsProtocolManager> m_pGlobalShortcutsProtocolManager;
    std::unique_ptr<CScreencopyProtocolManager>      m_pScreencopyProtocolManager;

  private:
    std::unordered_map<std::string, CHyprSignalListener> m_mModeChangeListeners;

    void                                                 onMonitorModeChange(CMonitor* pMonitor);
};

inline std::unique_ptr<CProtocolManager> g_pProtocolManager;
