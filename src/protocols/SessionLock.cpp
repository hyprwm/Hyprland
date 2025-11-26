#include "SessionLock.hpp"
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "FractionalScale.hpp"
#include "LockNotify.hpp"
#include "core/Compositor.hpp"
#include "core/Output.hpp"
#include "../helpers/Monitor.hpp"
#include "../render/Renderer.hpp"

CSessionLockSurface::CSessionLockSurface(SP<CExtSessionLockSurfaceV1> resource_, SP<CWLSurfaceResource> surface_, PHLMONITOR pMonitor_, WP<CSessionLock> owner_) :
    m_resource(resource_), m_sessionLock(owner_), m_surface(surface_), m_monitor(pMonitor_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CExtSessionLockSurfaceV1* r) {
        m_events.destroy.emit();
        PROTO::sessionLock->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CExtSessionLockSurfaceV1* r) {
        m_events.destroy.emit();
        PROTO::sessionLock->destroyResource(this);
    });

    m_resource->setAckConfigure([this](CExtSessionLockSurfaceV1* r, uint32_t serial) { m_ackdConfigure = true; });

    m_listeners.surfaceCommit = m_surface->m_events.commit.listen([this] {
        if (!m_surface->m_current.texture) {
            LOGM(ERR, "SessionLock attached a null buffer");
            m_resource->error(EXT_SESSION_LOCK_SURFACE_V1_ERROR_NULL_BUFFER, "Null buffer attached");
            return;
        }

        if (!m_ackdConfigure) {
            LOGM(ERR, "SessionLock committed without an ack");
            m_resource->error(EXT_SESSION_LOCK_SURFACE_V1_ERROR_COMMIT_BEFORE_FIRST_ACK, "Committed surface before first ack");
            return;
        }

        if (m_committed)
            m_events.commit.emit();
        else {
            m_surface->map();
            m_events.map.emit();
        }
        m_committed = true;
    });

    m_listeners.surfaceDestroy = m_surface->m_events.destroy.listen([this] {
        LOGM(WARN, "SessionLockSurface object remains but surface is being destroyed???");
        m_surface->unmap();
        m_listeners.surfaceCommit.reset();
        m_listeners.surfaceDestroy.reset();
        if (g_pCompositor->m_lastFocus == m_surface)
            g_pCompositor->m_lastFocus.reset();

        m_surface.reset();
    });

    if (m_monitor) {
        PROTO::fractional->sendScale(surface_, m_monitor->m_scale);

        if (m_surface)
            m_surface->enter(m_monitor.lock());
    }

    sendConfigure();

    m_listeners.monitorMode = m_monitor->m_events.modeChanged.listen([this] { sendConfigure(); });
}

CSessionLockSurface::~CSessionLockSurface() {
    if (m_surface && m_surface->m_mapped)
        m_surface->unmap();
    m_listeners.surfaceCommit.reset();
    m_listeners.surfaceDestroy.reset();
    m_events.destroy.emit(); // just in case.
}

void CSessionLockSurface::sendConfigure() {
    if (!m_monitor) {
        LOGM(ERR, "sendConfigure: monitor is gone");
        return;
    }

    const auto SERIAL = g_pSeatManager->nextSerial(g_pSeatManager->seatResourceForClient(m_resource->client()));
    m_resource->sendConfigure(SERIAL, m_monitor->m_size.x, m_monitor->m_size.y);
}

bool CSessionLockSurface::good() {
    return m_resource->resource();
}

bool CSessionLockSurface::inert() {
    return m_sessionLock.expired();
}

PHLMONITOR CSessionLockSurface::monitor() {
    return m_monitor.lock();
}

SP<CWLSurfaceResource> CSessionLockSurface::surface() {
    return m_surface.lock();
}

