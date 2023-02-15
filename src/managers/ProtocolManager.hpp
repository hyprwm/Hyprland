#pragma once

#include "../defines.hpp"
#include "../protocols/ToplevelExport.hpp"
#include "../protocols/FractionalScale.hpp"

class CProtocolManager {
  public:
    CProtocolManager();

    std::unique_ptr<CToplevelExportProtocolManager>  m_pToplevelExportProtocolManager;
    std::unique_ptr<CFractionalScaleProtocolManager> m_pFractionalScaleProtocolManager;
};

inline std::unique_ptr<CProtocolManager> g_pProtocolManager;
