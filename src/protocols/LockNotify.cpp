#include "LockNotify.hpp"

CHyprlandLockNotification::CHyprlandLockNotification(SP<CHyprlandLockNotificationV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CHyprlandLockNotificationV1* r) { PROTO::lockNotify->destroyNotification(this); });
    m_resource->setOnDestroy([this](CHyprlandLockNotificationV1* r) { PROTO::lockNotify->destroyNotification(this); });
}

bool CHyprlandLockNotification::good() {
    return m_resource->resource();
}

void CHyprlandLockNotification::onLocked() {
    if LIKELY (!m_locked)
        m_resource->sendLocked();

    m_locked = true;
}

void CHyprlandLockNotification::onUnlocked() {
    if LIKELY (m_locked)
        m_resource->sendUnlocked();

    m_locked = false;
}

CLockNotifyProtocol::CLockNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CLockNotifyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(std::make_unique<CHyprlandLockNotifierV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CHyprlandLockNotifierV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CHyprlandLockNotifierV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetLockNotification([this](CHyprlandLockNotifierV1* pMgr, uint32_t id) { this->onGetNotification(pMgr, id); });
}

void CLockNotifyProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CLockNotifyProtocol::destroyNotification(CHyprlandLockNotification* notif) {
    std::erase_if(m_notifications, [&](const auto& other) { return other.get() == notif; });
}

void CLockNotifyProtocol::onGetNotification(CHyprlandLockNotifierV1* pMgr, uint32_t id) {
    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_notifications.emplace_back(makeShared<CHyprlandLockNotification>(makeShared<CHyprlandLockNotificationV1>(CLIENT, pMgr->version(), id))).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_notifications.pop_back();
        return;
    }

    // Already locked?? Send locked right away
    if UNLIKELY (m_isLocked)
        m_notifications.back()->onLocked();
}

void CLockNotifyProtocol::onLocked() {
    if UNLIKELY (m_isLocked) {
        LOGM(ERR, "Not sending lock notification. Already locked!");
        return;
    }

    for (auto const& n : m_notifications) {
        n->onLocked();
    }

    m_isLocked = true;
}

void CLockNotifyProtocol::onUnlocked() {
    if UNLIKELY (!m_isLocked) {
        LOGM(ERR, "Not sending unlock notification. Not locked!");
        return;
    }

    for (auto const& n : m_notifications) {
        n->onUnlocked();
    }

    m_isLocked = false;
}
