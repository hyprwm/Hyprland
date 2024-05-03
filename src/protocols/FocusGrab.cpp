#include "FocusGrab.hpp"
#include "Compositor.hpp"
#include "desktop/LayerSurface.hpp"
#include "desktop/WLSurface.hpp"
#include "desktop/Workspace.hpp"
#include "render/decorations/CHyprGroupBarDecoration.hpp"
#include "wlr-layer-shell-unstable-v1-protocol.h"
#include <functional>
#include <hyprland-focus-grab-v1.hpp>
#include <managers/input/InputManager.hpp>
#include <cstdint>
#include <memory>
#include <wayland-server.h>
#include <wayland-util.h>

static void focus_grab_pointer_enter(wlr_seat_pointer_grab* grab, wlr_surface* surface, double sx, double sy) {
    if (static_cast<CFocusGrab*>(grab->data)->isSurfaceComitted(surface))
        wlr_seat_pointer_enter(grab->seat, surface, sx, sy);
    else
        wlr_seat_pointer_clear_focus(grab->seat);
}

static void focus_grab_pointer_clear_focus(wlr_seat_pointer_grab* grab) {
    wlr_seat_pointer_clear_focus(grab->seat);
}

static void focus_grab_pointer_motion(wlr_seat_pointer_grab* grab, uint32_t time, double sx, double sy) {
    wlr_seat_pointer_send_motion(grab->seat, time, sx, sy);
}

static uint32_t focus_grab_pointer_button(wlr_seat_pointer_grab* grab, uint32_t time, uint32_t button, wl_pointer_button_state state) {
    uint32_t serial = wlr_seat_pointer_send_button(grab->seat, time, button, state);

    if (serial)
        return serial;
    else {
        static_cast<CFocusGrab*>(grab->data)->finish(true);
        return 0;
    }
}

static void focus_grab_pointer_axis(wlr_seat_pointer_grab* grab, uint32_t time, enum wl_pointer_axis orientation, double value, int32_t value_discrete,
                                    enum wl_pointer_axis_source source, enum wl_pointer_axis_relative_direction relative_direction) {
    wlr_seat_pointer_send_axis(grab->seat, time, orientation, value, value_discrete, source, relative_direction);
}

static void focus_grab_pointer_frame(wlr_seat_pointer_grab* grab) {
    wlr_seat_pointer_send_frame(grab->seat);
}

static void focus_grab_pointer_cancel(wlr_seat_pointer_grab* grab) {
    static_cast<CFocusGrab*>(grab->data)->finish(true);
}

static const wlr_pointer_grab_interface focus_grab_pointer_impl = {
    .enter       = focus_grab_pointer_enter,
    .clear_focus = focus_grab_pointer_clear_focus,
    .motion      = focus_grab_pointer_motion,
    .button      = focus_grab_pointer_button,
    .axis        = focus_grab_pointer_axis,
    .frame       = focus_grab_pointer_frame,
    .cancel      = focus_grab_pointer_cancel,
};

static void focus_grab_keyboard_enter(wlr_seat_keyboard_grab* grab, wlr_surface* surface, const uint32_t keycodes[], size_t num_keycodes, const wlr_keyboard_modifiers* modifiers) {
    if (static_cast<CFocusGrab*>(grab->data)->isSurfaceComitted(surface))
        wlr_seat_keyboard_enter(grab->seat, surface, keycodes, num_keycodes, modifiers);

    // otherwise the last grabbed window should retain keyboard focus.
}

static void focus_grab_keyboard_clear_focus(wlr_seat_keyboard_grab* grab) {
    // focus will be cleared from non whitelisted surfaces if no
    // whitelisted surfaces will accept it, so this should not reset the grab.
}

static void focus_grab_keyboard_key(wlr_seat_keyboard_grab* grab, uint32_t time, uint32_t key, uint32_t state) {
    wlr_seat_keyboard_send_key(grab->seat, time, key, state);
}

static void focus_grab_keyboard_modifiers(wlr_seat_keyboard_grab* grab, const wlr_keyboard_modifiers* modifiers) {
    wlr_seat_keyboard_send_modifiers(grab->seat, modifiers);
}

static void focus_grab_keyboard_cancel(wlr_seat_keyboard_grab* grab) {
    static_cast<CFocusGrab*>(grab->data)->finish(true);
}

static const wlr_keyboard_grab_interface focus_grab_keyboard_impl = {
    .enter       = focus_grab_keyboard_enter,
    .clear_focus = focus_grab_keyboard_clear_focus,
    .key         = focus_grab_keyboard_key,
    .modifiers   = focus_grab_keyboard_modifiers,
    .cancel      = focus_grab_keyboard_cancel,
};

