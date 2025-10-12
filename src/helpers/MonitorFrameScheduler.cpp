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
        Debug::log(TRACE, "CMonitorFrameScheduler: {} -> onSyncFired, didn't miss.", m_monitor->m_name);
        m_renderAtFrame = true;
        return;
    }

    Debug::log(TRACE, "CMonitorFrameScheduler: {} -> onSyncFired, missed.", m_monitor->m_name);

    // we are out. The frame is taking too long to render. Begin rendering immediately, but don't commit yet.
    m_pendingThird  = true;
    m_renderAtFrame = false; // block frame rendering, we already scheduled

    m_lastRenderBegun = hrc::now();

    // get a ref to ourselves. renderMonitor can destroy this scheduler if it decides to perform a monitor reload
    // FIXME: this is horrible. "renderMonitor" should not be able to do that.
    auto self = m_self;

    g_pHyprRenderer->renderMonitor(m_monitor.lock(), false);

    if (!self)
        return;

    onFinishRender();
}

void CMonitorFrameScheduler::onPresented(const Time::steady_tp& when) {
    if (!m_monitor->isMirror())
        g_pHyprRenderer->sendFrameEventsMonitor(m_monitor.lock(), when);

    if (!newSchedulingEnabled())
        return;

    if (!m_pendingThird)
        return;

    Debug::log(TRACE, "CMonitorFrameScheduler: {} -> onPresented, missed, committing pending.", m_monitor->m_name);

    m_pendingThird = false;

    Debug::log(TRACE, "CMonitorFrameScheduler: {} -> onPresented, missed, committing pending at the earliest convenience.", m_monitor->m_name);

    m_pendingThird = false;

    g_pEventLoopManager->doLater([m = m_monitor.lock()] {
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

    if (!m_renderAtFrame) {
        Debug::log(TRACE, "CMonitorFrameScheduler: {} -> frame event, but m_renderAtFrame = false.", m_monitor->m_name);
        return;
    }

    Debug::log(TRACE, "CMonitorFrameScheduler: {} -> frame event, render = true, rendering normally.", m_monitor->m_name);

    m_lastRenderBegun = hrc::now();

    // get a ref to ourselves. renderMonitor can destroy this scheduler if it decides to perform a monitor reload
    // FIXME: this is horrible. "renderMonitor" should not be able to do that.
    auto self = m_self;

    g_pHyprRenderer->renderMonitor(m_monitor.lock());

    if (!self)
        return;

    onFinishRender();
}

void CMonitorFrameScheduler::onFinishRender() {
    m_sync = CEGLSync::create(); // this destroys the old sync
    g_pEventLoopManager->doOnReadable(m_sync->fd().duplicate(), [this, self = m_self] {
        if (!self) // might've gotten destroyed
            return;
        onSyncFired();
    });
}

bool CMonitorFrameScheduler::canRender() {
    if ((g_pCompositor->m_aqBackend->hasSession() && !g_pCompositor->m_aqBackend->session->active) || !g_pCompositor->m_sessionActive || g_pCompositor->m_unsafeState) {
        Debug::log(WARN, "Attempted to render frame on inactive session!");

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
