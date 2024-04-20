#include "ProtocolManager.hpp"

#include "../protocols/TearingControl.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/XDGOutput.hpp"

#include "tearing-control-v1.hpp"
#include "fractional-scale-v1.hpp"
#include "xdg-output-unstable-v1.hpp"

CProtocolManager::CProtocolManager() {

    PROTO::tearing    = std::make_unique<CTearingControlProtocol>(&wp_tearing_control_manager_v1_interface, 1, "TearingControl");
    PROTO::fractional = std::make_unique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "FractionalScale");
    PROTO::xdgOutput  = std::make_unique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "XDGOutput");

    // Old protocol implementations.
    // TODO: rewrite them to use hyprwayland-scanner.
    m_pToplevelExportProtocolManager  = std::make_unique<CToplevelExportProtocolManager>();
    m_pTextInputV1ProtocolManager     = std::make_unique<CTextInputV1ProtocolManager>();
    m_pGlobalShortcutsProtocolManager = std::make_unique<CGlobalShortcutsProtocolManager>();
    m_pScreencopyProtocolManager      = std::make_unique<CScreencopyProtocolManager>();
}
