#include "MonitorFrameScheduler.hpp"
#include "../config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../render/Renderer.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"

CMonitorFrameScheduler::CMonitorFrameScheduler(PHLMONITOR m) : m_monitor(m) {
    ;
}

bool CMonitorFrameScheduler::newSchedulingEnabled() {
    static auto PENABLENEW = CConfigValue<Hyprlang::INT>("render:new_render_scheduling");

    return *PENABLENEW && g_pHyprOpenGL->explicitSyncSupported();
}

void CMonitorFrameScheduler::onSyncFired() {

    if (!newSchedulingEnabled())
        return;

    // Sync fired: reset submitted state, set as rendered. Check the last render time. If we are running
    // late, we will instantly render here.

    if (std::chrono::duration_cast<std::chrono::microseconds>(hrc::now() - m_lastRenderBegun).count() / 1000.F < 1000.F / m_monitor->m_refreshRate) {
        // we are in. Frame is valid. We can just render as normal.
        Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> onSyncFired, didn't miss.", m_monitor->m_name);
        if (!m_renderAtFrame)
            g_pCompositor->scheduleFrameForMonitor(m_monitor.lock());

        m_renderAtFrame = true;
        return;
    } else if (m_renderAtFrame) {
        m_renderAtFrame = false;
        g_pCompositor->scheduleFrameForMonitor(m_monitor.lock());
        return;
    }

    Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> onSyncFired, rendering third frame.", m_monitor->m_name);
    // we are out. The frame is taking too long to render. Begin rendering immediately, but don't commit yet.
    m_pendingThird = true;

    m_lastRenderBegun = hrc::now();

    // get a ref to ourselves. renderMonitor can destroy this scheduler if it decides to perform a monitor reload
    // FIXME: this is horrible. "renderMonitor" should not be able to do that.
    auto self = m_self;

    g_pHyprRenderer->renderMonitor(m_monitor.lock(), false);

    if (!self)
        return;

    onFinishRender();
}

void CMonitorFrameScheduler::onPresented() {
    if (!newSchedulingEnabled())
        return;

    if (!m_pendingThird)
        return;

    Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> onPresented, missed, committing pending.", m_monitor->m_name);
    m_pendingThird = false;
    auto mon       = m_monitor.lock();
    auto now       = Time::steadyNow();

    if (!mon->isMirror()) {
        g_pHyprRenderer->sendFrameEventsToWorkspace(mon, mon->m_activeWorkspace, now);
        if (mon->m_activeSpecialWorkspace)
            g_pHyprRenderer->sendFrameEventsToWorkspace(mon, mon->m_activeSpecialWorkspace, now);
    }

    g_pHyprRenderer->commitPendingAndDoExplicitSync(mon, true); // commit the pending frame. If it didn't fire yet (is not rendered) it doesn't matter. Syncs will wait.

    // schedule a frame: we might have some missed damage, which got cleared due to the above commit.
    // TODO: this is not always necessary, but doesn't hurt in general. We likely won't hit this if nothing's happening anyways.
    if (mon->m_damage.hasChanged())
        g_pCompositor->scheduleFrameForMonitor(mon);
}

void CMonitorFrameScheduler::onFrame() {
    if (!canRender())
        return;

    m_monitor->recheckSolitary();

    m_monitor->m_tearingState.busy = false;

    if (m_monitor->m_tearingState.activelyTearing && m_monitor->m_solitaryClient.lock() /* can be invalidated by a recheck */) {

        if (!m_monitor->m_tearingState.frameScheduledWhileBusy)
            return; // we did not schedule a frame yet to be displayed, but we are tearing. Why render?

        m_monitor->m_tearingState.nextRenderTorn          = true;
        m_monitor->m_tearingState.frameScheduledWhileBusy = false;
    }

    if (!newSchedulingEnabled()) {
        m_monitor->m_lastPresentationTimer.reset();

        g_pHyprRenderer->renderMonitor(m_monitor.lock());
        return;
    }

    m_lastRenderBegun = hrc::now();
    // get a ref to ourselves. renderMonitor can destroy this scheduler if it decides to perform a monitor reload
    // FIXME: this is horrible. "renderMonitor" should not be able to do that.
    auto self = m_self;

    g_pHyprRenderer->renderMonitor(m_monitor.lock(), true, m_renderAtFrame);

    if (!self)
        return;

    onFinishRender();
}

void CMonitorFrameScheduler::onFinishRender() {
    if (!m_monitor->m_inFence.isValid()) {
        Log::logger->log(Log::ERR, "CMonitorFrameScheduler: {} -> onFinishRender, m_inFence is not valid.", m_monitor->m_name);
        return;
    }

    g_pEventLoopManager->doOnReadable(m_monitor->m_inFence.duplicate(), [this, self = m_self] {
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