static uint32_t focus_grab_touch_down(wlr_seat_touch_grab* grab, uint32_t time, wlr_touch_point* point) {
    if (!static_cast<CFocusGrab*>(grab->data)->isSurfaceComitted(point->surface))
        return 0;

    return wlr_seat_touch_send_down(grab->seat, point->surface, time, point->touch_id, point->sx, point->sy);
}

static void focus_grab_touch_up(wlr_seat_touch_grab* grab, uint32_t time, wlr_touch_point* point) {
    wlr_seat_touch_send_up(grab->seat, time, point->touch_id);
}

static void focus_grab_touch_motion(wlr_seat_touch_grab* grab, uint32_t time, wlr_touch_point* point) {
    wlr_seat_touch_send_motion(grab->seat, time, point->touch_id, point->sx, point->sy);
}

static void focus_grab_touch_enter(wlr_seat_touch_grab* grab, uint32_t time, wlr_touch_point* point) {}

static void focus_grab_touch_frame(wlr_seat_touch_grab* grab) {
    wlr_seat_touch_send_frame(grab->seat);
}

static void focus_grab_touch_cancel(wlr_seat_touch_grab* grab) {
    static_cast<CFocusGrab*>(grab->data)->finish(true);
}

static const wlr_touch_grab_interface focus_grab_touch_impl = {
    .down   = focus_grab_touch_down,
    .up     = focus_grab_touch_up,
    .motion = focus_grab_touch_motion,
    .enter  = focus_grab_touch_enter,
    .frame  = focus_grab_touch_frame,
    .cancel = focus_grab_touch_cancel,
};

CFocusGrabSurfaceState::CFocusGrabSurfaceState(CFocusGrab* grab, wlr_surface* surface) {
    hyprListener_surfaceDestroy.initCallback(
        &surface->events.destroy, [=](void*, void*) { grab->eraseSurface(surface); }, this, "CFocusGrab");
}

CFocusGrabSurfaceState::~CFocusGrabSurfaceState() {
    hyprListener_surfaceDestroy.removeCallback();
}

CFocusGrab::CFocusGrab(SP<CHyprlandFocusGrabV1> resource_) : resource(resource_) {
    if (!resource->resource())
        return;

    m_sPointerGrab.interface = &focus_grab_pointer_impl;
    m_sPointerGrab.data      = this;

    m_sKeyboardGrab.interface = &focus_grab_keyboard_impl;
    m_sKeyboardGrab.data      = this;

    m_sTouchGrab.interface = &focus_grab_touch_impl;
    m_sTouchGrab.data      = this;

    resource->setDestroy([this](CHyprlandFocusGrabV1* pMgr) { PROTO::focusGrab->destroyGrab(this); });
    resource->setOnDestroy([this](CHyprlandFocusGrabV1* pMgr) { PROTO::focusGrab->destroyGrab(this); });
    resource->setAddSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { this->addSurface(wlr_surface_from_resource(surface)); });
    resource->setRemoveSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { this->removeSurface(wlr_surface_from_resource(surface)); });
    resource->setCommit([this](CHyprlandFocusGrabV1* pMgr) { this->commit(); });
}

CFocusGrab::~CFocusGrab() {
    finish(false);
}

bool CFocusGrab::good() {
    return resource->resource();
}

bool CFocusGrab::isSurfaceComitted(wlr_surface* surface) {
    auto iter = m_mSurfaces.find(surface);
    if (iter == m_mSurfaces.end())
        return false;

    return iter->second->state == CFocusGrabSurfaceState::Comitted;
}

void CFocusGrab::start() {
    if (!m_bGrabActive) {
        wlr_seat_pointer_start_grab(g_pCompositor->m_sSeat.seat, &m_sPointerGrab);
        wlr_seat_keyboard_start_grab(g_pCompositor->m_sSeat.seat, &m_sKeyboardGrab);
        wlr_seat_touch_start_grab(g_pCompositor->m_sSeat.seat, &m_sTouchGrab);
        m_bGrabActive = true;

        // Ensure the grab ends if another grab begins, including from xdg_popup::grab.

        hyprListener_pointerGrabStarted.initCallback(
            &g_pCompositor->m_sSeat.seat->events.pointer_grab_begin, [this](void*, void*) { this->finish(true); }, this, "CFocusGrab");

        hyprListener_keyboardGrabStarted.initCallback(
            &g_pCompositor->m_sSeat.seat->events.keyboard_grab_begin, [this](void*, void*) { this->finish(true); }, this, "CFocusGrab");

        hyprListener_touchGrabStarted.initCallback(
            &g_pCompositor->m_sSeat.seat->events.touch_grab_begin, [this](void*, void*) { this->finish(true); }, this, "CFocusGrab");
    }

    // Ensure new surfaces are focused if under the mouse when comitted.
    g_pInputManager->refocus();
    refocusKeyboard();
}

