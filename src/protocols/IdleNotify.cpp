#include "IdleNotify.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"

static int onTimer(SP<CEventLoopTimer> self, void* data) {

    const auto NOTIF = sc<CExtIdleNotification*>(data);

    NOTIF->onTimerFired();

    return 0;
}

CExtIdleNotification::CExtIdleNotification(SP<CExtIdleNotificationV1> resource_, uint32_t timeoutMs_, bool obeyInhibitors_) :
    m_resource(resource_), m_timeoutMs(timeoutMs_), m_obeyInhibitors(obeyInhibitors_) {
    if UNLIKELY (!resource_->resource())
        return;

    m_resource->setDestroy([this](CExtIdleNotificationV1* r) { PROTO::idle->destroyNotification(this); });
    m_resource->setOnDestroy([this](CExtIdleNotificationV1* r) { PROTO::idle->destroyNotification(this); });

    m_timer = makeShared<CEventLoopTimer>(std::nullopt, onTimer, this);
    g_pEventLoopManager->addTimer(m_timer);

    update();

    LOGM(LOG, "Registered idle-notification for {}ms", timeoutMs_);
}

CExtIdleNotification::~CExtIdleNotification() {
    g_pEventLoopManager->removeTimer(m_timer);
    m_timer.reset();
}

bool CExtIdleNotification::good() {
    return m_resource->resource();
}

void CExtIdleNotification::update(uint32_t elapsedMs) {
    m_timer->updateTimeout(std::nullopt);

    if (elapsedMs == 0 && PROTO::idle->isInhibited && m_obeyInhibitors) {
        reset();
        return;
    }

    if (m_timeoutMs > elapsedMs) {
        reset();
        m_timer->updateTimeout(std::chrono::milliseconds(m_timeoutMs - elapsedMs));
    } else
        onTimerFired();
}

void CExtIdleNotification::update() {
    update(0);
}

void CExtIdleNotification::onTimerFired() {
    if (m_idled)
        return;

    m_resource->sendIdled();
    m_idled = true;
}

void CExtIdleNotification::reset() {
    if (!m_idled)
        return;

    m_resource->sendResumed();
    m_idled = false;
}

bool CExtIdleNotification::inhibitorsAreObeyed() const {
    return m_obeyInhibitors;
}

CIdleNotifyProtocol::CIdleNotifyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CIdleNotifyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CExtIdleNotifierV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CExtIdleNotifierV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CExtIdleNotifierV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetIdleNotification(
        [this](CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat) { this->onGetNotification(pMgr, id, timeout, seat, true); });
    RESOURCE->setGetInputIdleNotification(
        [this](CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat) { this->onGetNotification(pMgr, id, timeout, seat, false); });
}

void CIdleNotifyProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CIdleNotifyProtocol::destroyNotification(CExtIdleNotification* notif) {
    std::erase_if(m_notifications, [&](const auto& other) { return other.get() == notif; });
}

void CIdleNotifyProtocol::onGetNotification(CExtIdleNotifierV1* pMgr, uint32_t id, uint32_t timeout, wl_resource* seat, bool obeyInhibitors) {
    const auto CLIENT = pMgr->client();
    const auto RESOURCE =
        m_notifications.emplace_back(makeShared<CExtIdleNotification>(makeShared<CExtIdleNotificationV1>(CLIENT, pMgr->version(), id), timeout, obeyInhibitors)).get();

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_notifications.pop_back();
        return;
    }
}

void CIdleNotifyProtocol::onActivity() {
    for (auto const& n : m_notifications) {
        n->update();
    }
}

void CIdleNotifyProtocol::setInhibit(bool inhibited) {
    isInhibited = inhibited;
    for (auto const& n : m_notifications) {
        if (n->inhibitorsAreObeyed())
            n->update();
    }
}

void CIdleNotifyProtocol::setTimers(uint32_t elapsedMs) {
    for (auto const& n : m_notifications) {
        n->update(elapsedMs);
    }
}
