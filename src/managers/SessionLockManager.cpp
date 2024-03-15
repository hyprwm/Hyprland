#include "SessionLockManager.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"

static void handleSurfaceMap(void* owner, void* data) {
    const auto PSURFACE = (SSessionLockSurface*)owner;

    Debug::log(LOG, "SessionLockSurface {:x} mapped", (uintptr_t)PSURFACE);

    PSURFACE->mapped = true;

    g_pCompositor->focusSurface(PSURFACE->pWlrLockSurface->surface);

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PSURFACE->iMonitorID);

    if (PMONITOR)
        g_pHyprRenderer->damageMonitor(PMONITOR);
}

static void handleSurfaceCommit(void* owner, void* data) {
    const auto PSURFACE = (SSessionLockSurface*)owner;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PSURFACE->iMonitorID);

    if (PMONITOR)
        g_pHyprRenderer->damageMonitor(PMONITOR);
}

static void handleSurfaceDestroy(void* owner, void* data) {
    const auto PSURFACE = (SSessionLockSurface*)owner;

    Debug::log(LOG, "SessionLockSurface {:x} destroyed", (uintptr_t)PSURFACE);

    PSURFACE->hyprListener_commit.removeCallback();
    PSURFACE->hyprListener_destroy.removeCallback();
    PSURFACE->hyprListener_map.removeCallback();

    if (PSURFACE->pWlrLockSurface->surface == g_pCompositor->m_pLastFocus)
        g_pCompositor->m_pLastFocus = nullptr;

    g_pSessionLockManager->removeSessionLockSurface(PSURFACE);
}

void CSessionLockManager::onNewSessionLock(wlr_session_lock_v1* pWlrLock) {

    static auto PALLOWRELOCK = CConfigValue<Hyprlang::INT>("misc:allow_session_lock_restore");

    if (m_sSessionLock.active && (!*PALLOWRELOCK || m_sSessionLock.pWlrLock)) {
        Debug::log(LOG, "Attempted to lock a locked session!");
        wlr_session_lock_v1_destroy(pWlrLock);
        return;
    }

    Debug::log(LOG, "Session got locked by {:x}", (uintptr_t)pWlrLock);

    m_sSessionLock.pWlrLock = pWlrLock;

    g_pCompositor->m_sSeat.exclusiveClient = wl_resource_get_client(pWlrLock->resource);

    m_sSessionLock.hyprListener_newSurface.initCallback(
        &pWlrLock->events.new_surface,
        [&](void* owner, void* data) {
            const auto PSURFACE = &*m_sSessionLock.vSessionLockSurfaces.emplace_back(std::make_unique<SSessionLockSurface>());

            const auto PWLRSURFACE = (wlr_session_lock_surface_v1*)data;

            const auto PMONITOR = g_pCompositor->getMonitorFromOutput(PWLRSURFACE->output);

            if (!PMONITOR) {
                m_sSessionLock.vSessionLockSurfaces.pop_back();
                return;
            }

            PSURFACE->pWlrLockSurface = PWLRSURFACE;
            PSURFACE->iMonitorID      = PMONITOR->ID;

            g_pProtocolManager->m_pFractionalScaleProtocolManager->setPreferredScaleForSurface(PSURFACE->pWlrLockSurface->surface, PMONITOR->scale);

            wlr_session_lock_surface_v1_configure(PWLRSURFACE, PMONITOR->vecSize.x, PMONITOR->vecSize.y);

            PSURFACE->hyprListener_map.initCallback(&PWLRSURFACE->surface->events.map, &handleSurfaceMap, PSURFACE, "SSessionLockSurface");
            PSURFACE->hyprListener_destroy.initCallback(&PWLRSURFACE->events.destroy, &handleSurfaceDestroy, PSURFACE, "SSessionLockSurface");
            PSURFACE->hyprListener_commit.initCallback(&PWLRSURFACE->surface->events.commit, &handleSurfaceCommit, PSURFACE, "SSessionLockSurface");
        },
        pWlrLock, "wlr_session_lock_v1");

    m_sSessionLock.hyprListener_unlock.initCallback(
        &pWlrLock->events.unlock,
        [&](void* owner, void* data) {
            Debug::log(LOG, "Session Unlocked");

            m_sSessionLock.hyprListener_destroy.removeCallback();
            m_sSessionLock.hyprListener_newSurface.removeCallback();
            m_sSessionLock.hyprListener_unlock.removeCallback();

            m_sSessionLock.active = false;

            m_sSessionLock.mMonitorsWithoutMappedSurfaceTimers.clear();

            g_pCompositor->m_sSeat.exclusiveClient = nullptr;
            g_pInputManager->refocus();

            for (auto& m : g_pCompositor->m_vMonitors)
                g_pHyprRenderer->damageMonitor(m.get());
        },
        pWlrLock, "wlr_session_lock_v1");

    m_sSessionLock.hyprListener_destroy.initCallback(
        &pWlrLock->events.destroy,
        [&](void* owner, void* data) {
            Debug::log(LOG, "Session Lock Abandoned");

            m_sSessionLock.hyprListener_destroy.removeCallback();
            m_sSessionLock.hyprListener_newSurface.removeCallback();
            m_sSessionLock.hyprListener_unlock.removeCallback();

            g_pCompositor->m_sSeat.exclusiveClient = nullptr;

            g_pCompositor->focusSurface(nullptr);

            m_sSessionLock.pWlrLock = nullptr;

            for (auto& m : g_pCompositor->m_vMonitors)
                g_pHyprRenderer->damageMonitor(m.get());
        },
        pWlrLock, "wlr_session_lock_v1");

    wlr_session_lock_v1_send_locked(pWlrLock);

    g_pSessionLockManager->activateLock();
}

