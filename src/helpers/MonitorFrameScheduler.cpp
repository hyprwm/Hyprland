#include "MonitorFrameScheduler.hpp"
#include "../config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../render/Renderer.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"

using namespace Render::GL;

CMonitorFrameScheduler::CMonitorFrameScheduler(PHLMONITOR m) : m_monitor(m) {
    ;
}

bool CMonitorFrameScheduler::newSchedulingEnabled() {
    static auto PENABLENEW = CConfigValue<Config::INTEGER>("render:new_render_scheduling");

    return *PENABLENEW && g_pHyprRenderer->explicitSyncSupported() && m_monitor && !m_monitor->m_directScanoutIsActive;
}

void CMonitorFrameScheduler::onSyncFired() {
    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR || !newSchedulingEnabled())
        return;

    // Sync fired: reset submitted state, set as rendered. Check the last render time. If we are running
    // late, we will instantly render here.

    if (std::chrono::duration_cast<std::chrono::microseconds>(hrc::now() - m_lastRenderBegun).count() / 1000.F < 1000.F / PMONITOR->m_refreshRate) {
        // we are in. Frame is valid. We can just render as normal.
        Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> onSyncFired, didn't miss.", PMONITOR->m_name);
        m_renderAtFrame = true;
        return;
    }

    Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> onSyncFired, missed.", PMONITOR->m_name);

    // we are out. The frame is taking too long to render. Begin rendering immediately, but don't commit yet.
    m_pendingThird  = true;
    m_renderAtFrame = false; // block frame rendering, we already scheduled

    m_lastRenderBegun = hrc::now();

    // get a ref to ourselves. renderMonitor can destroy this scheduler if it decides to perform a monitor reload
    // FIXME: this is horrible. "renderMonitor" should not be able to do that.
    auto self = m_self;

    g_pHyprRenderer->renderMonitor(PMONITOR, false);

    if (!self)
        return;

    onFinishRender();
}

void CMonitorFrameScheduler::onPresented() {
    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR || !newSchedulingEnabled())
        return;

    if (!m_pendingThird)
        return;

    Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> onPresented, missed, committing pending.", PMONITOR->m_name);

    m_pendingThird = false;

    Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> onPresented, missed, committing pending at the earliest convenience.", PMONITOR->m_name);

    m_pendingThird = false;

    g_pEventLoopManager->doLater([m = PMONITOR] {
        if (!m)
            return;
        g_pHyprRenderer->commitPendingAndDoExplicitSync(m); // commit the pending frame. If it didn't fire yet (is not rendered) it doesn't matter. Syncs will wait.

        // schedule a frame: we might have some missed damage, which got cleared due to the above commit.
        // TODO: this is not always necessary, but doesn't hurt in general. We likely won't hit this if nothing's happening anyways.
        if (m->m_damage.hasChanged())
            g_pCompositor->scheduleFrameForMonitor(m);
    });
}

void CMonitorFrameScheduler::onFrame() {
    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR || !canRender())
        return;

    PMONITOR->recheckSolitary();

    PMONITOR->m_tearingState.busy = false;

    if (PMONITOR->m_tearingState.activelyTearing && PMONITOR->m_solitaryClient.lock() /* can be invalidated by a recheck */) {

        if (!PMONITOR->m_tearingState.frameScheduledWhileBusy)
            return; // we did not schedule a frame yet to be displayed, but we are tearing. Why render?

        PMONITOR->m_tearingState.nextRenderTorn          = true;
        PMONITOR->m_tearingState.frameScheduledWhileBusy = false;
    }

    if (!newSchedulingEnabled()) {
        PMONITOR->m_lastPresentationTimer.reset();

        g_pHyprRenderer->renderMonitor(PMONITOR);
        return;
    }

    if (!m_renderAtFrame) {
        Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> frame event, but m_renderAtFrame = false.", PMONITOR->m_name);
        return;
    }

    Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> frame event, render = true, rendering normally.", PMONITOR->m_name);

    m_lastRenderBegun = hrc::now();

    // get a ref to ourselves. renderMonitor can destroy this scheduler if it decides to perform a monitor reload
    // FIXME: this is horrible. "renderMonitor" should not be able to do that.
    auto self = m_self;

    g_pHyprRenderer->renderMonitor(PMONITOR);

    if (!self)
        return;

    onFinishRender();
}

void CMonitorFrameScheduler::onFinishRender() {
    m_sync = g_pHyprRenderer->createSyncFDManager(); // this destroys the old sync
    g_pEventLoopManager->doOnReadable(m_sync->fd().duplicate(), [this, self = m_self] {
        if (!self) // might've gotten destroyed
            return;
        onSyncFired();
    });
}

bool CMonitorFrameScheduler::canRender() {
    if ((g_pCompositor->m_aqBackend->hasSession() && !g_pCompositor->m_aqBackend->session->active) || !g_pCompositor->m_sessionActive || g_pCompositor->m_unsafeState) {
        Log::logger->log(Log::WARN, "Attempted to render frame on inactive session!");

        if (g_pCompositor->m_unsafeState && std::ranges::any_of(g_pCompositor->m_monitors.begin(), g_pCompositor->m_monitors.end(), [&](auto& m) {
                return m->m_output != g_pCompositor->m_unsafeOutput->m_output;
            })) {
            // restore from unsafe state
            g_pCompositor->leaveUnsafeState();
        }

        return false; // cannot draw on session inactive (different tty)
    }

    if (!m_monitor->m_enabled)
        return false;

    return true;
}
