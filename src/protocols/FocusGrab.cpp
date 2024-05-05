#include "FocusGrab.hpp"
#include "Compositor.hpp"
#include <hyprland-focus-grab-v1.hpp>
#include <managers/input/InputManager.hpp>
#include <cstdint>
#include <memory>
#include <wayland-server.h>

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
    static_cast<CFocusGrab*>(grab->data)->finish(true);
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
    resource->setAddSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { addSurface(wlr_surface_from_resource(surface)); });
    resource->setRemoveSurface([this](CHyprlandFocusGrabV1* pMgr, wl_resource* surface) { removeSurface(wlr_surface_from_resource(surface)); });
    resource->setCommit([this](CHyprlandFocusGrabV1* pMgr) { commit(); });
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
            &g_pCompositor->m_sSeat.seat->events.pointer_grab_begin, [this](void*, void*) { finish(true); }, this, "CFocusGrab");

        hyprListener_keyboardGrabStarted.initCallback(
            &g_pCompositor->m_sSeat.seat->events.keyboard_grab_begin, [this](void*, void*) { finish(true); }, this, "CFocusGrab");

        hyprListener_touchGrabStarted.initCallback(
            &g_pCompositor->m_sSeat.seat->events.touch_grab_begin, [this](void*, void*) { finish(true); }, this, "CFocusGrab");
    }

    // Ensure new surfaces are focused if under the mouse when comitted.
    g_pInputManager->simulateMouseMovement();
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
        if (g_pCompositor->m_sSeat.seat->pointer_state.grab == &m_sPointerGrab) {
            wlr_seat_pointer_end_grab(g_pCompositor->m_sSeat.seat);
            hadGrab = true;
        }

        if (g_pCompositor->m_sSeat.seat->keyboard_state.grab == &m_sKeyboardGrab) {
            wlr_seat_keyboard_end_grab(g_pCompositor->m_sSeat.seat);
            hadGrab = true;
        }

        if (g_pCompositor->m_sSeat.seat->touch_state.grab == &m_sTouchGrab) {
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
    if (iter == m_mSurfaces.end())
        m_mSurfaces.emplace(surface, std::make_unique<CFocusGrabSurfaceState>(this, surface));
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

void CFocusGrab::refocusKeyboard() {
    auto keyboardSurface = g_pCompositor->m_sSeat.seat->keyboard_state.focused_surface;
    if (keyboardSurface != nullptr && isSurfaceComitted(keyboardSurface))
        return;

    wlr_surface* surface = nullptr;
    for (auto& [surf, state] : m_mSurfaces) {
        if (state->state == CFocusGrabSurfaceState::Comitted) {
            surface = surf;
            break;
        }
    }

    if (surface)
        g_pCompositor->focusSurface(surface);
    else
        Debug::log(ERR, "CFocusGrab::refocusKeyboard called with no committed surfaces. This should never happen.");
}

void CFocusGrab::commit() {
    auto surfacesChanged = false;
    auto anyComitted     = false;
    for (auto iter = m_mSurfaces.begin(); iter != m_mSurfaces.end();) {
        switch (iter->second->state) {
            case CFocusGrabSurfaceState::PendingRemoval:
                iter            = m_mSurfaces.erase(iter);
                surfacesChanged = true;
                continue;
            case CFocusGrabSurfaceState::PendingAddition:
                iter->second->state = CFocusGrabSurfaceState::Comitted;
                surfacesChanged     = true;
                anyComitted         = true;
                break;
            case CFocusGrabSurfaceState::Comitted: anyComitted = true; break;
        }

        iter++;
    }

    if (surfacesChanged) {
        if (anyComitted)
            start();
        else
            finish(true);
    }
}

CFocusGrabProtocol::CFocusGrabProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CFocusGrabProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CHyprlandFocusGrabManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CHyprlandFocusGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CHyprlandFocusGrabManagerV1* p) { onManagerResourceDestroy(p->resource()); });
    RESOURCE->setCreateGrab([this](CHyprlandFocusGrabManagerV1* pMgr, uint32_t id) { onCreateGrab(pMgr, id); });
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
