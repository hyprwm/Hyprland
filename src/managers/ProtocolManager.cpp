#include "ProtocolManager.hpp"

#include "xdg-output-unstable-v1-protocol.h"

CProtocolManager::CProtocolManager() {
    m_pToplevelExportProtocolManager  = std::make_unique<CToplevelExportProtocolManager>();
    m_pFractionalScaleProtocolManager = std::make_unique<CFractionalScaleProtocolManager>();
    m_pTextInputV1ProtocolManager     = std::make_unique<CTextInputV1ProtocolManager>();
    m_pGlobalShortcutsProtocolManager = std::make_unique<CGlobalShortcutsProtocolManager>();
    m_pScreencopyProtocolManager      = std::make_unique<CScreencopyProtocolManager>();

    m_pXDGOutputProtocol = std::make_unique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "XDGOutput");
}
