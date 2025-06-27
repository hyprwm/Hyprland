#include "SessionLockManager.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/SessionLock.hpp"
#include "../render/Renderer.hpp"
#include "./managers/SeatManager.hpp"
#include "./managers/input/InputManager.hpp"
#include "./managers/eventLoop/EventLoopManager.hpp"
#include <algorithm>
#include <ranges>

SSessionLockSurface::SSessionLockSurface(SP<CSessionLockSurface> surface_) : surface(surface_) {
    pWlrSurface = surface->surface();

    listeners.map = surface_->m_events.map.registerListener([this](std::any data) {
        mapped = true;

        g_pInputManager->simulateMouseMovement();

        const auto PMONITOR = g_pCompositor->getMonitorFromID(iMonitorID);

        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
    });

    listeners.destroy = surface_->m_events.destroy.registerListener([this](std::any data) {
        if (pWlrSurface == g_pCompositor->m_lastFocus)
            g_pCompositor->m_lastFocus.reset();

        g_pSessionLockManager->removeSessionLockSurface(this);
    });

    listeners.commit = surface_->m_events.commit.registerListener([this](std::any data) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(iMonitorID);

        if (mapped && !g_pCompositor->m_lastFocus)
            g_pInputManager->simulateMouseMovement();

        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
    });
}

CSessionLockManager::CSessionLockManager() {
    m_listeners.newLock = PROTO::sessionLock->m_events.newLock.registerListener([this](std::any data) { this->onNewSessionLock(std::any_cast<SP<CSessionLock>>(data)); });
}

void CSessionLockManager::onNewSessionLock(SP<CSessionLock> pLock) {

    static auto PALLOWRELOCK = CConfigValue<Hyprlang::INT>("misc:allow_session_lock_restore");

    if (PROTO::sessionLock->isLocked() && !*PALLOWRELOCK) {
        LOGM(LOG, "Cannot re-lock, misc:allow_session_lock_restore is disabled");
        pLock->sendDenied();
        return;
    }

    if (m_sessionLock && !clientDenied() && !clientLocked())
        return; // Not allowing to relock in case the old lock is still in a limbo

    LOGM(LOG, "Session got locked by {:x}", (uintptr_t)pLock.get());

    m_sessionLock       = makeUnique<SSessionLock>();
    m_sessionLock->lock = pLock;
    m_sessionLock->lockTimer.reset();

    m_sessionLock->listeners.newSurface = pLock->m_events.newLockSurface.registerListener([this](std::any data) {
        auto       SURFACE = std::any_cast<SP<CSessionLockSurface>>(data);

        const auto PMONITOR = SURFACE->monitor();

        const auto NEWSURFACE  = m_sessionLock->vSessionLockSurfaces.emplace_back(makeUnique<SSessionLockSurface>(SURFACE)).get();
        NEWSURFACE->iMonitorID = PMONITOR->m_id;
        PROTO::fractional->sendScale(SURFACE->surface(), PMONITOR->m_scale);
    });

    m_sessionLock->listeners.unlock = pLock->m_events.unlockAndDestroy.registerListener([this](std::any data) {
        m_sessionLock.reset();
        g_pInputManager->refocus();

        for (auto const& m : g_pCompositor->m_monitors)
            g_pHyprRenderer->damageMonitor(m);
    });

    m_sessionLock->listeners.destroy = pLock->m_events.destroyed.registerListener([this](std::any data) {
        m_sessionLock.reset();
        g_pCompositor->focusSurface(nullptr);

        for (auto const& m : g_pCompositor->m_monitors)
            g_pHyprRenderer->damageMonitor(m);
    });

    g_pCompositor->focusSurface(nullptr);
    g_pSeatManager->setGrab(nullptr);

    m_sessionLock->sendDeniedTimer = SP<CEventLoopTimer>(new CEventLoopTimer(
        std::chrono::seconds(5),
        [](auto, auto) {
            if (!g_pSessionLockManager || g_pSessionLockManager->clientLocked() || g_pSessionLockManager->clientDenied())
                return;

            if (g_pSessionLockManager->m_sessionLock && g_pSessionLockManager->m_sessionLock->lock) {
                g_pSessionLockManager->m_sessionLock->lock->sendDenied();
                g_pSessionLockManager->m_sessionLock->hasSentDenied = true;
            }
        },
        nullptr));

    if (m_sessionLock->sendDeniedTimer)
        g_pEventLoopManager->addTimer(m_sessionLock->sendDeniedTimer);

    // Normally the locked event is sent after each output rendered a lock screen frame.
    // When there are no outputs, send it right away.
    if (g_pCompositor->m_unsafeState) {
        removeSendDeniedTimer();
        m_sessionLock->lock->sendLocked();
        m_sessionLock->hasSentLocked = true;
    }
}

