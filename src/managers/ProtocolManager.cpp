#include "ProtocolManager.hpp"

#include "../protocols/TearingControl.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/XDGOutput.hpp"
#include "../protocols/CursorShape.hpp"
#include "../protocols/IdleInhibit.hpp"
#include "../protocols/RelativePointer.hpp"
#include "../protocols/XDGDecoration.hpp"
#include "../protocols/AlphaModifier.hpp"

#include "tearing-control-v1.hpp"
#include "fractional-scale-v1.hpp"
#include "xdg-output-unstable-v1.hpp"
#include "cursor-shape-v1.hpp"
#include "idle-inhibit-unstable-v1.hpp"
#include "relative-pointer-unstable-v1.hpp"
#include "xdg-decoration-unstable-v1.hpp"
#include "alpha-modifier-v1.hpp"

CProtocolManager::CProtocolManager() {

    PROTO::tearing         = std::make_unique<CTearingControlProtocol>(&wp_tearing_control_manager_v1_interface, 1, "TearingControl");
    PROTO::fractional      = std::make_unique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "FractionalScale");
    PROTO::xdgOutput       = std::make_unique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "XDGOutput");
    PROTO::cursorShape     = std::make_unique<CCursorShapeProtocol>(&wp_cursor_shape_manager_v1_interface, 1, "CursorShape");
    PROTO::idleInhibit     = std::make_unique<CIdleInhibitProtocol>(&zwp_idle_inhibit_manager_v1_interface, 1, "IdleInhibit");
    PROTO::relativePointer = std::make_unique<CRelativePointerProtocol>(&zwp_relative_pointer_manager_v1_interface, 1, "RelativePointer");
    PROTO::xdgDecoration   = std::make_unique<CXDGDecorationProtocol>(&zxdg_decoration_manager_v1_interface, 1, "XDGDecoration");
    PROTO::alphaModifier   = std::make_unique<CAlphaModifierProtocol>(&wp_alpha_modifier_v1_interface, 1, "AlphaModifier");

    // Old protocol implementations.
    // TODO: rewrite them to use hyprwayland-scanner.
    m_pToplevelExportProtocolManager  = std::make_unique<CToplevelExportProtocolManager>();
    m_pTextInputV1ProtocolManager     = std::make_unique<CTextInputV1ProtocolManager>();
    m_pGlobalShortcutsProtocolManager = std::make_unique<CGlobalShortcutsProtocolManager>();
    m_pScreencopyProtocolManager      = std::make_unique<CScreencopyProtocolManager>();
}
