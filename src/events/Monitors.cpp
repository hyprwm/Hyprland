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

CMonitor* pMostHzMonitor = nullptr;

void Events::listener_change(wl_listener* listener, void* data) {
    // layout got changed, let's update monitors.
    const auto CONFIG = wlr_output_configuration_v1_create();

    for (auto& m : g_pCompositor->m_vMonitors) {
        const auto CONFIGHEAD = wlr_output_configuration_head_v1_create(CONFIG, m->output);

        // TODO: clients off of disabled
        wlr_box BOX;
        wlr_output_layout_get_box(g_pCompositor->m_sWLROutputLayout, m->output, &BOX);

        //m->vecSize.x = BOX.width;
       // m->vecSize.y = BOX.height;
        m->vecPosition.x = BOX.x;
        m->vecPosition.y = BOX.y;

        CONFIGHEAD->state.enabled = m->output->enabled;
        CONFIGHEAD->state.mode = m->output->current_mode;
        CONFIGHEAD->state.x = m->vecPosition.x;
        CONFIGHEAD->state.y = m->vecPosition.y;

        wlr_output_set_custom_mode(m->output, m->vecPixelSize.x, m->vecPixelSize.y, (int)(round(m->refreshRate * 1000)));
    }

    wlr_output_manager_v1_set_configuration(g_pCompositor->m_sWLROutputMgr, CONFIG);
}

void Events::listener_newOutput(wl_listener* listener, void* data) {
    // new monitor added, let's accomodate for that.
    const auto OUTPUT = (wlr_output*)data;

    if (!OUTPUT->name) {
        Debug::log(ERR, "New monitor has no name?? Ignoring");
        return;
    }

    if (g_pCompositor->getMonitorFromName(std::string(OUTPUT->name))) {
        Debug::log(WARN, "Monitor with name %s already exists, not adding as new!", OUTPUT->name);
        return;
    }

    // add it to real
    const auto PNEWMONITORWRAP = &g_pCompositor->m_vRealMonitors.emplace_back(std::make_shared<CMonitor>());
    const auto PNEWMONITOR = PNEWMONITORWRAP->get();

    PNEWMONITOR->ID = g_pCompositor->getNextAvailableMonitorID();

    PNEWMONITOR->output = OUTPUT;
    PNEWMONITOR->m_pThisWrap = PNEWMONITORWRAP;

    PNEWMONITOR->onConnect(false);

    if ((!pMostHzMonitor || PNEWMONITOR->refreshRate > pMostHzMonitor->refreshRate) && PNEWMONITOR->m_bEnabled)
        pMostHzMonitor = PNEWMONITOR;

    // ready to process cuz we have a monitor
    if (PNEWMONITOR->m_bEnabled)
        g_pCompositor->m_bReadyToProcess = true;
}

