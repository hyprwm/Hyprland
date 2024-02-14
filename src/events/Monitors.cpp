#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"
#include "Events.hpp"
#include "../debug/HyprCtl.hpp"

// --------------------------------------------------------- //
//   __  __  ____  _   _ _____ _______ ____  _____   _____   //
//  |  \/  |/ __ \| \ | |_   _|__   __/ __ \|  __ \ / ____|  //
//  | \  / | |  | |  \| | | |    | | | |  | | |__) | (___    //
//  | |\/| | |  | | . ` | | |    | | | |  | |  _  / \___ \   //
//  | |  | | |__| | |\  |_| |_   | | | |__| | | \ \ ____) |  //
//  |_|  |_|\____/|_| \_|_____|  |_|  \____/|_|  \_\_____/   //
//                                                           //
// --------------------------------------------------------- //

void Events::listener_change(wl_listener* listener, void* data) {
    // layout got changed, let's update monitors.
    const auto CONFIG = wlr_output_configuration_v1_create();

    if (!CONFIG)
        return;

    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (!m->output)
            continue;

        if (g_pCompositor->m_pUnsafeOutput == m.get())
            continue;

        const auto CONFIGHEAD = wlr_output_configuration_head_v1_create(CONFIG, m->output);

        CBox       BOX;
        wlr_output_layout_get_box(g_pCompositor->m_sWLROutputLayout, m->output, BOX.pWlr());
        BOX.applyFromWlr();

        //m->vecSize.x = BOX.width;
        // m->vecSize.y = BOX.height;
        m->vecPosition.x = BOX.x;
        m->vecPosition.y = BOX.y;

        CONFIGHEAD->state.enabled = m->output->enabled;
        CONFIGHEAD->state.mode    = m->output->current_mode;
        if (!m->output->current_mode) {
            CONFIGHEAD->state.custom_mode = {
                m->output->width,
                m->output->height,
                m->output->refresh,
            };
        }
        CONFIGHEAD->state.x                     = m->vecPosition.x;
        CONFIGHEAD->state.y                     = m->vecPosition.y;
        CONFIGHEAD->state.transform             = m->transform;
        CONFIGHEAD->state.scale                 = m->scale;
        CONFIGHEAD->state.adaptive_sync_enabled = m->vrrActive;
    }

    wlr_output_manager_v1_set_configuration(g_pCompositor->m_sWLROutputMgr, CONFIG);
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accommodate for that.
    const auto OUTPUT = (wlr_output*)data;

    // for warping the cursor on launch
    static bool firstLaunch = true;

    if (!OUTPUT->name) {
        Debug::log(ERR, "New monitor has no name?? Ignoring");
        return;
    }

    // add it to real
    std::shared_ptr<CMonitor>* PNEWMONITORWRAP = nullptr;

    PNEWMONITORWRAP = &g_pCompositor->m_vRealMonitors.emplace_back(std::make_shared<CMonitor>());
    if (std::string("HEADLESS-1") == OUTPUT->name)
        g_pCompositor->m_pUnsafeOutput = PNEWMONITORWRAP->get();

    (*PNEWMONITORWRAP)->output    = OUTPUT;
    const bool FALLBACK           = g_pCompositor->m_pUnsafeOutput ? OUTPUT == g_pCompositor->m_pUnsafeOutput->output : false;
    (*PNEWMONITORWRAP)->ID        = FALLBACK ? -1 : g_pCompositor->getNextAvailableMonitorID(OUTPUT->name);
    const auto PNEWMONITOR        = PNEWMONITORWRAP->get();
    PNEWMONITOR->isUnsafeFallback = FALLBACK;

    if (!FALLBACK)
        PNEWMONITOR->onConnect(false);

    if (!PNEWMONITOR->m_bEnabled || FALLBACK)
        return;

    // ready to process if we have a real monitor

    if ((!g_pHyprRenderer->m_pMostHzMonitor || PNEWMONITOR->refreshRate > g_pHyprRenderer->m_pMostHzMonitor->refreshRate) && PNEWMONITOR->m_bEnabled)
        g_pHyprRenderer->m_pMostHzMonitor = PNEWMONITOR;

    g_pCompositor->m_bReadyToProcess = true;

    g_pConfigManager->m_bWantsMonitorReload = true;
    g_pCompositor->scheduleFrameForMonitor(PNEWMONITOR);

    if (firstLaunch) {
        firstLaunch    = false;
        const auto POS = PNEWMONITOR->middle();
        if (g_pCompositor->m_sSeat.mouse)
            wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, POS.x, POS.y);
    } else {
        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_iMonitorID == PNEWMONITOR->ID) {
                w->m_iLastSurfaceMonitorID = -1;
                w->updateSurfaceScaleTransformDetails();
            }
        }
    }
}

void Events::listener_monitorFrame(void* owner, void* data) {
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

    if (PMONITOR->tearingState.activelyTearing && PMONITOR->solitaryClient /* can be invalidated by a recheck */) {

        if (!PMONITOR->tearingState.frameScheduledWhileBusy)
            return; // we did not schedule a frame yet to be displayed, but we are tearing. Why render?

        PMONITOR->tearingState.nextRenderTorn          = true;
        PMONITOR->tearingState.frameScheduledWhileBusy = false;
    }

    static auto* const PENABLERAT = &g_pConfigManager->getConfigValuePtr("misc:render_ahead_of_time")->intValue;
    static auto* const PRATSAFE   = &g_pConfigManager->getConfigValuePtr("misc:render_ahead_safezone")->intValue;

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

    pMonitor->onDisconnect(true);

    pMonitor->output                 = nullptr;
    pMonitor->m_bRenderingInitPassed = false;

    Debug::log(LOG, "Removing monitor {} from realMonitors", pMonitor->szName);

    std::erase_if(g_pCompositor->m_vRealMonitors, [&](std::shared_ptr<CMonitor>& el) { return el.get() == pMonitor; });
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
