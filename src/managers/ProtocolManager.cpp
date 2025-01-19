#include "ProtocolManager.hpp"

#include "../config/ConfigValue.hpp"

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
#include "../protocols/LockNotify.hpp"
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
#include "../protocols/Viewporter.hpp"
#include "../protocols/MesaDRM.hpp"
#include "../protocols/LinuxDMABUF.hpp"
#include "../protocols/DRMLease.hpp"
#include "../protocols/DRMSyncobj.hpp"
#include "../protocols/Screencopy.hpp"
#include "../protocols/ToplevelExport.hpp"
#include "../protocols/TextInputV1.hpp"
#include "../protocols/GlobalShortcuts.hpp"
#include "../protocols/XDGDialog.hpp"
#include "../protocols/SinglePixel.hpp"
#include "../protocols/SecurityContext.hpp"
#include "../protocols/CTMControl.hpp"
#include "../protocols/HyprlandSurface.hpp"

#include "../protocols/core/Seat.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/core/Subcompositor.hpp"
#include "../protocols/core/Output.hpp"
#include "../protocols/core/Shm.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../protocols/FrogColorManagement.hpp"

#include "../helpers/Monitor.hpp"
#include "../render/Renderer.hpp"
#include "../Compositor.hpp"

#include <aquamarine/buffer/Buffer.hpp>
#include <aquamarine/backend/Backend.hpp>

void CProtocolManager::onMonitorModeChange(PHLMONITOR pMonitor) {
    const bool ISMIRROR = pMonitor->isMirror();

    // onModeChanged we check if the current mirror status matches the global.
    // mirrored outputs should have their global removed, as they are not physical parts of the
    // layout.

    if (ISMIRROR && PROTO::outputs.contains(pMonitor->szName))
        PROTO::outputs.at(pMonitor->szName)->remove();
    else if (!ISMIRROR && (!PROTO::outputs.contains(pMonitor->szName) || PROTO::outputs.at(pMonitor->szName)->isDefunct())) {
        if (PROTO::outputs.contains(pMonitor->szName))
            PROTO::outputs.erase(pMonitor->szName);
        PROTO::outputs.emplace(pMonitor->szName, makeShared<CWLOutputProtocol>(&wl_output_interface, 4, std::format("WLOutput ({})", pMonitor->szName), pMonitor->self.lock()));
    }

    if (PROTO::colorManagement && g_pCompositor->shouldChangePreferredImageDescription())
        PROTO::colorManagement->onImagePreferredChanged();
}

