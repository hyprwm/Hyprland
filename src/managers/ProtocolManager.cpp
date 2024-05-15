#include "ProtocolManager.hpp"

#include "../protocols/TearingControl.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/XDGOutput.hpp"
#include "../protocols/CursorShape.hpp"
#include "../protocols/IdleInhibit.hpp"
#include "../protocols/RelativePointer.hpp"
#include "../protocols/XDGDecoration.hpp"
#include "../protocols/AlphaModifier.hpp"
#include "../protocols/GammaControl.hpp"
#include "../protocols/ForeignToplevel.hpp"
#include "../protocols/PointerGestures.hpp"
#include "../protocols/ForeignToplevelWlr.hpp"
#include "../protocols/ShortcutsInhibit.hpp"
#include "../protocols/TextInputV3.hpp"
#include "../protocols/PointerConstraints.hpp"
#include "../protocols/OutputPower.hpp"
#include "../protocols/XDGActivation.hpp"
#include "../protocols/IdleNotify.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/InputMethodV2.hpp"
#include "../protocols/VirtualKeyboard.hpp"
#include "../protocols/VirtualPointer.hpp"
#include "../protocols/OutputManagement.hpp"
#include "../protocols/ServerDecorationKDE.hpp"
#include "../protocols/FocusGrab.hpp"
#include "../protocols/Tablet.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/DataDeviceWlr.hpp"
#include "../protocols/PrimarySelection.hpp"
#include "../protocols/XWaylandShell.hpp"

#include "../protocols/core/Seat.hpp"
#include "../protocols/core/DataDevice.hpp"

CProtocolManager::CProtocolManager() {

    // Core
    PROTO::seat = std::make_unique<CWLSeatProtocol>(&wl_seat_interface, 9, "WLSeat");
    PROTO::data = std::make_unique<CWLDataDeviceProtocol>(&wl_data_device_manager_interface, 3, "WLDataDevice");

    // Extensions
    PROTO::tearing             = std::make_unique<CTearingControlProtocol>(&wp_tearing_control_manager_v1_interface, 1, "TearingControl");
    PROTO::fractional          = std::make_unique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "FractionalScale");
    PROTO::xdgOutput           = std::make_unique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "XDGOutput");
    PROTO::cursorShape         = std::make_unique<CCursorShapeProtocol>(&wp_cursor_shape_manager_v1_interface, 1, "CursorShape");
    PROTO::idleInhibit         = std::make_unique<CIdleInhibitProtocol>(&zwp_idle_inhibit_manager_v1_interface, 1, "IdleInhibit");
    PROTO::relativePointer     = std::make_unique<CRelativePointerProtocol>(&zwp_relative_pointer_manager_v1_interface, 1, "RelativePointer");
    PROTO::xdgDecoration       = std::make_unique<CXDGDecorationProtocol>(&zxdg_decoration_manager_v1_interface, 1, "XDGDecoration");
    PROTO::alphaModifier       = std::make_unique<CAlphaModifierProtocol>(&wp_alpha_modifier_v1_interface, 1, "AlphaModifier");
    PROTO::gamma               = std::make_unique<CGammaControlProtocol>(&zwlr_gamma_control_manager_v1_interface, 1, "GammaControl");
    PROTO::foreignToplevel     = std::make_unique<CForeignToplevelProtocol>(&ext_foreign_toplevel_list_v1_interface, 1, "ForeignToplevel");
    PROTO::pointerGestures     = std::make_unique<CPointerGesturesProtocol>(&zwp_pointer_gestures_v1_interface, 3, "PointerGestures");
    PROTO::foreignToplevelWlr  = std::make_unique<CForeignToplevelWlrProtocol>(&zwlr_foreign_toplevel_manager_v1_interface, 3, "ForeignToplevelWlr");
    PROTO::shortcutsInhibit    = std::make_unique<CKeyboardShortcutsInhibitProtocol>(&zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1, "ShortcutsInhibit");
    PROTO::textInputV3         = std::make_unique<CTextInputV3Protocol>(&zwp_text_input_manager_v3_interface, 1, "TextInputV3");
    PROTO::constraints         = std::make_unique<CPointerConstraintsProtocol>(&zwp_pointer_constraints_v1_interface, 1, "PointerConstraints");
    PROTO::outputPower         = std::make_unique<COutputPowerProtocol>(&zwlr_output_power_manager_v1_interface, 1, "OutputPower");
    PROTO::activation          = std::make_unique<CXDGActivationProtocol>(&xdg_activation_v1_interface, 1, "XDGActivation");
    PROTO::idle                = std::make_unique<CIdleNotifyProtocol>(&ext_idle_notifier_v1_interface, 1, "IdleNotify");
    PROTO::sessionLock         = std::make_unique<CSessionLockProtocol>(&ext_session_lock_manager_v1_interface, 1, "SessionLock");
    PROTO::ime                 = std::make_unique<CInputMethodV2Protocol>(&zwp_input_method_manager_v2_interface, 1, "IMEv2");
    PROTO::virtualKeyboard     = std::make_unique<CVirtualKeyboardProtocol>(&zwp_virtual_keyboard_manager_v1_interface, 1, "VirtualKeyboard");
    PROTO::virtualPointer      = std::make_unique<CVirtualPointerProtocol>(&zwlr_virtual_pointer_manager_v1_interface, 2, "VirtualPointer");
    PROTO::outputManagement    = std::make_unique<COutputManagementProtocol>(&zwlr_output_manager_v1_interface, 4, "OutputManagement");
    PROTO::serverDecorationKDE = std::make_unique<CServerDecorationKDEProtocol>(&org_kde_kwin_server_decoration_manager_interface, 1, "ServerDecorationKDE");
    PROTO::focusGrab           = std::make_unique<CFocusGrabProtocol>(&hyprland_focus_grab_manager_v1_interface, 1, "FocusGrab");
    PROTO::tablet              = std::make_unique<CTabletV2Protocol>(&zwp_tablet_manager_v2_interface, 1, "TabletV2");
    PROTO::layerShell          = std::make_unique<CLayerShellProtocol>(&zwlr_layer_shell_v1_interface, 5, "LayerShell");
    PROTO::presentation        = std::make_unique<CPresentationProtocol>(&wp_presentation_interface, 1, "Presentation");
    PROTO::xdgShell            = std::make_unique<CXDGShellProtocol>(&xdg_wm_base_interface, 6, "XDGShell");
    PROTO::dataWlr             = std::make_unique<CDataDeviceWLRProtocol>(&zwlr_data_control_manager_v1_interface, 2, "DataDeviceWlr");
    PROTO::primarySelection    = std::make_unique<CPrimarySelectionProtocol>(&zwp_primary_selection_device_manager_v1_interface, 1, "PrimarySelection");
    PROTO::xwaylandShell       = std::make_unique<CXWaylandShellProtocol>(&xwayland_shell_v1_interface, 1, "XWaylandShell");

    // Old protocol implementations.
    // TODO: rewrite them to use hyprwayland-scanner.
    m_pToplevelExportProtocolManager  = std::make_unique<CToplevelExportProtocolManager>();
    m_pTextInputV1ProtocolManager     = std::make_unique<CTextInputV1ProtocolManager>();
    m_pGlobalShortcutsProtocolManager = std::make_unique<CGlobalShortcutsProtocolManager>();
    m_pScreencopyProtocolManager      = std::make_unique<CScreencopyProtocolManager>();
}
