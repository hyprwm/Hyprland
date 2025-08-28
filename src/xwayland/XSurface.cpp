#include "XSurface.hpp"
#include "XWayland.hpp"
#include "../protocols/XWaylandShell.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../managers/ANRManager.hpp"
#include "../helpers/time/Time.hpp"

#ifndef NO_XWAYLAND

#include <ranges>

CXWaylandSurface::CXWaylandSurface(uint32_t xID_, CBox geometry_, bool OR) : m_xID(xID_), m_geometry(geometry_), m_overrideRedirect(OR) {
    xcb_res_query_client_ids_cookie_t client_id_cookie = {0};
    if (g_pXWayland->m_wm->m_xres) {
        xcb_res_client_id_spec_t spec = {.client = m_xID, .mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID};
        client_id_cookie              = xcb_res_query_client_ids(g_pXWayland->m_wm->getConnection(), 1, &spec);
    }

    uint32_t values[1];
    values[0] = XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(g_pXWayland->m_wm->getConnection(), m_xID, XCB_CW_EVENT_MASK, values);

    if (g_pXWayland->m_wm->m_xres) {
        xcb_res_query_client_ids_reply_t* reply = xcb_res_query_client_ids_reply(g_pXWayland->m_wm->getConnection(), client_id_cookie, nullptr);
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
        m_pid = *ppid;
        free(reply);
    }

    m_events.resourceChange.listenStatic([this] { ensureListeners(); });
}

void CXWaylandSurface::ensureListeners() {
    bool connected = m_listeners.destroySurface;

    if (connected && !m_surface) {
        m_listeners.destroySurface.reset();
        m_listeners.commitSurface.reset();
    } else if (!connected && m_surface) {
        m_listeners.destroySurface = m_surface->m_events.destroy.listen([this] {
            if (m_mapped)
                unmap();

            m_surface.reset();
            m_listeners.destroySurface.reset();
            m_listeners.commitSurface.reset();
            m_events.resourceChange.emit();
        });

        m_listeners.commitSurface = m_surface->m_events.commit.listen([this] {
            if (m_surface->m_current.texture && !m_mapped) {
                map();
                return;
            }

            if (!m_surface->m_current.texture && m_mapped) {
                unmap();
                return;
            }

            m_events.commit.emit();
        });
    }

    if (m_resource) {
        m_listeners.destroyResource = m_resource->events.destroy.listen([this] {
            unmap();
            m_surface.reset();
            m_events.resourceChange.emit();
        });
    }
}

void CXWaylandSurface::map() {
    if (m_mapped)
        return;

    ASSERT(m_surface);

    g_pXWayland->m_wm->m_mappedSurfaces.emplace_back(m_self);
    g_pXWayland->m_wm->m_mappedSurfacesStacking.emplace_back(m_self);

    m_mapped = true;
    m_surface->map();

    Debug::log(LOG, "XWayland surface {:x} mapping", rc<uintptr_t>(this));

    m_events.map.emit();

    g_pXWayland->m_wm->updateClientList();
}

void CXWaylandSurface::unmap() {
    if (!m_mapped)
        return;

    ASSERT(m_surface);

    std::erase(g_pXWayland->m_wm->m_mappedSurfaces, m_self);
    std::erase(g_pXWayland->m_wm->m_mappedSurfacesStacking, m_self);

    m_mapped = false;
    m_events.unmap.emit();
    m_surface->unmap();

    Debug::log(LOG, "XWayland surface {:x} unmapping", rc<uintptr_t>(this));

    g_pXWayland->m_wm->updateClientList();
}

void CXWaylandSurface::considerMap() {
    if (m_mapped)
        return;

    if (!m_surface) {
        Debug::log(LOG, "XWayland surface: considerMap, nope, no surface");
        return;
    }

    if (m_surface->m_current.texture) {
        Debug::log(LOG, "XWayland surface: considerMap, sure, we have a buffer");
        map();
        return;
    }

    Debug::log(LOG, "XWayland surface: considerMap, nope, we don't have a buffer");
}

bool CXWaylandSurface::wantsFocus() {
    if (m_atoms.empty())
        return true;

    const std::array<uint32_t, 10> search = {
        HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"],   HYPRATOMS["_NET_WM_WINDOW_TYPE_DND"],          HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"],    HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"], HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"],  HYPRATOMS["_NET_WM_WINDOW_TYPE_DESKTOP"],      HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"],
    };

    for (auto const& searched : search) {
        for (auto const& a : m_atoms) {
            if (a == searched)
                return false;
        }
    }

    return true;
}

