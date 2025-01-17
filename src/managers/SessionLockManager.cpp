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

    listeners.map = surface_->events.map.registerListener([this](std::any data) {
        mapped = true;

        g_pInputManager->simulateMouseMovement();

        const auto PMONITOR = g_pCompositor->getMonitorFromID(iMonitorID);

        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
    });

    listeners.destroy = surface_->events.destroy.registerListener([this](std::any data) {
        if (pWlrSurface == g_pCompositor->m_pLastFocus)
            g_pCompositor->m_pLastFocus.reset();

        g_pSessionLockManager->removeSessionLockSurface(this);
    });

    listeners.commit = surface_->events.commit.registerListener([this](std::any data) {
        const auto PMONITOR = g_pCompositor->getMonitorFromID(iMonitorID);

        if (mapped && !g_pCompositor->m_pLastFocus)
            g_pInputManager->simulateMouseMovement();

        if (PMONITOR)
            g_pHyprRenderer->damageMonitor(PMONITOR);
    });
}

CSessionLockManager::CSessionLockManager() {
    listeners.newLock = PROTO::sessionLock->events.newLock.registerListener([this](std::any data) { this->onNewSessionLock(std::any_cast<SP<CSessionLock>>(data)); });
}

void CSessionLockManager::onNewSessionLock(SP<CSessionLock> pLock) {

    static auto PALLOWRELOCK = CConfigValue<Hyprlang::INT>("misc:allow_session_lock_restore");

    if (PROTO::sessionLock->isLocked() && !*PALLOWRELOCK) {
        Debug::log(LOG, "Cannot re-lock, misc:allow_session_lock_restore is disabled");
        pLock->sendDenied();
        return;
    }

    Debug::log(LOG, "Session got locked by {:x}", (uintptr_t)pLock.get());

    m_pSessionLock       = std::make_unique<SSessionLock>();
    m_pSessionLock->lock = pLock;
    m_pSessionLock->mLockTimer.reset();

    m_pSessionLock->listeners.newSurface = pLock->events.newLockSurface.registerListener([this](std::any data) {
        auto       SURFACE = std::any_cast<SP<CSessionLockSurface>>(data);

        const auto PMONITOR = SURFACE->monitor();

        const auto NEWSURFACE  = m_pSessionLock->vSessionLockSurfaces.emplace_back(std::make_unique<SSessionLockSurface>(SURFACE)).get();
        NEWSURFACE->iMonitorID = PMONITOR->ID;
        PROTO::fractional->sendScale(SURFACE->surface(), PMONITOR->scale);
    });

    m_pSessionLock->listeners.unlock = pLock->events.unlockAndDestroy.registerListener([this](std::any data) {
        m_pSessionLock.reset();
        g_pInputManager->refocus();

        for (auto const& m : g_pCompositor->m_vMonitors)
            g_pHyprRenderer->damageMonitor(m);
    });

    m_pSessionLock->listeners.destroy = pLock->events.destroyed.registerListener([this](std::any data) {
        m_pSessionLock.reset();
        g_pCompositor->focusSurface(nullptr);

        for (auto const& m : g_pCompositor->m_vMonitors)
            g_pHyprRenderer->damageMonitor(m);
    });

    g_pCompositor->focusSurface(nullptr);
    g_pSeatManager->setGrab(nullptr);
}

bool CSessionLockManager::isSessionLocked() {
    return PROTO::sessionLock->isLocked();
}

SSessionLockSurface* CSessionLockManager::getSessionLockSurfaceForMonitor(uint64_t id) {
    if (!m_pSessionLock)
        return nullptr;

    for (auto const& sls : m_pSessionLock->vSessionLockSurfaces) {
        if (sls->iMonitorID == id) {
            if (sls->mapped)
                return sls.get();
            else
                return nullptr;
        }
    }

    return nullptr;
}

// We don't want the red screen to flash.
float CSessionLockManager::getRedScreenAlphaForMonitor(uint64_t id) {
    if (!m_pSessionLock)
        return 1.F;

    const auto& NOMAPPEDSURFACETIMER = m_pSessionLock->mMonitorsWithoutMappedSurfaceTimers.find(id);

    if (NOMAPPEDSURFACETIMER == m_pSessionLock->mMonitorsWithoutMappedSurfaceTimers.end()) {
        m_pSessionLock->mMonitorsWithoutMappedSurfaceTimers.emplace(id, CTimer());
        m_pSessionLock->mMonitorsWithoutMappedSurfaceTimers[id].reset();
        return 0.f;
    }

    return std::clamp(NOMAPPEDSURFACETIMER->second.getSeconds() - /* delay for screencopy */ 0.5f, 0.f, 1.f);
}

void CSessionLockManager::onLockscreenRenderedOnMonitor(uint64_t id) {
    if (!m_pSessionLock || m_pSessionLock->m_hasSentLocked)
        return;
    m_pSessionLock->m_lockedMonitors.emplace(id);
    const auto MONITORS = g_pCompositor->m_vMonitors;
    const bool LOCKED   = std::all_of(MONITORS.begin(), MONITORS.end(), [this](auto m) { return m_pSessionLock->m_lockedMonitors.contains(m->ID); });
    if (LOCKED && m_pSessionLock->lock->good()) {
        m_pSessionLock->lock->sendLocked();
        m_pSessionLock->m_hasSentLocked = true;
    }
}

bool CSessionLockManager::isSurfaceSessionLock(SP<CWLSurfaceResource> pSurface) {
    // TODO: this has some edge cases when it's wrong (e.g. destroyed lock but not yet surfaces)
    // but can be easily fixed when I rewrite wlr_surface

    if (!m_pSessionLock)
        return false;

    for (auto const& sls : m_pSessionLock->vSessionLockSurfaces) {
        if (sls->surface->surface() == pSurface)
            return true;
    }

    return false;
}

void CSessionLockManager::removeSessionLockSurface(SSessionLockSurface* pSLS) {
    if (!m_pSessionLock)
        return;

    std::erase_if(m_pSessionLock->vSessionLockSurfaces, [&](const auto& other) { return pSLS == other.get(); });

    if (g_pCompositor->m_pLastFocus)
        return;

    for (auto const& sls : m_pSessionLock->vSessionLockSurfaces) {
        if (!sls->mapped)
            continue;

        g_pCompositor->focusSurface(sls->surface->surface());
        break;
    }
}

bool CSessionLockManager::isSessionLockPresent() {
    return m_pSessionLock && !m_pSessionLock->vSessionLockSurfaces.empty();
}

bool CSessionLockManager::anySessionLockSurfacesPresent() {
    return m_pSessionLock && std::ranges::any_of(m_pSessionLock->vSessionLockSurfaces, [](const auto& surf) { return surf->mapped; });
}

bool CSessionLockManager::shallConsiderLockMissing() {
    if (!m_pSessionLock)
        return false;

    static auto LOCKDEAD_SCREEN_DELAY = CConfigValue<Hyprlang::INT>("misc:lockdead_screen_delay");

    return m_pSessionLock->mLockTimer.getMillis() > *LOCKDEAD_SCREEN_DELAY;
}
