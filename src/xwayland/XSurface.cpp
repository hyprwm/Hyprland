#include "XSurface.hpp"
#include "XWayland.hpp"
#include "../protocols/XWaylandShell.hpp"
#include "../protocols/core/Compositor.hpp"

#ifndef NO_XWAYLAND

#include <ranges>

CXWaylandSurface::CXWaylandSurface(uint32_t xID_, CBox geometry_, bool OR) : xID(xID_), geometry(geometry_), overrideRedirect(OR) {
    xcb_res_query_client_ids_cookie_t client_id_cookie = {0};
    if (g_pXWayland->pWM->xres) {
        xcb_res_client_id_spec_t spec = {.client = xID, .mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID};
        client_id_cookie              = xcb_res_query_client_ids(g_pXWayland->pWM->connection, 1, &spec);
    }

    uint32_t values[1];
    values[0] = XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(g_pXWayland->pWM->connection, xID, XCB_CW_EVENT_MASK, values);

    if (g_pXWayland->pWM->xres) {
        xcb_res_query_client_ids_reply_t* reply = xcb_res_query_client_ids_reply(g_pXWayland->pWM->connection, client_id_cookie, nullptr);
        if (!reply)
            return;

        uint32_t*                          ppid = nullptr;
        xcb_res_client_id_value_iterator_t iter = xcb_res_query_client_ids_ids_iterator(reply);
        while (iter.rem > 0) {
            if (iter.data->spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID && xcb_res_client_id_value_value_length(iter.data) > 0) {
                ppid = xcb_res_client_id_value_value(iter.data);
                break;
            }
            xcb_res_client_id_value_next(&iter);
        }
        if (!ppid) {
            free(reply);
            return;
        }
        pid = *ppid;
        free(reply);
    }

    events.resourceChange.registerStaticListener([this](void* data, std::any d) { ensureListeners(); }, nullptr);
}

void CXWaylandSurface::ensureListeners() {
    bool connected = listeners.destroySurface;

    if (connected && !surface) {
        listeners.destroySurface.reset();
        listeners.commitSurface.reset();
    } else if (!connected && surface) {
        listeners.destroySurface = surface->events.destroy.registerListener([this](std::any d) {
            if (mapped)
                unmap();

            surface.reset();
            listeners.destroySurface.reset();
            listeners.commitSurface.reset();
            events.resourceChange.emit();
        });

        listeners.commitSurface = surface->events.commit.registerListener([this](std::any d) {
            if (surface->pending.texture && !mapped) {
                map();
                return;
            }

            if (!surface->pending.texture && mapped) {
                unmap();
                return;
            }

            events.commit.emit();
        });
    }

    if (resource) {
        listeners.destroyResource = resource->events.destroy.registerListener([this](std::any d) {
            unmap();
            surface.reset();
            events.resourceChange.emit();
        });
    }
}

void CXWaylandSurface::map() {
    if (mapped)
        return;

    ASSERT(surface);

    g_pXWayland->pWM->mappedSurfaces.emplace_back(self);
    g_pXWayland->pWM->mappedSurfacesStacking.emplace_back(self);

    mapped = true;
    surface->map();

    Debug::log(LOG, "XWayland surface {:x} mapping", (uintptr_t)this);

    events.map.emit();

    g_pXWayland->pWM->updateClientList();
}

void CXWaylandSurface::unmap() {
    if (!mapped)
        return;

    ASSERT(surface);

    std::erase(g_pXWayland->pWM->mappedSurfaces, self);
    std::erase(g_pXWayland->pWM->mappedSurfacesStacking, self);

    mapped = false;
    events.unmap.emit();
    surface->unmap();

    Debug::log(LOG, "XWayland surface {:x} unmapping", (uintptr_t)this);

    g_pXWayland->pWM->updateClientList();
}

void CXWaylandSurface::considerMap() {
    if (mapped)
        return;

    if (!surface) {
        Debug::log(LOG, "XWayland surface: considerMap, nope, no surface");
        return;
    }

    if (surface->pending.texture) {
        Debug::log(LOG, "XWayland surface: considerMap, sure, we have a buffer");
        map();
        return;
    }

    Debug::log(LOG, "XWayland surface: considerMap, nope, we don't have a buffer");
}

bool CXWaylandSurface::wantsFocus() {
    if (atoms.empty())
        return true;

    const std::array<uint32_t, 10> search = {
        HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"],   HYPRATOMS["_NET_WM_WINDOW_TYPE_DND"],          HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"],    HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"], HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"],  HYPRATOMS["_NET_WM_WINDOW_TYPE_DESKTOP"],      HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"],
    };

    for (auto const& searched : search) {
        for (auto const& a : atoms) {
            if (a == searched)
                return false;
        }
    }

    return true;
}