void CXWaylandSurface::configure(const CBox& box) {
    Vector2D oldSize = m_geometry.size();

    m_geometry = box;

    uint32_t mask     = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH;
    uint32_t values[] = {box.x, box.y, box.width, box.height, 0};
    xcb_configure_window(g_pXWayland->m_wm->getConnection(), m_xID, mask, values);

    if (m_geometry.width == box.width && m_geometry.height == box.height) {
        // ICCCM requires a synthetic event when window size is not changed
        xcb_configure_notify_event_t e;
        e.response_type     = XCB_CONFIGURE_NOTIFY;
        e.event             = m_xID;
        e.window            = m_xID;
        e.x                 = box.x;
        e.y                 = box.y;
        e.width             = box.width;
        e.height            = box.height;
        e.border_width      = 0;
        e.above_sibling     = XCB_NONE;
        e.override_redirect = m_overrideRedirect;
        xcb_send_event(g_pXWayland->m_wm->getConnection(), false, m_xID, XCB_EVENT_MASK_STRUCTURE_NOTIFY, rc<const char*>(&e));
    }

    g_pXWayland->m_wm->updateClientList();

    xcb_flush(g_pXWayland->m_wm->getConnection());
}

void CXWaylandSurface::activate(bool activate) {
    if (m_overrideRedirect && !activate)
        return;
    setWithdrawn(false);
    g_pXWayland->m_wm->activateSurface(m_self.lock(), activate);
}

void CXWaylandSurface::setFullscreen(bool fs) {
    m_fullscreen = fs;
    g_pXWayland->m_wm->sendState(m_self.lock());
}

void CXWaylandSurface::setMinimized(bool mz) {
    m_minimized = mz;
    g_pXWayland->m_wm->sendState(m_self.lock());
}

void CXWaylandSurface::restackToTop() {
    uint32_t values[1] = {XCB_STACK_MODE_ABOVE};

    xcb_configure_window(g_pXWayland->m_wm->getConnection(), m_xID, XCB_CONFIG_WINDOW_STACK_MODE, values);

    auto& stack = g_pXWayland->m_wm->m_mappedSurfacesStacking;
    auto  it    = std::ranges::find(stack, m_self);

    if (it != stack.end())
        std::rotate(it, it + 1, stack.end());

    g_pXWayland->m_wm->updateClientList();

    xcb_flush(g_pXWayland->m_wm->getConnection());
}

void CXWaylandSurface::close() {
    xcb_client_message_data_t msg = {};
    msg.data32[0]                 = HYPRATOMS["WM_DELETE_WINDOW"];
    msg.data32[1]                 = XCB_CURRENT_TIME;
    g_pXWayland->m_wm->sendWMMessage(m_self.lock(), &msg, XCB_EVENT_MASK_NO_EVENT);
}

void CXWaylandSurface::setWithdrawn(bool withdrawn_) {
    m_withdrawn                 = withdrawn_;
    std::vector<uint32_t> props = {XCB_ICCCM_WM_STATE_NORMAL, XCB_WINDOW_NONE};

    if (m_withdrawn)
        props[0] = XCB_ICCCM_WM_STATE_WITHDRAWN;
    else if (m_minimized)
        props[0] = XCB_ICCCM_WM_STATE_ICONIC;
    else
        props[0] = XCB_ICCCM_WM_STATE_NORMAL;

    xcb_change_property(g_pXWayland->m_wm->getConnection(), XCB_PROP_MODE_REPLACE, m_xID, HYPRATOMS["WM_STATE"], HYPRATOMS["WM_STATE"], 32, props.size(), props.data());
}

void CXWaylandSurface::ping() {
    bool supportsPing = std::ranges::find(m_protocols, HYPRATOMS["_NET_WM_PING"]) != m_protocols.end();

    if (!supportsPing) {
        Debug::log(TRACE, "CXWaylandSurface: XID {} does not support ping, just sending an instant reply", m_xID);
        g_pANRManager->onResponse(m_self.lock());
        return;
    }

    xcb_client_message_data_t msg = {};
    msg.data32[0]                 = HYPRATOMS["_NET_WM_PING"];
    msg.data32[1]                 = Time::millis(Time::steadyNow());
    msg.data32[2]                 = m_xID;

    m_lastPingSeq = msg.data32[1];

    g_pXWayland->m_wm->sendWMMessage(m_self.lock(), &msg, XCB_EVENT_MASK_PROPERTY_CHANGE);
}

#else

CXWaylandSurface::CXWaylandSurface(uint32_t xID_, CBox geometry_, bool OR) : m_xID(xID_), m_geometry(geometry_), m_overrideRedirect(OR) {
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
