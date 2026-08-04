#include "MonitorFrameScheduler.hpp"
#include "../config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../render/Renderer.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"

using namespace Render::GL;
using namespace Monitor;

CMonitorFrameScheduler::CMonitorFrameScheduler(PHLMONITOR m) : m_monitor(m) {
    ;
}

CMonitorFrameScheduler::~CMonitorFrameScheduler() {
    if (g_pEventLoopManager && m_renderTimer) {
        m_renderTimer->cancel();
        g_pEventLoopManager->removeTimer(m_renderTimer);
    }
}

bool CMonitorFrameScheduler::newSchedulingEnabled() {
    static auto PENABLENEW = CConfigValue<Config::INTEGER>("render:new_render_scheduling");

    //#TODO: figure out if this can be done on vrr/tearing
    return *PENABLENEW && g_pHyprRenderer->explicitSyncSupported() && m_monitor && !m_monitor->m_tearingState.activelyTearing && !m_monitor->m_vrrActive;
}

void CMonitorFrameScheduler::onPresented(const Time::steady_tp& when, int refreshNs) {
    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR || !newSchedulingEnabled())
        return;

    // drm hands us the period in ns, but its 0 if the connector has no refresh.
    const float HZ  = PMONITOR->m_refreshRate > 0.F ? PMONITOR->m_refreshRate : 60.F;
    m_refreshPeriod = refreshNs > 0 ? std::chrono::nanoseconds(refreshNs) : std::chrono::nanoseconds(static_cast<int64_t>(1'000'000'000.0 / HZ));

    m_earliestNextFlip = when + m_refreshPeriod;
    m_delayNextFrame   = true; // this comes with the assumption next frame from AQ actually comes from the pageflip, the AQ currently does.
}

void CMonitorFrameScheduler::onFrame() {
    // whatever we do below, the arming from onPresented belongs to this frame only.
    const auto EARLIEST_FLIP = std::exchange(m_earliestNextFlip, {});
    const bool DELAY         = std::exchange(m_delayNextFrame, false);

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
        // config change might still have this armed.
        if (!m_renderTimer || (m_renderTimer && !m_renderTimer->armed()))
            renderNow();

        return;
    }

    Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> frame event, scheduling a render on the event loop.", PMONITOR->m_name);

    if (!m_renderTimer) {
        m_renderTimer = makeShared<CEventLoopTimer>(
            std::nullopt,
            [this, self = m_self](SP<CEventLoopTimer>, void*) {
                if (self.expired())
                    return;

                renderNow();
            },
            nullptr);

        g_pEventLoopManager->addTimer(m_renderTimer);
    }

    if (DELAY && m_refreshPeriod > Time::steady_dur::zero() && !m_renderTimer->armed()) /*pageflip emitted .frame()*/ {
        //const auto NOW    = Time::steadyNow();
        //const auto TARGET = EARLIEST_FLIP - estimatedRenderCost();
        //m_renderTimer->updateTimeout(TARGET > NOW ? TARGET - NOW : std::chrono::nanoseconds(50));

        //#TODO: until rendermonitor rewrite happends, we cant measure rendercost reliably.
        m_renderTimer->updateTimeout(std::chrono::nanoseconds(50));
    } else if (!m_renderTimer->armed())                             // idle frame callback emitted .frame()
        m_renderTimer->updateTimeout(std::chrono::nanoseconds(50)); // just add a tiny delay, since the wl_event_loop has no order guarantee, but delaying it means should be last.
}

bool CMonitorFrameScheduler::renderPending() {
    return m_renderTimer && m_renderTimer->armed();
}

void CMonitorFrameScheduler::renderNow() {
    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR || !canRender())
        return;

    PMONITOR->m_lastPresentationTimer.reset();

    // get a ref to ourselves. renderMonitor can destroy this scheduler if it decides to perform a monitor reload
    // FIXME: this is horrible. "renderMonitor" should not be able to do that.
    auto self = m_self;
    g_pHyprRenderer->renderMonitor(PMONITOR);
}

bool CMonitorFrameScheduler::canRender() {
    if ((g_pCompositor->m_aqBackend->hasSession() && !g_pCompositor->m_aqBackend->session->active) || !g_pCompositor->m_sessionActive) {
        Log::logger->log(Log::WARN, "Attempted to render frame on inactive session!");
        return false; // cannot draw on session inactive (different tty)
    }

    if (!m_monitor->m_enabled)
        return false;

    return true;
}