void CSessionLockManager::removeSendDeniedTimer() {
    if (!m_sessionLock || !m_sessionLock->sendDeniedTimer)
        return;

    g_pEventLoopManager->removeTimer(m_sessionLock->sendDeniedTimer);
    m_sessionLock->sendDeniedTimer.reset();
}

bool CSessionLockManager::isSessionLocked() {
    return PROTO::sessionLock->isLocked();
}

WP<SSessionLockSurface> CSessionLockManager::getSessionLockSurfaceForMonitor(uint64_t id) {
    if (!m_sessionLock)
        return {};

    for (auto const& sls : m_sessionLock->vSessionLockSurfaces) {
        if (sls->iMonitorID == id) {
            if (sls->mapped)
                return sls;
            else
                return {};
        }
    }

    return {};
}

void CSessionLockManager::onLockscreenRenderedOnMonitor(uint64_t id) {
    if (!m_sessionLock || m_sessionLock->hasSentLocked || m_sessionLock->hasSentDenied)
        return;
    m_sessionLock->lockedMonitors.emplace(id);
    const bool LOCKED = std::ranges::all_of(g_pCompositor->m_monitors, [this](auto m) { return m_sessionLock->lockedMonitors.contains(m->m_id); });
    if (LOCKED && m_sessionLock->lock->good()) {
        removeSendDeniedTimer();
        m_sessionLock->lock->sendLocked();
        m_sessionLock->hasSentLocked = true;
    }
}

bool CSessionLockManager::isSurfaceSessionLock(SP<CWLSurfaceResource> pSurface) {
    // TODO: this has some edge cases when it's wrong (e.g. destroyed lock but not yet surfaces)
    // but can be easily fixed when I rewrite wlr_surface

    if (!m_sessionLock)
        return false;

    for (auto const& sls : m_sessionLock->vSessionLockSurfaces) {
        if (sls->surface->surface() == pSurface)
            return true;
    }

    return false;
}

void CSessionLockManager::removeSessionLockSurface(SSessionLockSurface* pSLS) {
    if (!m_sessionLock)
        return;

    std::erase_if(m_sessionLock->vSessionLockSurfaces, [&](const auto& other) { return pSLS == other.get(); });

    if (g_pCompositor->m_lastFocus)
        return;

    for (auto const& sls : m_sessionLock->vSessionLockSurfaces) {
        if (!sls->mapped)
            continue;

        g_pCompositor->focusSurface(sls->surface->surface());
        break;
    }
}

bool CSessionLockManager::clientLocked() {
    return m_sessionLock && m_sessionLock->hasSentLocked;
}

bool CSessionLockManager::clientDenied() {
    return m_sessionLock && m_sessionLock->hasSentDenied;
}

bool CSessionLockManager::shallConsiderLockMissing() {
    if (!m_sessionLock)
        return true;

    static auto LOCKDEAD_SCREEN_DELAY = CConfigValue<Hyprlang::INT>("misc:lockdead_screen_delay");

    return m_sessionLock->lockTimer.getMillis() > *LOCKDEAD_SCREEN_DELAY;
}
