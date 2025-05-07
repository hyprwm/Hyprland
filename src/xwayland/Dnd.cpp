#include "Dnd.hpp"
#ifndef NO_XWAYLAND
#include "XWM.hpp"
#include "XWayland.hpp"
#include "Server.hpp"
#endif
#include "../managers/XWaylandManager.hpp"
#include "../desktop/WLSurface.hpp"
#include "../protocols/core/Compositor.hpp"

using namespace Hyprutils::OS;

#define PROPERTY_FORMAT_32BIT 32
#define PROPERTY_LENGTH       1
#define PROPERTY_OFFSET       0

#ifndef NO_XWAYLAND
static xcb_atom_t dndActionToAtom(uint32_t actions) {
    if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
        return HYPRATOMS["XdndActionCopy"];
    else if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
        return HYPRATOMS["XdndActionMove"];
    else if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
        return HYPRATOMS["XdndActionAsk"];

    return XCB_ATOM_NONE;
}

void CX11DataDevice::sendDndEvent(xcb_window_t targetWindow, xcb_atom_t type, xcb_client_message_data_t& data) {
    xcb_client_message_event_t event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format        = 32,
        .sequence      = 0,
        .window        = targetWindow,
        .type          = type,
        .data          = data,
    };

    xcb_send_event(g_pXWayland->m_wm->m_connection, 0, targetWindow, XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
    xcb_flush(g_pXWayland->m_wm->m_connection);
}

xcb_window_t CX11DataDevice::getProxyWindow(xcb_window_t window) {
    xcb_window_t              targetWindow = window;
    xcb_get_property_cookie_t proxyCookie =
        xcb_get_property(g_pXWayland->m_wm->m_connection, PROPERTY_OFFSET, window, HYPRATOMS["XdndProxy"], XCB_ATOM_WINDOW, PROPERTY_OFFSET, PROPERTY_LENGTH);
    xcb_get_property_reply_t* proxyReply = xcb_get_property_reply(g_pXWayland->m_wm->m_connection, proxyCookie, nullptr);

    const auto                isValidPropertyReply = [](xcb_get_property_reply_t* reply) {
        return reply && reply->type == XCB_ATOM_WINDOW && reply->format == PROPERTY_FORMAT_32BIT && xcb_get_property_value_length(reply) == sizeof(xcb_window_t);
    };

    if (isValidPropertyReply(proxyReply)) {
        xcb_window_t              proxyWindow = *(xcb_window_t*)xcb_get_property_value(proxyReply);

        xcb_get_property_cookie_t proxyVerifyCookie =
            xcb_get_property(g_pXWayland->m_wm->m_connection, PROPERTY_OFFSET, proxyWindow, HYPRATOMS["XdndProxy"], XCB_ATOM_WINDOW, PROPERTY_OFFSET, PROPERTY_LENGTH);
        xcb_get_property_reply_t* proxyVerifyReply = xcb_get_property_reply(g_pXWayland->m_wm->m_connection, proxyVerifyCookie, nullptr);

        if (isValidPropertyReply(proxyVerifyReply)) {
            xcb_window_t verifyWindow = *(xcb_window_t*)xcb_get_property_value(proxyVerifyReply);
            if (verifyWindow == proxyWindow) {
                targetWindow = proxyWindow;
                Debug::log(LOG, "Using XdndProxy window {:x} for window {:x}", proxyWindow, window);
            }
        }
        free(proxyVerifyReply);
    }
    free(proxyReply);

    return targetWindow;
}
#endif

eDataSourceType CX11DataOffer::type() {
    return DATA_SOURCE_TYPE_X11;
}

SP<CWLDataOfferResource> CX11DataOffer::getWayland() {
    return nullptr;
}

SP<CX11DataOffer> CX11DataOffer::getX11() {
    return m_self.lock();
}

SP<IDataSource> CX11DataOffer::getSource() {
    return m_source.lock();
}

void CX11DataOffer::markDead() {
#ifndef NO_XWAYLAND
    std::erase(g_pXWayland->m_wm->m_dndDataOffers, m_self);
#endif
}

void CX11DataDevice::sendDataOffer(SP<IDataOffer> offer) {
    ; // no-op, I don't think this has an X equiv
}

void CX11DataDevice::sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<IDataOffer> offer) {
#ifndef NO_XWAYLAND
    auto XSURF = g_pXWayland->m_wm->windowForWayland(surf);

    if (!XSURF) {
        Debug::log(ERR, "CX11DataDevice::sendEnter: No xwayland surface for destination");
        return;
    }

    auto SOURCE = offer->getSource();

    if (!SOURCE) {
        Debug::log(ERR, "CX11DataDevice::sendEnter: No source");
        return;
    }

    std::vector<xcb_atom_t> targets;
    // reserve to avoid reallocations
    targets.reserve(SOURCE->mimes().size());
    for (auto const& m : SOURCE->mimes()) {
        targets.push_back(g_pXWayland->m_wm->mimeToAtom(m));
    }

    xcb_change_property(g_pXWayland->m_wm->m_connection, XCB_PROP_MODE_REPLACE, g_pXWayland->m_wm->m_dndSelection.window, HYPRATOMS["XdndTypeList"], XCB_ATOM_ATOM, 32,
                        targets.size(), targets.data());

    xcb_set_selection_owner(g_pXWayland->m_wm->m_connection, g_pXWayland->m_wm->m_dndSelection.window, HYPRATOMS["XdndSelection"], XCB_TIME_CURRENT_TIME);
    xcb_flush(g_pXWayland->m_wm->m_connection);

    xcb_window_t              targetWindow = getProxyWindow(XSURF->m_xID);

    xcb_client_message_data_t data = {{0}};
    data.data32[0]                 = g_pXWayland->m_wm->m_dndSelection.window;
    data.data32[1]                 = XDND_VERSION << 24;
    data.data32[1] |= 1;

    sendDndEvent(targetWindow, HYPRATOMS["XdndEnter"], data);

    m_lastSurface = XSURF;
    m_lastOffer   = offer;

    auto hlSurface = XSURF->m_surface.lock();
    if (!hlSurface) {
        Debug::log(ERR, "CX11DataDevice::sendEnter: Non desktop x surface?!");
        m_lastSurfaceCoords = {};
        return;
    }

    m_lastSurfaceCoords = g_pXWaylandManager->xwaylandToWaylandCoords(XSURF->m_geometry.pos());
#endif
}

