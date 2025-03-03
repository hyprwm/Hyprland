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

    xcb_send_event(g_pXWayland->pWM->connection, 0, targetWindow, XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
    xcb_flush(g_pXWayland->pWM->connection);
}

xcb_window_t CX11DataDevice::getProxyWindow(xcb_window_t window) {
    xcb_window_t              targetWindow = window;
    xcb_get_property_cookie_t proxyCookie =
        xcb_get_property(g_pXWayland->pWM->connection, PROPERTY_OFFSET, window, HYPRATOMS["XdndProxy"], XCB_ATOM_WINDOW, PROPERTY_OFFSET, PROPERTY_LENGTH);
    xcb_get_property_reply_t* proxyReply = xcb_get_property_reply(g_pXWayland->pWM->connection, proxyCookie, nullptr);

    const auto                isValidPropertyReply = [](xcb_get_property_reply_t* reply) {
        return reply && reply->type == XCB_ATOM_WINDOW && reply->format == PROPERTY_FORMAT_32BIT && xcb_get_property_value_length(reply) == sizeof(xcb_window_t);
    };

    if (isValidPropertyReply(proxyReply)) {
        xcb_window_t              proxyWindow = *(xcb_window_t*)xcb_get_property_value(proxyReply);

        xcb_get_property_cookie_t proxyVerifyCookie =
            xcb_get_property(g_pXWayland->pWM->connection, PROPERTY_OFFSET, proxyWindow, HYPRATOMS["XdndProxy"], XCB_ATOM_WINDOW, PROPERTY_OFFSET, PROPERTY_LENGTH);
        xcb_get_property_reply_t* proxyVerifyReply = xcb_get_property_reply(g_pXWayland->pWM->connection, proxyVerifyCookie, nullptr);

        if (isValidPropertyReply(proxyVerifyReply)) {
            xcb_window_t verifyWindow = *(xcb_window_t*)xcb_get_property_value(proxyVerifyReply);
            if (verifyWindow == proxyWindow) {
                targetWindow = proxyWindow;
                NDebug::log(LOG, "Using XdndProxy window {:x} for window {:x}", proxyWindow, window);
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
    return self.lock();
}

SP<CIDataSource> CX11DataOffer::getSource() {
    return source.lock();
}

void CX11DataOffer::markDead() {
#ifndef NO_XWAYLAND
    std::erase(g_pXWayland->pWM->dndDataOffers, self);
#endif
}

void CX11DataDevice::sendDataOffer(SP<CIDataOffer> offer) {
    ; // no-op, I don't think this has an X equiv
}

void CX11DataDevice::sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<CIDataOffer> offer) {
#ifndef NO_XWAYLAND
    auto XSURF = g_pXWayland->pWM->windowForWayland(surf);

    if (!XSURF) {
        NDebug::log(ERR, "CX11DataDevice::sendEnter: No xwayland surface for destination");
        return;
    }

    auto SOURCE = offer->getSource();

    if (!SOURCE) {
        NDebug::log(ERR, "CX11DataDevice::sendEnter: No source");
        return;
    }

    std::vector<xcb_atom_t> targets;
    // reserve to avoid reallocations
    targets.reserve(SOURCE->mimes().size());
    for (auto const& m : SOURCE->mimes()) {
        targets.push_back(g_pXWayland->pWM->mimeToAtom(m));
    }

    xcb_change_property(g_pXWayland->pWM->connection, XCB_PROP_MODE_REPLACE, g_pXWayland->pWM->dndSelection.window, HYPRATOMS["XdndTypeList"], XCB_ATOM_ATOM, 32, targets.size(),
                        targets.data());

    xcb_set_selection_owner(g_pXWayland->pWM->connection, g_pXWayland->pWM->dndSelection.window, HYPRATOMS["XdndSelection"], XCB_TIME_CURRENT_TIME);
    xcb_flush(g_pXWayland->pWM->connection);

    xcb_window_t              targetWindow = getProxyWindow(XSURF->xID);

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;
    data.data32[1]                 = XDND_VERSION << 24;
    data.data32[1] |= 1;

    sendDndEvent(targetWindow, HYPRATOMS["XdndEnter"], data);

    lastSurface = XSURF;
    lastOffer   = offer;

    auto hlSurface = XSURF->surface.lock();
    if (!hlSurface) {
        NDebug::log(ERR, "CX11DataDevice::sendEnter: Non desktop x surface?!");
        lastSurfaceCoords = {};
        return;
    }

    lastSurfaceCoords = g_pXWaylandManager->xwaylandToWaylandCoords(XSURF->geometry.pos());
#endif
}

void CX11DataDevice::cleanupState() {
    lastSurface.reset();
    lastOffer.reset();
    lastSurfaceCoords = {};
    lastTime          = 0;
}

void CX11DataDevice::sendLeave() {
#ifndef NO_XWAYLAND
    if (!lastSurface)
        return;

    xcb_window_t              targetWindow = getProxyWindow(lastSurface->xID);

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;

    sendDndEvent(targetWindow, HYPRATOMS["XdndLeave"], data);

    cleanupState();
#endif
}

void CX11DataDevice::sendMotion(uint32_t timeMs, const Vector2D& local) {
#ifndef NO_XWAYLAND
    if (!lastSurface || !lastOffer || !lastOffer->getSource())
        return;

    xcb_window_t              targetWindow = getProxyWindow(lastSurface->xID);

    const auto                XCOORDS = g_pXWaylandManager->waylandToXWaylandCoords(lastSurfaceCoords + local);
    const uint32_t            coords  = ((uint32_t)XCOORDS.x << 16) | (uint32_t)XCOORDS.y;

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;
    data.data32[2]                 = coords;
    data.data32[3]                 = timeMs;
    data.data32[4]                 = dndActionToAtom(lastOffer->getSource()->actions());

    sendDndEvent(targetWindow, HYPRATOMS["XdndPosition"], data);

    lastTime = timeMs;
#endif
}

void CX11DataDevice::sendDrop() {
#ifndef NO_XWAYLAND
    if (!lastSurface || !lastOffer) {
        NDebug::log(ERR, "CX11DataDevice::sendDrop: No surface or offer");
        return;
    }

    xcb_window_t              targetWindow = getProxyWindow(lastSurface->xID);

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;
    data.data32[2]                 = lastTime;

    sendDndEvent(targetWindow, HYPRATOMS["XdndDrop"], data);

    cleanupState();
#endif
}

void CX11DataDevice::sendSelection(SP<CIDataOffer> offer) {
    ; // no-op. Selection is done separately.
}

eDataSourceType CX11DataDevice::type() {
    return DATA_SOURCE_TYPE_X11;
}

SP<CWLDataDeviceResource> CX11DataDevice::getWayland() {
    return nullptr;
}

SP<CX11DataDevice> CX11DataDevice::getX11() {
    return self.lock();
}

std::vector<std::string> CX11DataSource::mimes() {
    return mimeTypes;
}

void CX11DataSource::send(const std::string& mime, CFileDescriptor fd) {
    ; // no-op
}

void CX11DataSource::accepted(const std::string& mime) {
    ; // no-op
}

void CX11DataSource::cancelled() {
    dndSuccess = false;
    dropped    = false;
}

bool CX11DataSource::hasDnd() {
    return dnd;
}

bool CX11DataSource::dndDone() {
    return dropped;
}

void CX11DataSource::error(uint32_t code, const std::string& msg) {
    NDebug::log(ERR, "CX11DataSource error: code {} msg {}", code, msg);
    dndSuccess = false;
    dropped    = false;
}

void CX11DataSource::sendDndFinished() {
    dndSuccess = true;
}

uint32_t CX11DataSource::actions() {
    return supportedActions;
}

eDataSourceType CX11DataSource::type() {
    return DATA_SOURCE_TYPE_X11;
}

void CX11DataSource::sendDndDropPerformed() {
    dropped = true;
}

void CX11DataSource::sendDndAction(wl_data_device_manager_dnd_action a) {
    ; // no-op
}

void CX11DataDevice::forceCleanupDnd() {
#ifndef NO_XWAYLAND
    if (lastOffer) {
        auto source = lastOffer->getSource();
        if (source) {
            source->cancelled();
            source->sendDndFinished();
        }
    }

    xcb_set_selection_owner(g_pXWayland->pWM->connection, XCB_ATOM_NONE, HYPRATOMS["XdndSelection"], XCB_TIME_CURRENT_TIME);
    xcb_flush(g_pXWayland->pWM->connection);

    cleanupState();

    g_pSeatManager->setPointerFocus(nullptr, {});
    g_pInputManager->simulateMouseMovement();
#endif
}
