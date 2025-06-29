#include "SessionLockManager.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/FractionalScale.hpp"
#include "../protocols/SessionLock.hpp"
#include "../managers/SeatManager.hpp"
#include "../render/Renderer.hpp"
#include "../managers/input/InputManager.hpp"
#include <algorithm>
#include <ranges>

SSessionLockSurface::SSessionLockSurface(SP<CSessionLockSurface> surface_) : surface(surface_) {
    pWlrSurface = surface->surface();

    listeners.map = surface_->m_events.map.listen([this] {
        mapped = true;

        g_pInputManager->simulateMouseMovement();

        const auto PMONITOR = g_pCompositor->getMonitorFromID(iMonitorID);

        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
    });

    listeners.destroy = surface_->m_events.destroy.listen([this] {
        if (pWlrSurface == g_pCompositor->m_lastFocus)
            g_pCompositor->m_lastFocus.reset();

        g_pSessionLockManager->removeSessionLockSurface(this);
    });

    listeners.commit = surface_->m_events.commit.listen([this] {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(iMonitorID);

        if (mapped && !g_pCompositor->m_lastFocus)
            g_pInputManager->simulateMouseMovement();

        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
    });
}

CSessionLockManager::CSessionLockManager() {
    m_listeners.newLock = PROTO::sessionLock->m_events.newLock.listen([this](const auto& lock) { this->onNewSessionLock(lock); });
}

void CSessionLockManager::onNewSessionLock(SP<CSessionLock> pLock) {
    static auto PALLOWRELOCK = CConfigValue<Hyprlang::INT>("misc:allow_session_lock_restore");

    if (PROTO::sessionLock->isLocked() && !*PALLOWRELOCK) {
        Debug::log(LOG, "Cannot re-lock, misc:allow_session_lock_restore is disabled");
        pLock->sendDenied();
        return;
    }

    Debug::log(LOG, "Session got locked by {:x}", (uintptr_t)pLock.get());

    m_sessionLock       = makeUnique<SSessionLock>();
    m_sessionLock->lock = pLock;
    m_sessionLock->mLockTimer.reset();

    m_sessionLock->listeners.newSurface = pLock->m_events.newLockSurface.listen([this](const SP<CSessionLockSurface>& surface) {
        const auto PMONITOR = surface->monitor();

        const auto NEWSURFACE  = m_sessionLock->vSessionLockSurfaces.emplace_back(makeUnique<SSessionLockSurface>(surface)).get();
        NEWSURFACE->iMonitorID = PMONITOR->m_id;
        PROTO::fractional->sendScale(surface->surface(), PMONITOR->m_scale);
    });

    m_sessionLock->listeners.unlock = pLock->m_events.unlockAndDestroy.listen([this] {
        m_sessionLock.reset();
        g_pInputManager->refocus();

        for (auto const& m : g_pCompositor->m_monitors)
            g_pHyprRenderer->damageMonitor(m);
    });

    m_sessionLock->listeners.destroy = pLock->m_events.destroyed.listen([this] {
        m_sessionLock.reset();
        g_pCompositor->focusSurface(nullptr);

        for (auto const& m : g_pCompositor->m_monitors)
            g_pHyprRenderer->damageMonitor(m);
    });

    g_pCompositor->focusSurface(nullptr);
    g_pSeatManager->setGrab(nullptr);

    // Normally the locked event is sent after each output rendered a lock screen frame.
    // When there are no outputs, send it right away.
    if (g_pCompositor->m_unsafeState) {
        m_sessionLock->lock->sendLocked();
        m_sessionLock->hasSentLocked = true;
    }
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

// We don't want the red screen to flash.
float CSessionLockManager::getRedScreenAlphaForMonitor(uint64_t id) {
    if (!m_sessionLock)
        return 1.F;

    const auto& NOMAPPEDSURFACETIMER = m_sessionLock->mMonitorsWithoutMappedSurfaceTimers.find(id);

    if (NOMAPPEDSURFACETIMER == m_sessionLock->mMonitorsWithoutMappedSurfaceTimers.end()) {
        m_sessionLock->mMonitorsWithoutMappedSurfaceTimers.emplace(id, CTimer());
        m_sessionLock->mMonitorsWithoutMappedSurfaceTimers[id].reset();
        return 0.f;
    }

    return std::clamp(NOMAPPEDSURFACETIMER->second.getSeconds() - /* delay for screencopy */ 0.5f, 0.f, 1.f);
}

void CSessionLockManager::onLockscreenRenderedOnMonitor(uint64_t id) {
    if (!m_sessionLock || m_sessionLock->hasSentLocked)
        return;
    m_sessionLock->lockedMonitors.emplace(id);
    const bool LOCKED = std::ranges::all_of(g_pCompositor->m_monitors, [this](auto m) { return m_sessionLock->lockedMonitors.contains(m->m_id); });
    if (LOCKED && m_sessionLock->lock->good()) {
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

bool CSessionLockManager::isSessionLockPresent() {
    return m_sessionLock && !m_sessionLock->vSessionLockSurfaces.empty();
}

bool CSessionLockManager::anySessionLockSurfacesPresent() {
    return m_sessionLock && std::ranges::any_of(m_sessionLock->vSessionLockSurfaces, [](const auto& surf) { return surf->mapped; });
}

bool CSessionLockManager::shallConsiderLockMissing() {
    if (!m_sessionLock)
        return false;

    static auto LOCKDEAD_SCREEN_DELAY = CConfigValue<Hyprlang::INT>("misc:lockdead_screen_delay");

    return m_sessionLock->mLockTimer.getMillis() > *LOCKDEAD_SCREEN_DELAY;
}
