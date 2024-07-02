#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "Events.hpp"
#include "../debug/HyprCtl.hpp"
#include "../config/ConfigValue.hpp"

// --------------------------------------------------------- //
//   __  __  ____  _   _ _____ _______ ____  _____   _____   //
//  |  \/  |/ __ \| \ | |_   _|__   __/ __ \|  __ \ / ____|  //
//  | \  / | |  | |  \| | | |    | | | |  | | |__) | (___    //
//  | |\/| | |  | | . ` | | |    | | | |  | |  _  / \___ \   //
//  | |  | | |__| | |\  |_| |_   | | | |__| | | \ \ ____) |  //
//  |_|  |_|\____/|_| \_|_____|  |_|  \____/|_|  \_\_____/   //
//                                                           //
// --------------------------------------------------------- //

static void checkDefaultCursorWarp(SP<CMonitor> PNEWMONITOR, std::string monitorName) {

    static auto PCURSORMONITOR    = CConfigValue<std::string>("cursor:default_monitor");
    static auto firstMonitorAdded = std::chrono::steady_clock::now();
    static bool cursorDefaultDone = false;
    static bool firstLaunch       = true;

    const auto  POS = PNEWMONITOR->middle();

    // by default, cursor should be set to first monitor detected
    // this is needed as a default if the monitor given in config above doesn't exist
    if (firstLaunch) {
        firstLaunch = false;
        g_pCompositor->warpCursorTo(POS, true);
        g_pInputManager->refocus();
    }

    if (cursorDefaultDone || *PCURSORMONITOR == STRVAL_EMPTY)
        return;

    // after 10s, don't set cursor to default monitor
    auto timePassedSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - firstMonitorAdded);
    if (timePassedSec.count() > 10) {
        cursorDefaultDone = true;
        return;
    }

    if (*PCURSORMONITOR == monitorName) {
        cursorDefaultDone = true;
        g_pCompositor->warpCursorTo(POS, true);
        g_pInputManager->refocus();
    }
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accommodate for that.
    const auto OUTPUT = (wlr_output*)data;

    if (!OUTPUT->name) {
        Debug::log(ERR, "New monitor has no name?? Ignoring");
        return;
    }

    // add it to real
    auto PNEWMONITOR = g_pCompositor->m_vRealMonitors.emplace_back(makeShared<CMonitor>());
    if (std::string("HEADLESS-1") == OUTPUT->name)
        g_pCompositor->m_pUnsafeOutput = PNEWMONITOR.get();

    PNEWMONITOR->output           = OUTPUT;
    PNEWMONITOR->self             = PNEWMONITOR;
    const bool FALLBACK           = g_pCompositor->m_pUnsafeOutput ? OUTPUT == g_pCompositor->m_pUnsafeOutput->output : false;
    PNEWMONITOR->ID               = FALLBACK ? -1 : g_pCompositor->getNextAvailableMonitorID(OUTPUT->name);
    PNEWMONITOR->isUnsafeFallback = FALLBACK;

    EMIT_HOOK_EVENT("newMonitor", PNEWMONITOR);

    if (!FALLBACK)
        PNEWMONITOR->onConnect(false);

    if (!PNEWMONITOR->m_bEnabled || FALLBACK)
        return;

    // ready to process if we have a real monitor

    if ((!g_pHyprRenderer->m_pMostHzMonitor || PNEWMONITOR->refreshRate > g_pHyprRenderer->m_pMostHzMonitor->refreshRate) && PNEWMONITOR->m_bEnabled)
        g_pHyprRenderer->m_pMostHzMonitor = PNEWMONITOR.get();

    g_pCompositor->m_bReadyToProcess = true;

    g_pConfigManager->m_bWantsMonitorReload = true;
    g_pCompositor->scheduleFrameForMonitor(PNEWMONITOR.get());

    checkDefaultCursorWarp(PNEWMONITOR, OUTPUT->name);

    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iMonitorID == PNEWMONITOR->ID) {
            w->m_iLastSurfaceMonitorID = -1;
            w->updateSurfaceScaleTransformDetails();
        }
    }
}

void Events::listener_monitorFrame(void* owner, void* data) {
    if (g_pCompositor->m_bExitTriggered) {
        // Only signal cleanup once
        g_pCompositor->m_bExitTriggered = false;
        g_pCompositor->cleanup();
        return;
    }

    CMonitor* const PMONITOR = (CMonitor*)owner;

    if ((g_pCompositor->m_sWLRSession && !g_pCompositor->m_sWLRSession->active) || !g_pCompositor->m_bSessionActive || g_pCompositor->m_bUnsafeState) {
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

void Events::listener_monitorDestroy(void* owner, void* data) {
    const auto OUTPUT = (wlr_output*)data;

    CMonitor*  pMonitor = nullptr;

    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (m->output == OUTPUT) {
            pMonitor = m.get();
            break;
        }
    }

    if (!pMonitor)
        return;

    Debug::log(LOG, "Destroy called for monitor {}", pMonitor->output->name);

    if (pMonitor->output->idle_frame)
        wl_event_source_remove(pMonitor->output->idle_frame);

    pMonitor->onDisconnect(true);

    pMonitor->output                 = nullptr;
    pMonitor->m_bRenderingInitPassed = false;

    Debug::log(LOG, "Removing monitor {} from realMonitors", pMonitor->szName);

    std::erase_if(g_pCompositor->m_vRealMonitors, [&](SP<CMonitor>& el) { return el.get() == pMonitor; });
}

void Events::listener_monitorStateRequest(void* owner, void* data) {
    const auto PMONITOR = (CMonitor*)owner;
    const auto E        = (wlr_output_event_request_state*)data;

    if (!PMONITOR->createdByUser)
        return;

    const auto SIZE = E->state->mode ? Vector2D{E->state->mode->width, E->state->mode->height} : Vector2D{E->state->custom_mode.width, E->state->custom_mode.height};

    PMONITOR->forceSize = SIZE;

    SMonitorRule rule = PMONITOR->activeMonitorRule;
    rule.resolution   = SIZE;

    g_pHyprRenderer->applyMonitorRule(PMONITOR, &rule);
}

void Events::listener_monitorDamage(void* owner, void* data) {
    const auto PMONITOR = (CMonitor*)owner;
    const auto E        = (wlr_output_event_damage*)data;

    PMONITOR->addDamage(E->damage);
}

void Events::listener_monitorNeedsFrame(void* owner, void* data) {
    const auto PMONITOR = (CMonitor*)owner;

    g_pCompositor->scheduleFrameForMonitor(PMONITOR);
}

void Events::listener_monitorCommit(void* owner, void* data) {
    const auto PMONITOR = (CMonitor*)owner;

    const auto E = (wlr_output_event_commit*)data;

    if (E->state->committed & WLR_OUTPUT_STATE_BUFFER) {
        g_pProtocolManager->m_pScreencopyProtocolManager->onOutputCommit(PMONITOR, E);
        g_pProtocolManager->m_pToplevelExportProtocolManager->onOutputCommit(PMONITOR, E);
    }
}

void Events::listener_monitorBind(void* owner, void* data) {
    ;
}