CSessionLock::CSessionLock(SP<CExtSessionLockV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setDestroy([this](CExtSessionLockV1* r) { PROTO::sessionLock->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtSessionLockV1* r) { PROTO::sessionLock->destroyResource(this); });

    m_resource->setGetLockSurface([this](CExtSessionLockV1* r, uint32_t id, wl_resource* surf, wl_resource* output) {
        if (m_inert) {
            LOGM(ERR, "Lock is trying to send getLockSurface after it's inert");
            return;
        }

        PROTO::sessionLock->onGetLockSurface(r, id, surf, output);
    });

    m_resource->setUnlockAndDestroy([this](CExtSessionLockV1* r) {
        if (m_inert) {
            PROTO::sessionLock->destroyResource(this);
            return;
        }

        PROTO::sessionLock->m_locked = false;

        PROTO::lockNotify->onUnlocked();

        m_events.unlockAndDestroy.emit();

        // if lock tools have hidden it and doesn't restore it, we won't receive a new cursor until the cursorshape protocol gives us one.
        // so set it to left_ptr so the "desktop/wallpaper" doesn't end up missing a cursor until hover over a window sending us a shape.
        g_pHyprRenderer->setCursorFromName("left_ptr");

        m_inert = true;
        PROTO::sessionLock->destroyResource(this);
    });
}

CSessionLock::~CSessionLock() {
    m_events.destroyed.emit();
}

void CSessionLock::sendLocked() {
    m_resource->sendLocked();
    PROTO::lockNotify->onLocked();
}

bool CSessionLock::good() {
    return m_resource->resource();
}

void CSessionLock::sendDenied() {
    m_inert = true;
    m_resource->sendFinished();
}

CSessionLockProtocol::CSessionLockProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CSessionLockProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CExtSessionLockManagerV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CExtSessionLockManagerV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CExtSessionLockManagerV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setLock([this](CExtSessionLockManagerV1* pMgr, uint32_t id) { this->onLock(pMgr, id); });
}

void CSessionLockProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CSessionLockProtocol::destroyResource(CSessionLock* lock) {
    std::erase_if(m_locks, [&](const auto& other) { return other.get() == lock; });
}

void CSessionLockProtocol::destroyResource(CSessionLockSurface* surf) {
    std::erase_if(m_lockSurfaces, [&](const auto& other) { return other.get() == surf; });
}

void CSessionLockProtocol::onLock(CExtSessionLockManagerV1* pMgr, uint32_t id) {

    LOGM(LOG, "New sessionLock with id {}", id);

    const auto CLIENT   = pMgr->client();
    const auto RESOURCE = m_locks.emplace_back(makeShared<CSessionLock>(makeShared<CExtSessionLockV1>(CLIENT, pMgr->version(), id)));

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_locks.pop_back();
        return;
    }

    m_events.newLock.emit(RESOURCE);

    m_locked = true;
}

void CSessionLockProtocol::onGetLockSurface(CExtSessionLockV1* lock, uint32_t id, wl_resource* surface, wl_resource* output) {
    LOGM(LOG, "New sessionLockSurface with id {}", id);

    auto             PSURFACE = CWLSurfaceResource::fromResource(surface);
    auto             PMONITOR = CWLOutputResource::fromResource(output)->m_monitor.lock();

    SP<CSessionLock> sessionLock;
    for (auto const& l : m_locks) {
        if (l->m_resource.get() == lock) {
            sessionLock = l;
            break;
        }
    }

    const auto RESOURCE =
        m_lockSurfaces.emplace_back(makeShared<CSessionLockSurface>(makeShared<CExtSessionLockSurfaceV1>(lock->client(), lock->version(), id), PSURFACE, PMONITOR, sessionLock));

    if UNLIKELY (!RESOURCE->good()) {
        lock->noMemory();
        m_lockSurfaces.pop_back();
        return;
    }

    sessionLock->m_events.newLockSurface.emit(RESOURCE);
}

bool CSessionLockProtocol::isLocked() {
    return m_locked;
}