void CXWaylandSurface::configure(const CBox& box) {
    Vector2D oldSize = geometry.size();

    geometry = box;

    uint32_t mask     = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH;
    uint32_t values[] = {box.x, box.y, box.width, box.height, 0};
    xcb_configure_window(g_pXWayland->pWM->connection, xID, mask, values);

    if (geometry.width == box.width && geometry.height == box.height) {
        // ICCCM requires a synthetic event when window size is not changed
        xcb_configure_notify_event_t e;
        e.response_type     = XCB_CONFIGURE_NOTIFY;
        e.event             = xID;
        e.window            = xID;
        e.x                 = box.x;
        e.y                 = box.y;
        e.width             = box.width;
        e.height            = box.height;
        e.border_width      = 0;
        e.above_sibling     = XCB_NONE;
        e.override_redirect = overrideRedirect;
        xcb_send_event(g_pXWayland->pWM->connection, false, xID, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (const char*)&e);
    }

    g_pXWayland->pWM->updateClientList();

    xcb_flush(g_pXWayland->pWM->connection);
}

void CXWaylandSurface::activate(bool activate) {
    if (overrideRedirect && !activate)
        return;
    g_pXWayland->pWM->activateSurface(self.lock(), activate);
}

void CXWaylandSurface::setFullscreen(bool fs) {
    fullscreen = fs;
    g_pXWayland->pWM->sendState(self.lock());
}

void CXWaylandSurface::setMinimized(bool mz) {
    minimized = mz;
    g_pXWayland->pWM->sendState(self.lock());
}

void CXWaylandSurface::restackToTop() {
    uint32_t values[1] = {XCB_STACK_MODE_ABOVE};

    xcb_configure_window(g_pXWayland->pWM->connection, xID, XCB_CONFIG_WINDOW_STACK_MODE, values);

    auto& stack = g_pXWayland->pWM->mappedSurfacesStacking;
    auto  it    = std::find(stack.begin(), stack.end(), self);

    if (it != stack.end())
        std::rotate(it, it + 1, stack.end());

    g_pXWayland->pWM->updateClientList();

    xcb_flush(g_pXWayland->pWM->connection);
}

void CXWaylandSurface::close() {
    xcb_client_message_data_t msg = {};
    msg.data32[0]                 = HYPRATOMS["WM_DELETE_WINDOW"];
    msg.data32[1]                 = XCB_CURRENT_TIME;
    g_pXWayland->pWM->sendWMMessage(self.lock(), &msg, XCB_EVENT_MASK_NO_EVENT);
}

void CXWaylandSurface::setWithdrawn(bool withdrawn_) {
    withdrawn                   = withdrawn_;
    std::vector<uint32_t> props = {XCB_ICCCM_WM_STATE_NORMAL, XCB_WINDOW_NONE};

    if (withdrawn)
        props[0] = XCB_ICCCM_WM_STATE_WITHDRAWN;
    else if (minimized)
        props[0] = XCB_ICCCM_WM_STATE_ICONIC;
    else
        props[0] = XCB_ICCCM_WM_STATE_NORMAL;

    xcb_change_property(g_pXWayland->pWM->connection, XCB_PROP_MODE_REPLACE, xID, HYPRATOMS["WM_STATE"], HYPRATOMS["WM_STATE"], 32, props.size(), props.data());
}

void CXWaylandSurface::ping() {
    xcb_client_message_event_t event = {.response_type = XCB_CLIENT_MESSAGE,
                                        .format        = 32,
                                        .window        = xID,
                                        .type          = HYPRATOMS["_NET_WM_PING"],
                                        .data          = {.data32 = {XCB_CURRENT_TIME, HYPRATOMS["_NET_WM_PING"], xID}}};

    xcb_send_event(g_pXWayland->pWM->connection, 0, xID, XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
    xcb_flush(g_pXWayland->pWM->connection);
}

#else

CXWaylandSurface::CXWaylandSurface(uint32_t xID_, CBox geometry_, bool OR) : xID(xID_), geometry(geometry_), overrideRedirect(OR) {
    ;
}

void CXWaylandSurface::ensureListeners() {
    ;
}

void CXWaylandSurface::map() {
    ;
}

void CXWaylandSurface::unmap() {
    ;
}

bool CXWaylandSurface::wantsFocus() {
    return false;
}

void CXWaylandSurface::configure(const CBox& box) {
    ;
}

void CXWaylandSurface::activate(bool activate) {
    ;
}

void CXWaylandSurface::setFullscreen(bool fs) {
    ;
}

void CXWaylandSurface::setMinimized(bool mz) {
    ;
}

void CXWaylandSurface::restackToTop() {
    ;
}

void CXWaylandSurface::close() {
    ;
}

void CXWaylandSurface::considerMap() {
    ;
}

void CXWaylandSurface::setWithdrawn(bool withdrawn) {
    ;
}

void CXWaylandSurface::ping() {
    ;
}

#endif