void CFocusGrab::finish(bool sendCleared) {
    if (m_bGrabActive) {
        m_bGrabActive = false;
        hyprListener_pointerGrabStarted.removeCallback();
        hyprListener_keyboardGrabStarted.removeCallback();
        hyprListener_touchGrabStarted.removeCallback();

        // Only clear grabs that belong to this focus grab. When superseded by another grab
        // or xdg_popup grab we might not own the current grab.

        bool hadGrab = false;
        if (g_pCompositor->m_sSeat.seat->pointer_state.grab == &this->m_sPointerGrab) {
            wlr_seat_pointer_end_grab(g_pCompositor->m_sSeat.seat);
            hadGrab = true;
        }

        if (g_pCompositor->m_sSeat.seat->keyboard_state.grab == &this->m_sKeyboardGrab) {
            wlr_seat_keyboard_end_grab(g_pCompositor->m_sSeat.seat);
            hadGrab = true;
        }

        if (g_pCompositor->m_sSeat.seat->touch_state.grab == &this->m_sTouchGrab) {
            wlr_seat_touch_end_grab(g_pCompositor->m_sSeat.seat);
            hadGrab = true;
        }

        m_mSurfaces.clear();

        if (sendCleared)
            resource->sendCleared();

        // Ensure surfaces under the mouse when the grab ends get focus.
        if (hadGrab)
            g_pInputManager->refocus();
    }
}

void CFocusGrab::addSurface(wlr_surface* surface) {
    auto iter = m_mSurfaces.find(surface);
    if (iter == m_mSurfaces.end()) {
        m_mSurfaces.emplace(surface, std::make_unique<CFocusGrabSurfaceState>(this, surface));
    }
}

void CFocusGrab::removeSurface(wlr_surface* surface) {
    auto iter = m_mSurfaces.find(surface);
    if (iter != m_mSurfaces.end()) {
        if (iter->second->state == CFocusGrabSurfaceState::PendingAddition)
            m_mSurfaces.erase(iter);
        else
            iter->second->state = CFocusGrabSurfaceState::PendingRemoval;
    }
}

void CFocusGrab::eraseSurface(wlr_surface* surface) {
    removeSurface(surface);
    commit();
}

bool CFocusGrab::refocusKeyboardTestSurface(wlr_surface* surface) {
    if (!isSurfaceComitted(surface))
        return false;

    const auto KEYBOARD     = wlr_seat_get_keyboard(g_pCompositor->m_sSeat.seat);
    uint32_t   keycodes[32] = {0};

    wlr_seat_keyboard_enter(g_pCompositor->m_sSeat.seat, surface, keycodes, 0, &KEYBOARD->modifiers);
    return true;
}

bool CFocusGrab::refocusKeyboardTestPopupTree(UP<CPopup>& popup) {
    for (auto& popup : popup->m_vChildren) {
        if (refocusKeyboardTestPopupTree(popup))
            return true;
    }

    auto wlrPopup = popup->wlr();
    if (!wlrPopup)
        return false;

    return refocusKeyboardTestSurface(wlrPopup->base->surface);
}