void CX11DataDevice::cleanupState() {
    m_lastSurface.reset();
    m_lastOffer.reset();
    m_lastSurfaceCoords = {};
    m_lastTime          = 0;
}

void CX11DataDevice::sendLeave() {
#ifndef NO_XWAYLAND
    if (!m_lastSurface)
        return;

    xcb_window_t              targetWindow = getProxyWindow(m_lastSurface->m_xID);

    xcb_client_message_data_t data = {{0}};
    data.data32[0]                 = g_pXWayland->m_wm->m_dndSelection.window;

    sendDndEvent(targetWindow, HYPRATOMS["XdndLeave"], data);

    cleanupState();
#endif
}

void CX11DataDevice::sendMotion(uint32_t timeMs, const Vector2D& local) {
#ifndef NO_XWAYLAND
    if (!m_lastSurface || !m_lastOffer || !m_lastOffer->getSource())
        return;

    xcb_window_t              targetWindow = getProxyWindow(m_lastSurface->m_xID);

    const auto                XCOORDS = g_pXWaylandManager->waylandToXWaylandCoords(m_lastSurfaceCoords + local);
    const uint32_t            coords  = ((uint32_t)XCOORDS.x << 16) | (uint32_t)XCOORDS.y;

    xcb_client_message_data_t data = {{0}};
    data.data32[0]                 = g_pXWayland->m_wm->m_dndSelection.window;
    data.data32[2]                 = coords;
    data.data32[3]                 = timeMs;
    data.data32[4]                 = dndActionToAtom(m_lastOffer->getSource()->actions());

    sendDndEvent(targetWindow, HYPRATOMS["XdndPosition"], data);

    m_lastTime = timeMs;
#endif
}

void CX11DataDevice::sendDrop() {
#ifndef NO_XWAYLAND
    if (!m_lastSurface || !m_lastOffer) {
        Debug::log(ERR, "CX11DataDevice::sendDrop: No surface or offer");
        return;
    }

    xcb_window_t              targetWindow = getProxyWindow(m_lastSurface->m_xID);

    xcb_client_message_data_t data = {{0}};
    data.data32[0]                 = g_pXWayland->m_wm->m_dndSelection.window;
    data.data32[2]                 = m_lastTime;

    sendDndEvent(targetWindow, HYPRATOMS["XdndDrop"], data);

    cleanupState();
#endif
}

void CX11DataDevice::sendSelection(SP<IDataOffer> offer) {
    ; // no-op. Selection is done separately.
}

eDataSourceType CX11DataDevice::type() {
    return DATA_SOURCE_TYPE_X11;
}

SP<CWLDataDeviceResource> CX11DataDevice::getWayland() {
    return nullptr;
}

SP<CX11DataDevice> CX11DataDevice::getX11() {
    return m_self.lock();
}

std::vector<std::string> CX11DataSource::mimes() {
    return m_mimeTypes;
}

void CX11DataSource::send(const std::string& mime, CFileDescriptor fd) {
    ; // no-op
}

void CX11DataSource::accepted(const std::string& mime) {
    ; // no-op
}

void CX11DataSource::cancelled() {
    m_dndSuccess = false;
    m_dropped    = false;
}

bool CX11DataSource::hasDnd() {
    return m_dnd;
}

bool CX11DataSource::dndDone() {
    return m_dropped;
}

void CX11DataSource::error(uint32_t code, const std::string& msg) {
    Debug::log(ERR, "CX11DataSource error: code {} msg {}", code, msg);
    m_dndSuccess = false;
    m_dropped    = false;
}

void CX11DataSource::sendDndFinished() {
    m_dndSuccess = true;
}

uint32_t CX11DataSource::actions() {
    return m_supportedActions;
}

eDataSourceType CX11DataSource::type() {
    return DATA_SOURCE_TYPE_X11;
}

void CX11DataSource::sendDndDropPerformed() {
    m_dropped = true;
}

void CX11DataSource::sendDndAction(wl_data_device_manager_dnd_action a) {
    ; // no-op
}

void CX11DataDevice::forceCleanupDnd() {
#ifndef NO_XWAYLAND
    if (m_lastOffer) {
        auto source = m_lastOffer->getSource();
        if (source) {
            source->cancelled();
            source->sendDndFinished();
        }
    }

    xcb_set_selection_owner(g_pXWayland->m_wm->m_connection, XCB_ATOM_NONE, HYPRATOMS["XdndSelection"], XCB_TIME_CURRENT_TIME);
    xcb_flush(g_pXWayland->m_wm->m_connection);

    cleanupState();

    g_pSeatManager->setPointerFocus(nullptr, {});
    g_pInputManager->simulateMouseMovement();
#endif
}
