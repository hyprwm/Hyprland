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
#include "../protocols/XXColorManagement.hpp"
#include "../protocols/FrogColorManagement.hpp"
#include "../protocols/ContentType.hpp"
#include "../protocols/XDGTag.hpp"

#include "../helpers/Monitor.hpp"
#include "../render/Renderer.hpp"
#include "../Compositor.hpp"
#include "content-type-v1.hpp"

#include <aquamarine/buffer/Buffer.hpp>
#include <aquamarine/backend/Backend.hpp>

// ********************************************************************************************
// * IMPORTANT: make sure to .reset() any protocol UP's you create! (put reset in destructor) *
// * otherwise Hyprland might crash when exiting.                                             *
// ********************************************************************************************

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

    if (PROTO::colorManagement && g_pCompositor->shouldChangePreferredImageDescription()) {
        Debug::log(ERR, "FIXME: color management protocol is enabled, need a preferred image description id");
        PROTO::colorManagement->onImagePreferredChanged(0);
    }
}

CProtocolManager::CProtocolManager() {

    static const auto PENABLEEXPLICIT = CConfigValue<Hyprlang::INT>("render:explicit_sync");
    static const auto PENABLECM       = CConfigValue<Hyprlang::INT>("render:cm_enabled");
    static const auto PENABLEXXCM     = CConfigValue<Hyprlang::INT>("experimental:xx_color_management_v4");
    static const auto PDEBUGCM        = CConfigValue<Hyprlang::INT>("debug:full_cm_proto");

    // Outputs are a bit dumb, we have to agree.
    static auto P = g_pHookSystem->hookDynamic("monitorAdded", [this](void* self, SCallbackInfo& info, std::any param) {
        auto M = std::any_cast<PHLMONITOR>(param);

        // ignore mirrored outputs. I don't think this will ever be hit as mirrors are applied after
        // this event is emitted iirc.
        // also ignore the fallback
        if (M->isMirror() || M == g_pCompositor->m_unsafeOutput)
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
    PROTO::seat          = makeUnique<CWLSeatProtocol>(&wl_seat_interface, 9, "WLSeat");
    PROTO::data          = makeUnique<CWLDataDeviceProtocol>(&wl_data_device_manager_interface, 3, "WLDataDevice");
    PROTO::compositor    = makeUnique<CWLCompositorProtocol>(&wl_compositor_interface, 6, "WLCompositor");
    PROTO::subcompositor = makeUnique<CWLSubcompositorProtocol>(&wl_subcompositor_interface, 1, "WLSubcompositor");
    PROTO::shm           = makeUnique<CWLSHMProtocol>(&wl_shm_interface, 1, "WLSHM");

    // Extensions
    PROTO::viewport            = makeUnique<CViewporterProtocol>(&wp_viewporter_interface, 1, "Viewporter");
    PROTO::tearing             = makeUnique<CTearingControlProtocol>(&wp_tearing_control_manager_v1_interface, 1, "TearingControl");
    PROTO::fractional          = makeUnique<CFractionalScaleProtocol>(&wp_fractional_scale_manager_v1_interface, 1, "FractionalScale");
    PROTO::xdgOutput           = makeUnique<CXDGOutputProtocol>(&zxdg_output_manager_v1_interface, 3, "XDGOutput");
    PROTO::cursorShape         = makeUnique<CCursorShapeProtocol>(&wp_cursor_shape_manager_v1_interface, 1, "CursorShape");
    PROTO::idleInhibit         = makeUnique<CIdleInhibitProtocol>(&zwp_idle_inhibit_manager_v1_interface, 1, "IdleInhibit");
    PROTO::relativePointer     = makeUnique<CRelativePointerProtocol>(&zwp_relative_pointer_manager_v1_interface, 1, "RelativePointer");
    PROTO::xdgDecoration       = makeUnique<CXDGDecorationProtocol>(&zxdg_decoration_manager_v1_interface, 1, "XDGDecoration");
    PROTO::alphaModifier       = makeUnique<CAlphaModifierProtocol>(&wp_alpha_modifier_v1_interface, 1, "AlphaModifier");
    PROTO::gamma               = makeUnique<CGammaControlProtocol>(&zwlr_gamma_control_manager_v1_interface, 1, "GammaControl");
    PROTO::foreignToplevel     = makeUnique<CForeignToplevelProtocol>(&ext_foreign_toplevel_list_v1_interface, 1, "ForeignToplevel");
    PROTO::pointerGestures     = makeUnique<CPointerGesturesProtocol>(&zwp_pointer_gestures_v1_interface, 3, "PointerGestures");
    PROTO::foreignToplevelWlr  = makeUnique<CForeignToplevelWlrProtocol>(&zwlr_foreign_toplevel_manager_v1_interface, 3, "ForeignToplevelWlr");
    PROTO::shortcutsInhibit    = makeUnique<CKeyboardShortcutsInhibitProtocol>(&zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1, "ShortcutsInhibit");
    PROTO::textInputV1         = makeUnique<CTextInputV1Protocol>(&zwp_text_input_manager_v1_interface, 1, "TextInputV1");
    PROTO::textInputV3         = makeUnique<CTextInputV3Protocol>(&zwp_text_input_manager_v3_interface, 1, "TextInputV3");
    PROTO::constraints         = makeUnique<CPointerConstraintsProtocol>(&zwp_pointer_constraints_v1_interface, 1, "PointerConstraints");
    PROTO::outputPower         = makeUnique<COutputPowerProtocol>(&zwlr_output_power_manager_v1_interface, 1, "OutputPower");
    PROTO::activation          = makeUnique<CXDGActivationProtocol>(&xdg_activation_v1_interface, 1, "XDGActivation");
    PROTO::idle                = makeUnique<CIdleNotifyProtocol>(&ext_idle_notifier_v1_interface, 2, "IdleNotify");
    PROTO::lockNotify          = makeUnique<CLockNotifyProtocol>(&hyprland_lock_notifier_v1_interface, 1, "IdleNotify");
    PROTO::sessionLock         = makeUnique<CSessionLockProtocol>(&ext_session_lock_manager_v1_interface, 1, "SessionLock");
    PROTO::ime                 = makeUnique<CInputMethodV2Protocol>(&zwp_input_method_manager_v2_interface, 1, "IMEv2");
    PROTO::virtualKeyboard     = makeUnique<CVirtualKeyboardProtocol>(&zwp_virtual_keyboard_manager_v1_interface, 1, "VirtualKeyboard");
    PROTO::virtualPointer      = makeUnique<CVirtualPointerProtocol>(&zwlr_virtual_pointer_manager_v1_interface, 2, "VirtualPointer");
    PROTO::outputManagement    = makeUnique<COutputManagementProtocol>(&zwlr_output_manager_v1_interface, 4, "OutputManagement");
    PROTO::serverDecorationKDE = makeUnique<CServerDecorationKDEProtocol>(&org_kde_kwin_server_decoration_manager_interface, 1, "ServerDecorationKDE");
    PROTO::focusGrab           = makeUnique<CFocusGrabProtocol>(&hyprland_focus_grab_manager_v1_interface, 1, "FocusGrab");
    PROTO::tablet              = makeUnique<CTabletV2Protocol>(&zwp_tablet_manager_v2_interface, 1, "TabletV2");
    PROTO::layerShell          = makeUnique<CLayerShellProtocol>(&zwlr_layer_shell_v1_interface, 5, "LayerShell");
    PROTO::presentation        = makeUnique<CPresentationProtocol>(&wp_presentation_interface, 1, "Presentation");
    PROTO::xdgShell            = makeUnique<CXDGShellProtocol>(&xdg_wm_base_interface, 6, "XDGShell");
    PROTO::dataWlr             = makeUnique<CDataDeviceWLRProtocol>(&zwlr_data_control_manager_v1_interface, 2, "DataDeviceWlr");
    PROTO::primarySelection    = makeUnique<CPrimarySelectionProtocol>(&zwp_primary_selection_device_manager_v1_interface, 1, "PrimarySelection");
    PROTO::xwaylandShell       = makeUnique<CXWaylandShellProtocol>(&xwayland_shell_v1_interface, 1, "XWaylandShell");
    PROTO::screencopy          = makeUnique<CScreencopyProtocol>(&zwlr_screencopy_manager_v1_interface, 3, "Screencopy");
    PROTO::toplevelExport      = makeUnique<CToplevelExportProtocol>(&hyprland_toplevel_export_manager_v1_interface, 2, "ToplevelExport");
    PROTO::globalShortcuts     = makeUnique<CGlobalShortcutsProtocol>(&hyprland_global_shortcuts_manager_v1_interface, 1, "GlobalShortcuts");
    PROTO::xdgDialog           = makeUnique<CXDGDialogProtocol>(&xdg_wm_dialog_v1_interface, 1, "XDGDialog");
    PROTO::singlePixel         = makeUnique<CSinglePixelProtocol>(&wp_single_pixel_buffer_manager_v1_interface, 1, "SinglePixel");
    PROTO::securityContext     = makeUnique<CSecurityContextProtocol>(&wp_security_context_manager_v1_interface, 1, "SecurityContext");
    PROTO::ctm                 = makeUnique<CHyprlandCTMControlProtocol>(&hyprland_ctm_control_manager_v1_interface, 2, "CTMControl");
    PROTO::hyprlandSurface     = makeUnique<CHyprlandSurfaceProtocol>(&hyprland_surface_manager_v1_interface, 2, "HyprlandSurface");
    PROTO::contentType         = makeUnique<CContentTypeProtocol>(&wp_content_type_manager_v1_interface, 1, "ContentType");
    PROTO::xdgTag              = makeUnique<CXDGToplevelTagProtocol>(&xdg_toplevel_tag_manager_v1_interface, 1, "XDGTag");

    if (*PENABLECM)
        PROTO::colorManagement = makeUnique<CColorManagementProtocol>(&wp_color_manager_v1_interface, 1, "ColorManagement", *PDEBUGCM);

    if (*PENABLEXXCM && *PENABLECM) {
        PROTO::xxColorManagement   = makeUnique<CXXColorManagementProtocol>(&xx_color_manager_v4_interface, 1, "XXColorManagement");
        PROTO::frogColorManagement = makeUnique<CFrogColorManagementProtocol>(&frog_color_management_factory_v1_interface, 1, "FrogColorManagement");
    }

    // ! please read the top of this file before adding another protocol

    for (auto const& b : g_pCompositor->m_aqBackend->getImplementations()) {
        if (b->type() != Aquamarine::AQ_BACKEND_DRM)
            continue;

        PROTO::lease = makeUnique<CDRMLeaseProtocol>(&wp_drm_lease_device_v1_interface, 1, "DRMLease");
        if (*PENABLEEXPLICIT)
            PROTO::sync = makeUnique<CDRMSyncobjProtocol>(&wp_linux_drm_syncobj_manager_v1_interface, 1, "DRMSyncobj");
        break;
    }

    if (g_pHyprOpenGL->getDRMFormats().size() > 0) {
        PROTO::mesaDRM  = makeUnique<CMesaDRMProtocol>(&wl_drm_interface, 2, "MesaDRM");
        PROTO::linuxDma = makeUnique<CLinuxDMABufV1Protocol>(&zwp_linux_dmabuf_v1_interface, 5, "LinuxDMABUF");
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
    PROTO::contentType.reset();
    PROTO::colorManagement.reset();
    PROTO::xxColorManagement.reset();
    PROTO::frogColorManagement.reset();
    PROTO::xdgTag.reset();

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
		PROTO::xdgTag->getGlobal(),
        PROTO::sync     ? PROTO::sync->getGlobal()      : nullptr,
        PROTO::mesaDRM  ? PROTO::mesaDRM->getGlobal()   : nullptr,
        PROTO::linuxDma ? PROTO::linuxDma->getGlobal()  : nullptr,
    };
    // clang-format on

    return std::find(ALLOWED_WHITELIST.begin(), ALLOWED_WHITELIST.end(), global) == ALLOWED_WHITELIST.end();
}
