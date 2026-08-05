#include "MonitorFrameScheduler.hpp"
#include "../config/ConfigValue.hpp"
#include "../Compositor.hpp"
#include "../render/Renderer.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../helpers/Drm.hpp"

using namespace Render::GL;
using namespace Monitor;
using namespace Hyprutils::OS;

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
    // if we bail below, the deadline must not outlive this presentation.
    const auto AIMED_AT = std::exchange(m_inFlightDeadline, {});

    const auto PMONITOR = m_monitor.lock();
    if (!PMONITOR || !newSchedulingEnabled())
        return;

    // drm hands us the period in ns, but its 0 if the connector has no refresh.
    const float HZ  = PMONITOR->m_refreshRate > 0.F ? PMONITOR->m_refreshRate : 60.F;
    m_refreshPeriod = refreshNs > 0 ? std::chrono::nanoseconds(refreshNs) : std::chrono::nanoseconds(static_cast<int64_t>(1'000'000'000.0 / HZ));

    // did the frame we timed actually make the flip it aimed at?
    const auto LATE = when - AIMED_AT;
    if (AIMED_AT.time_since_epoch() > Time::steady_dur::zero() && LATE > m_refreshPeriod / 2 && LATE < m_refreshPeriod * 4)
        Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> missed the flip we aimed at by {:.3f}ms", PMONITOR->m_name,
                         std::chrono::duration<float, std::milli>(LATE).count());

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

                const auto START    = Time::steadyNow();
                const auto DEADLINE = std::exchange(m_pendingDeadline, {});

                renderNow();

                // renderNow -> renderMonitor can destroy us on a monitor reload.
                if (self.expired())
                    return;

                const auto PMONITOR = m_monitor.lock();
                if (!PMONITOR)
                    return;

                if (PMONITOR->m_inFence.isValid() && PMONITOR->output()->pendingPageFlip() && DEADLINE > START && m_refreshPeriod > Time::steady_dur::zero()) {
                    m_inFlightDeadline = DEADLINE; // we committed, so a presentation for this deadline is coming.

                    auto fence = makeShared<CFileDescriptor>(PMONITOR->m_inFence.duplicate());
                    g_pEventLoopManager->doOnReadable(PMONITOR->m_inFence.duplicate(), [this, self, START, fence]() {
                        if (self.expired())
                            return;

                        const auto SIGNALLED = DRM::fenceSignalTime(fence->get());
                        if (!SIGNALLED)
                            return;

                        m_frameTimes.addRenderCost(START, *SIGNALLED);
                    });
                }
            },
            nullptr);

        g_pEventLoopManager->addTimer(m_renderTimer);
    }

    if (DELAY && m_refreshPeriod > Time::steady_dur::zero() && !m_renderTimer->armed()) /*pageflip emitted .frame()*/ {
        const auto NOW      = Time::steadyNow();
        const auto COST     = m_frameTimes.estimatedRenderCost();
        const auto DEADLINE = std::min(EARLIEST_FLIP, NOW + m_refreshPeriod);
        const auto TARGET   = DEADLINE - COST;

        // only delay if the estimate says we can still make this flip.
        const bool CAN_DELAY = m_frameTimes.hasSamples() && COST < m_refreshPeriod && TARGET > NOW;
        const auto ARM_IN    = CAN_DELAY ? TARGET - NOW : FRAME_IDLE_DELAY;

        m_pendingDeadline = DEADLINE;

        Log::logger->log(Log::TRACE, "CMonitorFrameScheduler: {} -> frame event, period {:.3f}ms, target in {:.3f}ms, est. cost {:.3f}ms, arming in {:.3f}ms", PMONITOR->m_name,
                         std::chrono::duration<float, std::milli>(m_refreshPeriod).count(), std::chrono::duration<float, std::milli>(TARGET - NOW).count(),
                         std::chrono::duration<float, std::milli>(COST).count(), std::chrono::duration<float, std::milli>(ARM_IN).count());

        m_renderTimer->updateTimeout(ARM_IN);
    } else if (!m_renderTimer->armed()) { // idle frame callback emitted .frame()
        m_pendingDeadline = {};
        m_renderTimer->updateTimeout(FRAME_IDLE_DELAY); // just add a tiny delay, since the wl_event_loop has no order guarantee
    }
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
