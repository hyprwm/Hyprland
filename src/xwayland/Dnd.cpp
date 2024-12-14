#include "Dnd.hpp"
#include "XWM.hpp"
#include "XWayland.hpp"
#include "Server.hpp"
#include "../managers/XWaylandManager.hpp"
#include "../desktop/WLSurface.hpp"

static xcb_atom_t dndActionToAtom(uint32_t actions) {
    if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
        return HYPRATOMS["XdndActionCopy"];
    else if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
        return HYPRATOMS["XdndActionMove"];
    else if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
        return HYPRATOMS["XdndActionAsk"];

    return XCB_ATOM_NONE;
}

eDataSourceType CX11DataOffer::type() {
    return DATA_SOURCE_TYPE_X11;
}

SP<CWLDataOfferResource> CX11DataOffer::getWayland() {
    return nullptr;
}

SP<CX11DataOffer> CX11DataOffer::getX11() {
    return self.lock();
}

SP<IDataSource> CX11DataOffer::getSource() {
    return source.lock();
}

void CX11DataOffer::markDead() {
    std::erase(g_pXWayland->pWM->dndDataOffers, self);
}

void CX11DataDevice::sendDataOffer(SP<IDataOffer> offer) {
    ; // no-op, I don't think this has an X equiv
}

void CX11DataDevice::sendEnter(uint32_t serial, SP<CWLSurfaceResource> surf, const Vector2D& local, SP<IDataOffer> offer) {
    auto XSURF = g_pXWayland->pWM->windowForWayland(surf);

    if (offer == lastOffer)
        return;

    if (!XSURF) {
        Debug::log(ERR, "CX11DataDevice::sendEnter: No xwayland surface for destination");
        return;
    }

    auto SOURCE = offer->getSource();

    if (!SOURCE) {
        Debug::log(ERR, "CX11DataDevice::sendEnter: No source");
        return;
    }

    // invalidate old
    std::erase_if(g_pXWayland->pWM->dndDataOffers, [this](const auto& e) { return e != self; });

    xcb_set_selection_owner(g_pXWayland->pWM->connection, g_pXWayland->pWM->dndSelection.window, HYPRATOMS["XdndSelection"], XCB_TIME_CURRENT_TIME);

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;
    data.data32[1]                 = XDND_VERSION << 24;

    // let the client know it needs to check for DND_TYPE_LIST
    data.data32[1] |= 1;

    std::vector<xcb_atom_t> targets;

    for (auto& mime : SOURCE->mimes()) {
        targets.emplace_back(g_pXWayland->pWM->mimeToAtom(mime));
    }

    xcb_change_property(g_pXWayland->pWM->connection, XCB_PROP_MODE_REPLACE, g_pXWayland->pWM->dndSelection.window, HYPRATOMS["XdndTypeList"], XCB_ATOM_ATOM, 32, targets.size(),
                        targets.data());

    g_pXWayland->pWM->sendDndEvent(surf, HYPRATOMS["XdndEnter"], data);

    lastSurface = XSURF;
    lastOffer   = offer;

    auto hlSurface = CWLSurface::fromResource(surf);
    if (!hlSurface) {
        Debug::log(ERR, "CX11DataDevice::sendEnter: Non desktop x surface?!");
        lastSurfaceCoords = {};
        return;
    }

    lastSurfaceCoords = hlSurface->getSurfaceBoxGlobal().value_or(CBox{}).pos();
}

void CX11DataDevice::sendLeave() {
    if (!lastSurface)
        return;

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;

    g_pXWayland->pWM->sendDndEvent(lastSurface->surface.lock(), HYPRATOMS["XdndLeave"], data);

    lastSurface.reset();
    lastOffer.reset();

    xcb_set_selection_owner(g_pXWayland->pWM->connection, g_pXWayland->pWM->dndSelection.window, XCB_ATOM_NONE, XCB_TIME_CURRENT_TIME);
}

void CX11DataDevice::sendMotion(uint32_t timeMs, const Vector2D& local) {
    if (!lastSurface || !lastOffer || !lastOffer->getSource())
        return;

    const auto                XCOORDS = g_pXWaylandManager->waylandToXWaylandCoords(lastSurfaceCoords + local);

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;
    data.data32[2]                 = (((int32_t)XCOORDS.x) << 16) | (int32_t)XCOORDS.y;
    data.data32[3]                 = timeMs;
    data.data32[4]                 = dndActionToAtom(lastOffer->getSource()->actions());

    g_pXWayland->pWM->sendDndEvent(lastSurface->surface.lock(), HYPRATOMS["XdndPosition"], data);
    lastTime = timeMs;
}

void CX11DataDevice::sendDrop() {
    if (!lastSurface || !lastOffer)
        return;

    // we don't have timeMs here, just send last time + 1

    xcb_client_message_data_t data = {0};
    data.data32[0]                 = g_pXWayland->pWM->dndSelection.window;
    data.data32[2]                 = lastTime + 1;

    g_pXWayland->pWM->sendDndEvent(lastSurface->surface.lock(), HYPRATOMS["XdndDrop"], data);
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
    return self.lock();
}

std::vector<std::string> CX11DataSource::mimes() {
    return mimeTypes;
}

void CX11DataSource::send(const std::string& mime, uint32_t fd) {
    ;
}

void CX11DataSource::accepted(const std::string& mime) {
    ;
}

void CX11DataSource::cancelled() {
    ;
}

bool CX11DataSource::hasDnd() {
    return dnd;
}

bool CX11DataSource::dndDone() {
    return dropped;
}

void CX11DataSource::error(uint32_t code, const std::string& msg) {
    Debug::log(ERR, "CX11DataSource::error: this fn is a stub: code {} msg {}", code, msg);
}

void CX11DataSource::sendDndFinished() {
    ;
}

uint32_t CX11DataSource::actions() {
    return supportedActions;
}

eDataSourceType CX11DataSource::type() {
    return DATA_SOURCE_TYPE_X11;
}

void CX11DataSource::sendDndDropPerformed() {
    ; // FIXME:
}

void CX11DataSource::sendDndAction(wl_data_device_manager_dnd_action a) {
    ; // FIXME:
}
