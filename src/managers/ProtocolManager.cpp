#include "ProtocolManager.hpp"

#include "../protocols/TearingControl.hpp"
#include "../protocols/FractionalScale.hpp"

#include "xdg-output-unstable-v1-protocol.h"
#include "tearing-control-v1.hpp"
#include "fractional-scale-v1.hpp"

CProtocolManager::CProtocolManager() {
    m_pToplevelExportProtocolManager  = std::make_unique<CToplevelExportProtocolManager>();
    m_pTextInputV1ProtocolManager     = std::make_unique<CTextInputV1ProtocolManager>();
    m_pGlobalShortcutsProtocolManager = std::make_unique<CGlobalShortcutsProtocolManager>();
    m_pScreencopyProtocolManager      = std::make_unique<CScreencopyProtocolManager>();

    m_pXDGOutputProtocol = std::make_unique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "XDGOutput");
    PROTO::tearing       = std::make_unique<CTearingControlProtocol>(&wp_tearing_control_manager_v1_interface, 1, "TearingControl");
    PROTO::fractional    = std::make_unique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "FractionalScale");
}