void Events::listener_monitorFrame(void* owner, void* data) {
    CMonitor* const PMONITOR = (CMonitor*)owner;

    if ((g_pCompositor->m_sWLRSession && !g_pCompositor->m_sWLRSession->active) || !g_pCompositor->m_bSessionActive) {
        Debug::log(WARN, "Attempted to render frame on inactive session!");
        return; // cannot draw on session inactive (different tty)
    }
    
    if (!PMONITOR->m_bEnabled)
        return;

    static std::chrono::high_resolution_clock::time_point startRender = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point startRenderOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay = std::chrono::high_resolution_clock::now();

    static auto *const PDEBUGOVERLAY = &g_pConfigManager->getConfigValuePtr("debug:overlay")->intValue;
    static auto *const PDAMAGETRACKINGMODE = &g_pConfigManager->getConfigValuePtr("general:damage_tracking_internal")->intValue;
    static auto *const PDAMAGEBLINK = &g_pConfigManager->getConfigValuePtr("debug:damage_blink")->intValue;
    static auto *const PNOVFR = &g_pConfigManager->getConfigValuePtr("misc:no_vfr")->intValue;

    static int damageBlinkCleanup = 0; // because double-buffered

    if (!*PDAMAGEBLINK)
        damageBlinkCleanup = 0;

    if (*PDEBUGOVERLAY == 1) {
        startRender = std::chrono::high_resolution_clock::now();
        g_pDebugOverlay->frameData(PMONITOR);
    }

    if (PMONITOR->framesToSkip > 0) {
        PMONITOR->framesToSkip -= 1;

        if (!PMONITOR->noFrameSchedule)
            g_pCompositor->scheduleFrameForMonitor(PMONITOR);
        else {
            Debug::log(LOG, "NoFrameSchedule hit for %s.", PMONITOR->szName.c_str());
        }
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);

        if (PMONITOR->framesToSkip > 10)
            PMONITOR->framesToSkip = 0;
        return;
    }

    // checks //
    if (PMONITOR->ID == pMostHzMonitor->ID || !*PNOVFR) {  // unfortunately with VFR we don't have the guarantee mostHz is going to be updated all the time, so we have to ignore that
        g_pCompositor->sanityCheckWorkspaces();
        g_pAnimationManager->tick();

        g_pConfigManager->dispatchExecOnce();  // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_bWantsMonitorReload)
            g_pConfigManager->performMonitorReload();

        g_pHyprRenderer->ensureCursorRenderingMode();  // so that the cursor gets hidden/shown if the user requested timeouts
    }
    //       //

    if (PMONITOR->scheduledRecalc) {
        PMONITOR->scheduledRecalc = false;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);
    }

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // check the damage
    pixman_region32_t damage;
    bool hasChanged;
    pixman_region32_init(&damage);

    if (*PDAMAGETRACKINGMODE == -1) {
        Debug::log(CRIT, "Damage tracking mode -1 ????");
        return;
    }

    g_pHyprOpenGL->preRender(PMONITOR);

    if (!wlr_output_damage_attach_render(PMONITOR->damage, &hasChanged, &damage)){
        Debug::log(ERR, "Couldn't attach render to display %s ???", PMONITOR->szName.c_str());
        return;
    }

    // we need to cleanup fading out when rendering the appropriate context
    g_pCompositor->cleanupFadingOut(PMONITOR->ID);

    if (!hasChanged && *PDAMAGETRACKINGMODE != DAMAGE_TRACKING_NONE && PMONITOR->forceFullFrames == 0 && damageBlinkCleanup == 0) {
        pixman_region32_fini(&damage);
        wlr_output_rollback(PMONITOR->output);

        if (*PDAMAGEBLINK || *PNOVFR)
            g_pCompositor->scheduleFrameForMonitor(PMONITOR);

        return;
    }

    // if we have no tracking or full tracking, invalidate the entire monitor
    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR || PMONITOR->forceFullFrames > 0 || damageBlinkCleanup > 0) {
        pixman_region32_union_rect(&damage, &damage, 0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y);

        pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);
    } else {
        static auto* const PBLURENABLED = &g_pConfigManager->getConfigValuePtr("decoration:blur")->intValue;

        // if we use blur we need to expand the damage for proper blurring
        if (*PBLURENABLED == 1) {
            // TODO: can this be optimized?
            static auto* const PBLURSIZE = &g_pConfigManager->getConfigValuePtr("decoration:blur_size")->intValue;
            static auto* const PBLURPASSES = &g_pConfigManager->getConfigValuePtr("decoration:blur_passes")->intValue;
            const auto BLURRADIUS = *PBLURSIZE * pow(2, *PBLURPASSES);  // is this 2^pass? I don't know but it works... I think.

            pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);

            // now, prep the damage, get the extended damage region
            wlr_region_expand(&damage, &damage, BLURRADIUS);                                                   // expand for proper blurring
        } else {
            pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);
        }
    }

    if (PMONITOR->forceFullFrames > 0) {
        PMONITOR->forceFullFrames -= 1;
        if (PMONITOR->forceFullFrames > 10)
            PMONITOR->forceFullFrames = 0;
    }
        

    // TODO: this is getting called with extents being 0,0,0,0 should it be?
    // potentially can save on resources.

    g_pHyprOpenGL->begin(PMONITOR, &damage);
    g_pHyprOpenGL->clear(CColor(17, 17, 17, 255));
    g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"

    g_pHyprRenderer->renderAllClientsForMonitor(PMONITOR->ID, &now);

    // if correct monitor draw hyprerror
    if (PMONITOR->ID == 0)
        g_pHyprError->draw();

    // for drawing the debug overlay
    if (PMONITOR->ID == 0 && *PDEBUGOVERLAY == 1) {
        startRenderOverlay = std::chrono::high_resolution_clock::now();
        g_pDebugOverlay->draw();
        endRenderOverlay = std::chrono::high_resolution_clock::now();
    }

    if (*PDAMAGEBLINK && damageBlinkCleanup == 0) {
        wlr_box monrect = {0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y};
        g_pHyprOpenGL->renderRect(&monrect, CColor(255,0,255,100), 0);
        damageBlinkCleanup = 1;
    } else if (*PDAMAGEBLINK) {
        damageBlinkCleanup++;
        if (damageBlinkCleanup > 3)
            damageBlinkCleanup = 0;
    }

    wlr_renderer_begin(g_pCompositor->m_sWLRRenderer, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    wlr_output_render_software_cursors(PMONITOR->output, NULL);

    wlr_renderer_end(g_pCompositor->m_sWLRRenderer);

    g_pHyprOpenGL->end();

    // calc frame damage
    pixman_region32_t frameDamage;
    pixman_region32_init(&frameDamage);

    const auto TRANSFORM = wlr_output_transform_invert(PMONITOR->output->transform);
    wlr_region_transform(&frameDamage, &PMONITOR->damage->current, TRANSFORM, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y);

    if (*PDAMAGETRACKINGMODE == DAMAGE_TRACKING_NONE || *PDAMAGETRACKINGMODE == DAMAGE_TRACKING_MONITOR)
        pixman_region32_union_rect(&frameDamage, &frameDamage, 0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y);

    if (*PDAMAGEBLINK)
        pixman_region32_union(&frameDamage, &frameDamage, &damage);

    wlr_output_set_damage(PMONITOR->output, &frameDamage);
    pixman_region32_fini(&frameDamage);
    pixman_region32_fini(&damage);

    wlr_output_commit(PMONITOR->output);

    if (*PDAMAGEBLINK || *PNOVFR)
        g_pCompositor->scheduleFrameForMonitor(PMONITOR);

    if (*PDEBUGOVERLAY == 1) {
        const float µs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - startRender).count() / 1000.f;
        g_pDebugOverlay->renderData(PMONITOR, µs);
        if (PMONITOR->ID == 0) {
            const float µsNoOverlay = µs - std::chrono::duration_cast<std::chrono::nanoseconds>(endRenderOverlay - startRenderOverlay).count() / 1000.f;
            g_pDebugOverlay->renderDataNoOverlay(PMONITOR, µsNoOverlay);
        } else {
            g_pDebugOverlay->renderDataNoOverlay(PMONITOR, µs);
        }
    }
}

void Events::listener_monitorDestroy(void* owner, void* data) {
    const auto OUTPUT = (wlr_output*)data;

    CMonitor* pMonitor = nullptr;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->szName == OUTPUT->name) {
            pMonitor = m.get();
            break;
        }
    }

    if (!pMonitor)
        return;

    pMonitor->onDisconnect();

    // cleanup
    g_pCompositor->m_vRealMonitors.erase(std::remove_if(g_pCompositor->m_vRealMonitors.begin(), g_pCompositor->m_vRealMonitors.end(), [&](std::shared_ptr<CMonitor>& el) { return el.get() == pMonitor; }));

    if (pMostHzMonitor == pMonitor) {
        int mostHz = 0;
        CMonitor* pMonitorMostHz = nullptr;

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->refreshRate > mostHz) {
                pMonitorMostHz = m.get();
                mostHz = m->refreshRate;
            }
        }

        pMostHzMonitor = pMonitorMostHz;
    }
}
