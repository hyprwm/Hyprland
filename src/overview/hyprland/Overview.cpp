#include "Overview.hpp"

#include "scene/OverviewScene.hpp"
#include "../../animation/AnimationManager.hpp"
#include "../../config/shared/animation/AnimationTree.hpp"
#include "../../event/EventBus.hpp"
#include "../../managers/SessionLockManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../output/MonitorResources.hpp"
#include "../../render/Renderer.hpp"

using namespace Overview;
using namespace Overview::Hyprland;

COverview::COverview() : m_scene(makeShared<COverviewScene>(*this)) {
    ;
}

COverview::~COverview() {
    m_isOpen = false;
    if (m_progress)
        m_progress->resetAllCallbacks();
    finishClose(false);
}

void COverview::open(PHLMONITOR monitor) {
    if (!monitor || !monitor->m_enabled || monitor->isMirror() || !g_pHyprRenderer || (g_pSessionLockManager && g_pSessionLockManager->isSessionLocked()))
        return;

    if (!monitor->m_lastScanout.expired() || monitor->m_directScanoutIsActive)
        monitor->handleDSleave();

    const auto CURRENT_MONITOR = m_monitor.lock();
    if (m_sceneInstalled && CURRENT_MONITOR != monitor) {
        m_isOpen = false;
        finishClose();
    }

    if (m_isOpen)
        return;

    if (!m_progress) {
        Animation::mgr()->createAnimation(0.F, m_progress, Config::animationTree()->getAnimationPropertyConfig("workspacesIn"), AVARDAMAGE_NONE);
        m_progress->setUpdateCallback([this](auto) {
            if (const auto MONITOR = m_monitor.lock(); MONITOR && g_pHyprRenderer)
                g_pHyprRenderer->damageMonitor(MONITOR);
        });
        m_progress->setCallbackOnEnd(
            [this](auto) {
                if (!m_isOpen)
                    finishClose();
            },
            false);
    }

    const bool NEW_SCENE = !m_sceneInstalled;
    const auto RESOURCES = NEW_SCENE ? monitor->resources() : m_resources;
    if (!RESOURCES)
        return;

    m_isOpen = true;
    if (NEW_SCENE) {
        m_monitor   = monitor;
        m_resources = RESOURCES;
        RESOURCES->m_sceneStack.push(m_scene);
        m_sceneInstalled = true;
        m_progress->setValueAndWarp(0.F);

        m_listeners.monitorDisconnect  = monitor->m_events.disconnect.listen([this] { closeImmediately(); });
        m_listeners.monitorModeChanged = monitor->m_events.modeChanged.listen([this] { closeImmediately(); });
        m_listeners.monitorPreRender   = Event::bus()->m_events.render.preChecks.listen([this](PHLMONITOR monitor) {
            if (monitor != m_monitor.lock() || monitor->resources() == m_resources)
                return;

            closeImmediately();
        });
        if (g_pSessionLockManager)
            m_listeners.sessionLock = g_pSessionLockManager->m_events.lock.listen([this] { closeImmediately(); });
    }

    m_progress->setConfig(Config::animationTree()->getAnimationPropertyConfig("workspacesIn"));
    *m_progress = 1.F;

    monitor->recheckSolitary();
    if (g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(monitor);

    if (NEW_SCENE)
        m_events.opened.emit();
}

void COverview::close() {
    if (!m_isOpen || !m_sceneInstalled)
        return;

    m_isOpen = false;
    m_progress->setConfig(Config::animationTree()->getAnimationPropertyConfig("workspacesOut"));
    *m_progress = 0.F;

    if (const auto MONITOR = m_monitor.lock(); MONITOR && g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

bool COverview::isOpen() const {
    return m_isOpen;
}

void COverview::finishClose(bool emitEvent) {
    if (m_isOpen || !m_sceneInstalled)
        return;

    m_sceneInstalled = false;
    m_listeners      = {};

    const auto MONITOR   = m_monitor.lock();
    const auto RESOURCES = m_resources;
    if (RESOURCES)
        RESOURCES->m_sceneStack.remove(m_scene);

    m_scene->reset();
    m_resources.reset();
    m_monitor.reset();

    if (MONITOR && g_pHyprRenderer) {
        MONITOR->recheckSolitary();
        g_pHyprRenderer->damageMonitor(MONITOR);
    }

    if (emitEvent)
        m_events.closed.emit();
}

void COverview::closeImmediately() {
    if (!m_sceneInstalled)
        return;

    m_isOpen = false;
    if (m_progress)
        m_progress->setValueAndWarp(0.F);
    finishClose();
}