void CFocusGrab::refocusKeyboard() {
    // If there is no kb focused surface or the current kb focused surface is not whitelisted, replace it.
    auto keyboardSurface = g_pCompositor->m_sSeat.seat->keyboard_state.focused_surface;
    if (keyboardSurface != nullptr && isSurfaceComitted(keyboardSurface))
        return;

    auto testLayers = [&](std::vector<PHLLS>& lsl, std::function<bool(PHLLS&)> f) {
        for (auto& ls : lsl | std::views::reverse) {
            if (ls->fadingOut || !ls->layerSurface || (ls->layerSurface && !ls->layerSurface->surface->mapped) || ls->alpha.value() == 0.f)
                continue;

            if (ls->layerSurface->current.keyboard_interactive == ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE)
                continue;

            if (f(ls))
                return true;
        }

        return false;
    };

    auto testWindow = [&](PHLWINDOW& window) {
        if (refocusKeyboardTestPopupTree(window->m_pPopupHead))
            return true;

        if (refocusKeyboardTestSurface(window->m_pWLSurface.wlr()))
            return true;

        return false;
    };

    auto testWindows = [&](std::function<bool(PHLWINDOW&)> f) {
        for (auto& window : g_pCompositor->m_vWindows | std::views::reverse) {
            if (!window->m_bIsMapped || window->isHidden() || window->m_sAdditionalConfigData.noFocus)
                continue;

            if (f(window) && refocusKeyboardTestPopupTree(window->m_pPopupHead))
                return true;
        }

        for (auto& window : g_pCompositor->m_vWindows | std::views::reverse) {
            if (!window->m_bIsMapped || window->isHidden() || window->m_sAdditionalConfigData.noFocus)
                continue;

            if (f(window) && refocusKeyboardTestSurface(window->m_pWLSurface.wlr()))
                return true;
        }

        return false;
    };

    auto testMonitor = [&](CMonitor* monitor) {
        // layer popups
        for (auto& lsl : monitor->m_aLayerSurfaceLayers | std::views::reverse) {
            auto ret = testLayers(lsl, [&](auto& layer) { return refocusKeyboardTestPopupTree(layer->popupHead); });

            if (ret)
                return true;
        }

        // overlay and top layers
        auto layerCallback = [&](auto& layer) { return refocusKeyboardTestSurface(layer->layerSurface->surface); };

        if (testLayers(monitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY], layerCallback))
            return true;
        if (testLayers(monitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP], layerCallback))
            return true;

        auto workspace = monitor->activeWorkspace;

        // fullscreen windows
        if (workspace->m_bHasFullscreenWindow && workspace->m_efFullscreenMode == FULLSCREEN_FULL) {
            auto window = g_pCompositor->getFullscreenWindowOnWorkspace(workspace->m_iID);
            if (window) {
                if (testWindow(window))
                    return true;
            }
        }

        // pinned windows
        if (testWindows([&](auto& window) { return window->m_bPinned; }))
            return true;

        // windows on the active special workspace
        if (monitor->activeSpecialWorkspace) {
            auto ret = testWindows([&](auto& window) { return window->m_pWorkspace == monitor->activeSpecialWorkspace; });

            if (ret)
                return true;
        }

        if (workspace->m_bHasFullscreenWindow) {
            // windows created over a fullscreen window
            auto ret = testWindows([&](auto& window) { return window->m_pWorkspace == workspace && window->m_bCreatedOverFullscreen; });

            if (ret)
                return true;

            // the fullscreen window
            auto window = g_pCompositor->getFullscreenWindowOnWorkspace(workspace->m_iID);

            if (testWindow(window))
                return true;
        }

        // floating windows
        bool ret = testWindows([&](auto& window) { return window->m_pWorkspace == workspace && window->m_bIsFloating; });

        if (ret)
            return true;

        // tiled windows
        ret = testWindows([&](auto& window) { return window->m_pWorkspace == workspace; });

        if (ret)
            return true;

        // bottom and background layers
        if (testLayers(monitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM], layerCallback))
            return true;
        if (testLayers(monitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND], layerCallback))
            return true;

        return false;
    };

    auto activeMonitor = g_pCompositor->getMonitorFromCursor();
    if (testMonitor(activeMonitor))
        return;

    for (auto& monitor : g_pCompositor->m_vMonitors) {
        if (monitor.get() == activeMonitor)
            continue;

        if (testMonitor(monitor.get()))
            return;
    }

    // no applicable surfaces found, at least remove focus from any non whitelisted surfaces.
    wlr_seat_keyboard_clear_focus(g_pCompositor->m_sSeat.seat);
}

void CFocusGrab::commit() {
    auto surfacesChanged = false;
    for (auto iter = m_mSurfaces.begin(); iter != m_mSurfaces.end();) {
        switch (iter->second->state) {
            case CFocusGrabSurfaceState::PendingRemoval:
                iter            = m_mSurfaces.erase(iter);
                surfacesChanged = true;
                continue;
            case CFocusGrabSurfaceState::PendingAddition:
                iter->second->state = CFocusGrabSurfaceState::Comitted;
                surfacesChanged     = true;
                break;
            default: break;
        }

        iter++;
    }

    if (surfacesChanged) {
        if (!m_mSurfaces.empty())
            start();
        else
            finish(false);
    }
}

CFocusGrabProtocol::CFocusGrabProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFocusGrabProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CHyprlandFocusGrabManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CHyprlandFocusGrabManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });
    RESOURCE->setCreateGrab([this](CHyprlandFocusGrabManagerV1* pMgr, uint32_t id) { this->onCreateGrab(pMgr, id); });
}

void CFocusGrabProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CFocusGrabProtocol::destroyGrab(CFocusGrab* grab) {
    std::erase_if(m_vGrabs, [&](const auto& other) { return other.get() == grab; });
}

void CFocusGrabProtocol::onCreateGrab(CHyprlandFocusGrabManagerV1* pMgr, uint32_t id) {
    m_vGrabs.push_back(std::make_unique<CFocusGrab>(std::make_shared<CHyprlandFocusGrabV1>(pMgr->client(), pMgr->version(), id)));
    const auto RESOURCE = m_vGrabs.back().get();

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vGrabs.pop_back();
    }
}
