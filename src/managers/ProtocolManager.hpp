#pragma once

#include "../defines.hpp"
#include "../protocols/ToplevelExport.hpp"

class CProtocolManager {
public:
    CProtocolManager();

    std::unique_ptr<CToplevelExportProtocolManager> m_pToplevelExportProtocolManager;
};

inline std::unique_ptr<CProtocolManager> g_pProtocolManager;