bool CSessionLockManager::isSessionLocked() {
    return m_sSessionLock.active;
}

SSessionLockSurface* CSessionLockManager::getSessionLockSurfaceForMonitor(uint64_t id) {
    for (auto& sls : m_sSessionLock.vSessionLockSurfaces) {
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
// This violates the protocol a bit, but tries to handle the missing sync between a lock surface beeing created and the red screen beeing drawn.
float CSessionLockManager::getRedScreenAlphaForMonitor(uint64_t id) {
    const auto& NOMAPPEDSURFACETIMER = m_sSessionLock.mMonitorsWithoutMappedSurfaceTimers.find(id);

    if (NOMAPPEDSURFACETIMER == m_sSessionLock.mMonitorsWithoutMappedSurfaceTimers.end()) {
        m_sSessionLock.mMonitorsWithoutMappedSurfaceTimers.emplace(id, CTimer());
        m_sSessionLock.mMonitorsWithoutMappedSurfaceTimers[id].reset();
        return 0.f;
    }

    return std::clamp(NOMAPPEDSURFACETIMER->second.getSeconds() - /* delay for screencopy */ 0.5f, 0.f, 1.f);
}

bool CSessionLockManager::isSurfaceSessionLock(wlr_surface* pSurface) {
    for (auto& sls : m_sSessionLock.vSessionLockSurfaces) {
        if (sls->pWlrLockSurface->surface == pSurface)
            return true;
    }

    return false;
}

void CSessionLockManager::removeSessionLockSurface(SSessionLockSurface* pSLS) {
    std::erase_if(m_sSessionLock.vSessionLockSurfaces, [&](const auto& other) { return pSLS == other.get(); });

    if (g_pCompositor->m_pLastFocus)
        return;

    for (auto& sls : m_sSessionLock.vSessionLockSurfaces) {
        if (!sls->mapped)
            continue;

        g_pCompositor->focusSurface(sls->pWlrLockSurface->surface);
        break;
    }
}

void CSessionLockManager::activateLock() {
    m_sSessionLock.active = true;
}