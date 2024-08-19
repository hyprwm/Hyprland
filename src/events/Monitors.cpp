#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "Events.hpp"
#include "../debug/HyprCtl.hpp"
#include "../config/ConfigValue.hpp"
#include "../protocols/Screencopy.hpp"
#include "../protocols/ToplevelExport.hpp"
#include <aquamarine/output/Output.hpp>

// --------------------------------------------------------- //
//   __  __  ____  _   _ _____ _______ ____  _____   _____   //
//  |  \/  |/ __ \| \ | |_   _|__   __/ __ \|  __ \ / ____|  //
//  | \  / | |  | |  \| | | |    | | | |  | | |__) | (___    //
//  | |\/| | |  | | . ` | | |    | | | |  | |  _  / \___ \   //
//  | |  | | |__| | |\  |_| |_   | | | |__| | | \ \ ____) |  //
//  |_|  |_|\____/|_| \_|_____|  |_|  \____/|_|  \_\_____/   //
//                                                           //
// --------------------------------------------------------- //

void Events::listener_monitorFrame(void* owner, void* data) {
    CMonitor* const PMONITOR = (CMonitor*)owner;

    if ((g_pCompositor->m_pAqBackend->hasSession() && !g_pCompositor->m_pAqBackend->session->active) || !g_pCompositor->m_bSessionActive || g_pCompositor->m_bUnsafeState) {
        Debug::log(WARN, "Attempted to render frame on inactive session!");

        if (g_pCompositor->m_bUnsafeState && std::ranges::any_of(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& m) {
                return m->output != g_pCompositor->m_pUnsafeOutput->output;
            })) {
            // restore from unsafe state
            g_pCompositor->leaveUnsafeState();
        }

        return; // cannot draw on session inactive (different tty)
    }

    if (!PMONITOR->m_bEnabled)
        return;

    g_pHyprRenderer->recheckSolitaryForMonitor(PMONITOR);

    PMONITOR->tearingState.busy = false;

    if (PMONITOR->tearingState.activelyTearing && PMONITOR->solitaryClient.lock() /* can be invalidated by a recheck */) {

        if (!PMONITOR->tearingState.frameScheduledWhileBusy)
            return; // we did not schedule a frame yet to be displayed, but we are tearing. Why render?

        PMONITOR->tearingState.nextRenderTorn          = true;
        PMONITOR->tearingState.frameScheduledWhileBusy = false;
    }

    static auto PENABLERAT = CConfigValue<Hyprlang::INT>("misc:render_ahead_of_time");
    static auto PRATSAFE   = CConfigValue<Hyprlang::INT>("misc:render_ahead_safezone");

    PMONITOR->lastPresentationTimer.reset();

    if (*PENABLERAT && !PMONITOR->tearingState.nextRenderTorn) {
        if (!PMONITOR->RATScheduled) {
            // render
            g_pHyprRenderer->renderMonitor(PMONITOR);
        }

        PMONITOR->RATScheduled = false;

        const auto& [avg, max, min] = g_pHyprRenderer->getRenderTimes(PMONITOR);

        if (max + *PRATSAFE > 1000.0 / PMONITOR->refreshRate)
            return;

        const auto MSLEFT = 1000.0 / PMONITOR->refreshRate - PMONITOR->lastPresentationTimer.getMillis();

        PMONITOR->RATScheduled = true;

        const auto ESTRENDERTIME = std::ceil(avg + *PRATSAFE);
        const auto TIMETOSLEEP   = std::floor(MSLEFT - ESTRENDERTIME);

        if (MSLEFT < 1 || MSLEFT < ESTRENDERTIME || TIMETOSLEEP < 1)
            g_pHyprRenderer->renderMonitor(PMONITOR);
        else
            wl_event_source_timer_update(PMONITOR->renderTimer, TIMETOSLEEP);
    } else {
        g_pHyprRenderer->renderMonitor(PMONITOR);
    }
}

void Events::listener_monitorNeedsFrame(void* owner, void* data) {
    const auto PMONITOR = (CMonitor*)owner;

    g_pCompositor->scheduleFrameForMonitor(PMONITOR, Aquamarine::IOutput::AQ_SCHEDULE_NEEDS_FRAME);
}

void Events::listener_monitorCommit(void* owner, void* data) {
    const auto PMONITOR = (CMonitor*)owner;

    if (true) { // FIXME: E->state->committed & WLR_OUTPUT_STATE_BUFFER
        PROTO::screencopy->onOutputCommit(PMONITOR);
        PROTO::toplevelExport->onOutputCommit(PMONITOR);
    }
}

void Events::listener_monitorBind(void* owner, void* data) {
    ;
}
