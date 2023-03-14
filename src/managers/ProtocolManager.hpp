#pragma once

#include "../defines.hpp"
#include "../protocols/ToplevelExport.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/TextInputV1.hpp"

class CProtocolManager {
  public:
    CProtocolManager();

    std::unique_ptr<CToplevelExportProtocolManager>  m_pToplevelExportProtocolManager;
    std::unique_ptr<CFractionalScaleProtocolManager> m_pFractionalScaleProtocolManager;
    std::unique_ptr<CTextInputV1ProtocolManager>     m_pTextInputV1ProtocolManager;
};

inline std::unique_ptr<CProtocolManager> g_pProtocolManager;
