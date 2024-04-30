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

#include "tearing-control-v1.hpp"
#include "fractional-scale-v1.hpp"
#include "xdg-output-unstable-v1.hpp"
#include "cursor-shape-v1.hpp"
#include "idle-inhibit-unstable-v1.hpp"
#include "relative-pointer-unstable-v1.hpp"
#include "xdg-decoration-unstable-v1.hpp"
#include "alpha-modifier-v1.hpp"
#include "wlr-gamma-control-unstable-v1.hpp"
#include "ext-foreign-toplevel-list-v1.hpp"
#include "pointer-gestures-unstable-v1.hpp"
#include "wlr-foreign-toplevel-management-unstable-v1.hpp"
#include "keyboard-shortcuts-inhibit-unstable-v1.hpp"
#include "text-input-unstable-v3.hpp"
#include "pointer-constraints-unstable-v1.hpp"
#include "wlr-output-power-management-unstable-v1.hpp"
#include "xdg-activation-v1.hpp"
#include "ext-idle-notify-v1.hpp"
#include "ext-session-lock-v1.hpp"

CProtocolManager::CProtocolManager() {

    PROTO::tearing            = std::make_unique<CTearingControlProtocol>(&wp_tearing_control_manager_v1_interface, 1, "TearingControl");
    PROTO::fractional         = std::make_unique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "FractionalScale");
    PROTO::xdgOutput          = std::make_unique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "XDGOutput");
    PROTO::cursorShape        = std::make_unique<CCursorShapeProtocol>(&wp_cursor_shape_manager_v1_interface, 1, "CursorShape");
    PROTO::idleInhibit        = std::make_unique<CIdleInhibitProtocol>(&zwp_idle_inhibit_manager_v1_interface, 1, "IdleInhibit");
    PROTO::relativePointer    = std::make_unique<CRelativePointerProtocol>(&zwp_relative_pointer_manager_v1_interface, 1, "RelativePointer");
    PROTO::xdgDecoration      = std::make_unique<CXDGDecorationProtocol>(&zxdg_decoration_manager_v1_interface, 1, "XDGDecoration");
    PROTO::alphaModifier      = std::make_unique<CAlphaModifierProtocol>(&wp_alpha_modifier_v1_interface, 1, "AlphaModifier");
    PROTO::gamma              = std::make_unique<CGammaControlProtocol>(&zwlr_gamma_control_manager_v1_interface, 1, "GammaControl");
    PROTO::foreignToplevel    = std::make_unique<CForeignToplevelProtocol>(&ext_foreign_toplevel_list_v1_interface, 1, "ForeignToplevel");
    PROTO::pointerGestures    = std::make_unique<CPointerGesturesProtocol>(&zwp_pointer_gestures_v1_interface, 3, "PointerGestures");
    PROTO::foreignToplevelWlr = std::make_unique<CForeignToplevelWlrProtocol>(&zwlr_foreign_toplevel_manager_v1_interface, 3, "ForeignToplevelWlr");
    PROTO::shortcutsInhibit   = std::make_unique<CKeyboardShortcutsInhibitProtocol>(&zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1, "ShortcutsInhibit");
    PROTO::textInputV3        = std::make_unique<CTextInputV3Protocol>(&zwp_text_input_manager_v3_interface, 1, "TextInputV3");
    PROTO::constraints        = std::make_unique<CPointerConstraintsProtocol>(&zwp_pointer_constraints_v1_interface, 1, "PointerConstraints");
    PROTO::outputPower        = std::make_unique<COutputPowerProtocol>(&zwlr_output_power_manager_v1_interface, 1, "OutputPower");
    PROTO::activation         = std::make_unique<CXDGActivationProtocol>(&xdg_activation_v1_interface, 1, "XDGActivation");
    PROTO::idle               = std::make_unique<CIdleNotifyProtocol>(&ext_idle_notifier_v1_interface, 1, "IdleNotify");
    PROTO::sessionLock        = std::make_unique<CSessionLockProtocol>(&ext_session_lock_manager_v1_interface, 1, "SessionLock");

    // Old protocol implementations.
    // TODO: rewrite them to use hyprwayland-scanner.
    m_pToplevelExportProtocolManager  = std::make_unique<CToplevelExportProtocolManager>();
    m_pTextInputV1ProtocolManager     = std::make_unique<CTextInputV1ProtocolManager>();
    m_pGlobalShortcutsProtocolManager = std::make_unique<CGlobalShortcutsProtocolManager>();
    m_pScreencopyProtocolManager      = std::make_unique<CScreencopyProtocolManager>();
}