CProtocolManager::CProtocolManager() {

    static const auto PENABLEEXPLICIT = CConfigValue<Hyprlang::INT>("render:explicit_sync");
    static const auto PENABLEXXCM     = CConfigValue<Hyprlang::INT>("experimental:xx_color_management_v4");

    // Outputs are a bit dumb, we have to agree.
    static auto P = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any param) {
        auto M = std::any_cast<PHLMONITOR>(param);

        // ignore mirrored outputs. I don't think this will ever be hit as mirrors are applied after
        // this event is emitted iirc.
        // also ignore the fallback
        if (M->isMirror() || M == g_pCompositor->m_pUnsafeOutput)
            return;

        if (PROTO::outputs.contains(M->szName))
            PROTO::outputs.erase(M->szName);

        auto ref = makeShared<CWLOutputProtocol>(&wl_output_interface, 4, std::format("WLOutput ({})", M->szName), M->self.lock());
        PROTO::outputs.emplace(M->szName, ref);
        ref->self = ref;

        m_mModeChangeListeners[M->szName] = M->events.modeChanged.registerListener([M, this](std::any d) { onMonitorModeChange(M); });
    });

    static auto P2 = g_pHookSystem->hookDynamic("monitorRemoved", [this](void* self, SCallbackInfo& info, std::any param) {
        auto M = std::any_cast<PHLMONITOR>(param);
        if (!PROTO::outputs.contains(M->szName))
            return;
        PROTO::outputs.at(M->szName)->remove();
        m_mModeChangeListeners.erase(M->szName);
    });

    // Core
    PROTO::seat          = std::make_unique<CWLSeatProtocol>(&wl_seat_interface, 9, "WLSeat");
    PROTO::data          = std::make_unique<CWLDataDeviceProtocol>(&wl_data_device_manager_interface, 3, "WLDataDevice");
    PROTO::compositor    = std::make_unique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "WLCompositor");
    PROTO::subcompositor = std::make_unique<CWLSubcompositorProtocol>(&wl_subcompositor_interface, 1, "WLSubcompositor");
    PROTO::shm           = std::make_unique<CWLSHMProtocol>(&wl_shm_interface, 1, "WLSHM");

    // Extensions
    PROTO::viewport            = std::make_unique<CViewporterProtocol>(&wp_viewporter_interface, 1, "Viewporter");
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
    PROTO::textInputV1         = std::make_unique<CTextInputV1Protocol>(&zwp_text_input_manager_v1_interface, 1, "TextInputV1");
    PROTO::textInputV3         = std::make_unique<CTextInputV3Protocol>(&zwp_text_input_manager_v3_interface, 1, "TextInputV3");
    PROTO::constraints         = std::make_unique<CPointerConstraintsProtocol>(&zwp_pointer_constraints_v1_interface, 1, "PointerConstraints");
    PROTO::outputPower         = std::make_unique<COutputPowerProtocol>(&zwlr_output_power_manager_v1_interface, 1, "OutputPower");
    PROTO::activation          = std::make_unique<CXDGActivationProtocol>(&xdg_activation_v1_interface, 1, "XDGActivation");
    PROTO::idle                = std::make_unique<CIdleNotifyProtocol>(&ext_idle_notifier_v1_interface, 1, "IdleNotify");
    PROTO::lockNotify          = std::make_unique<CLockNotifyProtocol>(&hyprland_lock_notifier_v1_interface, 1, "IdleNotify");
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
    PROTO::screencopy          = std::make_unique<CScreencopyProtocol>(&zwlr_screencopy_manager_v1_interface, 3, "Screencopy");
    PROTO::toplevelExport      = std::make_unique<CToplevelExportProtocol>(&hyprland_toplevel_export_manager_v1_interface, 2, "ToplevelExport");
    PROTO::globalShortcuts     = std::make_unique<CGlobalShortcutsProtocol>(&hyprland_global_shortcuts_manager_v1_interface, 1, "GlobalShortcuts");
    PROTO::xdgDialog           = std::make_unique<CXDGDialogProtocol>(&xdg_dialog_v1_interface, 1, "XDGDialog");
    PROTO::singlePixel         = std::make_unique<CSinglePixelProtocol>(&wp_single_pixel_buffer_manager_v1_interface, 1, "SinglePixel");
    PROTO::securityContext     = std::make_unique<CSecurityContextProtocol>(&wp_security_context_manager_v1_interface, 1, "SecurityContext");
    PROTO::ctm                 = std::make_unique<CHyprlandCTMControlProtocol>(&hyprland_ctm_control_manager_v1_interface, 1, "CTMControl");
    PROTO::hyprlandSurface     = std::make_unique<CHyprlandSurfaceProtocol>(&hyprland_surface_manager_v1_interface, 1, "HyprlandSurface");

    if (*PENABLEXXCM) {
        PROTO::colorManagement     = std::make_unique<CColorManagementProtocol>(&xx_color_manager_v4_interface, 1, "ColorManagement");
        PROTO::frogColorManagement = std::make_unique<CFrogColorManagementProtocol>(&frog_color_management_factory_v1_interface, 1, "FrogColorManagement");
    }

    for (auto const& b : g_pCompositor->m_pAqBackend->getImplementations()) {
        if (b->type() != Aquamarine::AQ_BACKEND_DRM)
            continue;

        PROTO::lease = std::make_unique<CDRMLeaseProtocol>(&wp_drm_lease_device_v1_interface, 1, "DRMLease");
        if (*PENABLEEXPLICIT)
            PROTO::sync = std::make_unique<CDRMSyncobjProtocol>(&wp_linux_drm_syncobj_manager_v1_interface, 1, "DRMSyncobj");
        break;
    }

    if (g_pHyprOpenGL->getDRMFormats().size() > 0) {
        PROTO::mesaDRM  = std::make_unique<CMesaDRMProtocol>(&wl_drm_interface, 2, "MesaDRM");
        PROTO::linuxDma = std::make_unique<CLinuxDMABufV1Protocol>(&zwp_linux_dmabuf_v1_interface, 5, "LinuxDMABUF");
    } else
        Debug::log(WARN, "ProtocolManager: Not binding linux-dmabuf and MesaDRM: DMABUF not available");
}

