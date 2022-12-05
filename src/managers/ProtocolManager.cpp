#include "ProtocolManager.hpp"

CProtocolManager::CProtocolManager() {
    m_pToplevelExportProtocolManager = std::make_unique<CToplevelExportProtocolManager>();
}