CProtocolManager::~CProtocolManager() {
    // this is dumb but i don't want to replace all 600 PROTO with the right thing

    // Output
    PROTO::outputs.clear();

    // Core
    PROTO::seat.reset();
    PROTO::data.reset();
    PROTO::compositor.reset();
    PROTO::subcompositor.reset();
    PROTO::shm.reset();

    // Extensions
    PROTO::viewport.reset();
    PROTO::tearing.reset();
    PROTO::fractional.reset();
    PROTO::xdgOutput.reset();
    PROTO::cursorShape.reset();
    PROTO::idleInhibit.reset();
    PROTO::relativePointer.reset();
    PROTO::xdgDecoration.reset();
    PROTO::alphaModifier.reset();
    PROTO::gamma.reset();
    PROTO::foreignToplevel.reset();
    PROTO::pointerGestures.reset();
    PROTO::foreignToplevelWlr.reset();
    PROTO::shortcutsInhibit.reset();
    PROTO::textInputV1.reset();
    PROTO::textInputV3.reset();
    PROTO::constraints.reset();
    PROTO::outputPower.reset();
    PROTO::activation.reset();
    PROTO::idle.reset();
    PROTO::lockNotify.reset();
    PROTO::sessionLock.reset();
    PROTO::ime.reset();
    PROTO::virtualKeyboard.reset();
    PROTO::virtualPointer.reset();
    PROTO::outputManagement.reset();
    PROTO::serverDecorationKDE.reset();
    PROTO::focusGrab.reset();
    PROTO::tablet.reset();
    PROTO::layerShell.reset();
    PROTO::presentation.reset();
    PROTO::xdgShell.reset();
    PROTO::dataWlr.reset();
    PROTO::primarySelection.reset();
    PROTO::xwaylandShell.reset();
    PROTO::screencopy.reset();
    PROTO::toplevelExport.reset();
    PROTO::globalShortcuts.reset();
    PROTO::xdgDialog.reset();
    PROTO::singlePixel.reset();
    PROTO::securityContext.reset();
    PROTO::ctm.reset();
    PROTO::hyprlandSurface.reset();

    PROTO::lease.reset();
    PROTO::sync.reset();
    PROTO::mesaDRM.reset();
    PROTO::linuxDma.reset();
}

bool CProtocolManager::isGlobalPrivileged(const wl_global* global) {
    if (!global)
        return false;

    for (auto& [k, v] : PROTO::outputs) {
        if (global == v->getGlobal())
            return false;
    }

    // this is a static whitelist of allowed protocols,
    // outputs are dynamic so we checked them above
    // clang-format off
    static const std::vector<wl_global*> ALLOWED_WHITELIST = {
        PROTO::seat->getGlobal(),
        PROTO::data->getGlobal(),
        PROTO::compositor->getGlobal(),
        PROTO::subcompositor->getGlobal(),
        PROTO::shm->getGlobal(),
        PROTO::viewport->getGlobal(),
        PROTO::tearing->getGlobal(),
        PROTO::fractional->getGlobal(),
        PROTO::cursorShape->getGlobal(),
        PROTO::idleInhibit->getGlobal(),
        PROTO::relativePointer->getGlobal(),
        PROTO::xdgDecoration->getGlobal(),
        PROTO::alphaModifier->getGlobal(),
        PROTO::pointerGestures->getGlobal(),
        PROTO::shortcutsInhibit->getGlobal(),
        PROTO::textInputV1->getGlobal(),
        PROTO::textInputV3->getGlobal(),
        PROTO::constraints->getGlobal(),
        PROTO::activation->getGlobal(),
        PROTO::idle->getGlobal(),
        PROTO::ime->getGlobal(),
        PROTO::virtualKeyboard->getGlobal(),
        PROTO::virtualPointer->getGlobal(),
        PROTO::serverDecorationKDE->getGlobal(),
        PROTO::tablet->getGlobal(),
        PROTO::presentation->getGlobal(),
        PROTO::xdgShell->getGlobal(),
        PROTO::xdgDialog->getGlobal(),
        PROTO::singlePixel->getGlobal(),
        PROTO::primarySelection->getGlobal(),
			  PROTO::hyprlandSurface->getGlobal(),
        PROTO::sync     ? PROTO::sync->getGlobal()      : nullptr,
        PROTO::mesaDRM  ? PROTO::mesaDRM->getGlobal()   : nullptr,
        PROTO::linuxDma ? PROTO::linuxDma->getGlobal()  : nullptr,
    };
    // clang-format on

    return std::find(ALLOWED_WHITELIST.begin(), ALLOWED_WHITELIST.end(), global) == ALLOWED_WHITELIST.end();
}
