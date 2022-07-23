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

SMonitor* pMostHzMonitor = nullptr;

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

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(OUTPUT->name);

    // if it's disabled, disable and ignore
    if (monitorRule.disabled) {
        wlr_output_enable(OUTPUT, 0);
        wlr_output_commit(OUTPUT);

        if (const auto PMONITOR = g_pCompositor->getMonitorFromName(std::string(OUTPUT->name)); PMONITOR) {
            listener_monitorDestroy(nullptr, PMONITOR->output);
        }
        return;
    }

    const auto PNEWMONITOR = g_pCompositor->m_vMonitors.emplace_back(std::make_unique<SMonitor>()).get();

    PNEWMONITOR->output = OUTPUT;
    PNEWMONITOR->ID = g_pCompositor->getNextAvailableMonitorID();
    PNEWMONITOR->szName = OUTPUT->name;

    wlr_output_init_render(OUTPUT, g_pCompositor->m_sWLRAllocator, g_pCompositor->m_sWLRRenderer);

    wlr_output_set_scale(OUTPUT, monitorRule.scale);
    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, monitorRule.scale);
    wlr_output_set_transform(OUTPUT, WL_OUTPUT_TRANSFORM_NORMAL);  // TODO: support other transforms

    wlr_output_enable_adaptive_sync(OUTPUT, 1);

    // create it in the arr
    PNEWMONITOR->vecPosition = monitorRule.offset;
    PNEWMONITOR->vecSize = monitorRule.resolution;
    PNEWMONITOR->refreshRate = monitorRule.refreshRate;

    PNEWMONITOR->hyprListener_monitorFrame.initCallback(&OUTPUT->events.frame, &Events::listener_monitorFrame, PNEWMONITOR);
    PNEWMONITOR->hyprListener_monitorDestroy.initCallback(&OUTPUT->events.destroy, &Events::listener_monitorDestroy, PNEWMONITOR);

    wlr_output_enable(OUTPUT, 1);

    // TODO: this doesn't seem to set the X and Y correctly,
    // wlr_output_layout_output_coords returns invalid values, I think...
    wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, OUTPUT, monitorRule.offset.x, monitorRule.offset.y);

    // set mode, also applies
    g_pHyprRenderer->applyMonitorRule(PNEWMONITOR, &monitorRule, true);

    Debug::log(LOG, "Added new monitor with name %s at %i,%i with size %ix%i, pointer %x", OUTPUT->name, (int)monitorRule.offset.x, (int)monitorRule.offset.y, (int)monitorRule.resolution.x, (int)monitorRule.resolution.y, OUTPUT);

    PNEWMONITOR->damage = wlr_output_damage_create(PNEWMONITOR->output);

    // add a WLR workspace group
    PNEWMONITOR->pWLRWorkspaceGroupHandle = wlr_ext_workspace_group_handle_v1_create(g_pCompositor->m_sWLREXTWorkspaceMgr);
    wlr_ext_workspace_group_handle_v1_output_enter(PNEWMONITOR->pWLRWorkspaceGroupHandle, PNEWMONITOR->output);

    // Workspace
    std::string newDefaultWorkspaceName = "";
    auto WORKSPACEID = monitorRule.defaultWorkspace == "" ? g_pCompositor->m_vWorkspaces.size() + 1 : getWorkspaceIDFromString(monitorRule.defaultWorkspace, newDefaultWorkspaceName);
    
    if (WORKSPACEID == INT_MAX || WORKSPACEID == (long unsigned int)SPECIAL_WORKSPACE_ID) {
        WORKSPACEID = g_pCompositor->m_vWorkspaces.size() + 1;
        newDefaultWorkspaceName = std::to_string(WORKSPACEID);

        Debug::log(LOG, "Invalid workspace= directive name in monitor parsing, workspace name \"%s\" is invalid.", monitorRule.defaultWorkspace.c_str());
    }

    auto PNEWWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    Debug::log(LOG, "New monitor: WORKSPACEID %d, exists: %d", WORKSPACEID, (int)(PNEWWORKSPACE != nullptr));

    if (PNEWWORKSPACE) {
        // workspace exists, move it to the newly connected monitor
        g_pCompositor->moveWorkspaceToMonitor(PNEWWORKSPACE, PNEWMONITOR);
        PNEWMONITOR->activeWorkspace = PNEWWORKSPACE->m_iID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PNEWMONITOR->ID);
        PNEWWORKSPACE->startAnim(true,true,true);
    } else {
        PNEWWORKSPACE = g_pCompositor->m_vWorkspaces.emplace_back(std::make_unique<CWorkspace>(PNEWMONITOR->ID, newDefaultWorkspaceName)).get();

        // We are required to set the name here immediately
        wlr_ext_workspace_handle_v1_set_name(PNEWWORKSPACE->m_pWlrHandle, newDefaultWorkspaceName.c_str());

        PNEWWORKSPACE->m_iID = WORKSPACEID;
    }

    PNEWMONITOR->activeWorkspace = PNEWWORKSPACE->m_iID;
    PNEWMONITOR->scale = monitorRule.scale;

    PNEWMONITOR->forceFullFrames = 3; // force 3 full frames to make sure there is no blinking due to double-buffering.

    g_pCompositor->deactivateAllWLRWorkspaces(PNEWWORKSPACE->m_pWlrHandle);
    PNEWWORKSPACE->setActive(true);
    //

    if (!pMostHzMonitor || monitorRule.refreshRate > pMostHzMonitor->refreshRate)
        pMostHzMonitor = PNEWMONITOR;

    if (!g_pCompositor->m_pLastMonitor) // set the last monitor if it isnt set yet
        g_pCompositor->m_pLastMonitor = PNEWMONITOR;

    g_pEventManager->postEvent(SHyprIPCEvent{"monitoradded", PNEWMONITOR->szName});

    // ready to process cuz we have a monitor
    g_pCompositor->m_bReadyToProcess = true;
}

void Events::listener_monitorFrame(void* owner, void* data) {
    SMonitor* const PMONITOR = (SMonitor*)owner;

    if ((g_pCompositor->m_sWLRSession && !g_pCompositor->m_sWLRSession->active) || !g_pCompositor->m_bSessionActive) {
        Debug::log(WARN, "Attempted to render frame on inactive session!");
        return; // cannot draw on session inactive (different tty)
    }

    static std::chrono::high_resolution_clock::time_point startRender = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point startRenderOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay = std::chrono::high_resolution_clock::now();

    static auto *const PDEBUGOVERLAY = &g_pConfigManager->getConfigValuePtr("debug:overlay")->intValue;
    static auto *const PDAMAGETRACKINGMODE = &g_pConfigManager->getConfigValuePtr("general:damage_tracking_internal")->intValue;
    static auto *const PDAMAGEBLINK = &g_pConfigManager->getConfigValuePtr("debug:damage_blink")->intValue;
    static auto *const PNOVFR = &g_pConfigManager->getConfigValuePtr("misc:no_vfr")->intValue;

    static int damageBlinkCleanup = 0; // because double-buffered

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
        return;
    }

    // checks //
    if (PMONITOR->ID == pMostHzMonitor->ID || !*PNOVFR) {  // unfortunately with VFR we don't have the guarantee mostHz is going to be updated all the time, so we have to ignore that
        g_pCompositor->sanityCheckWorkspaces();
        g_pAnimationManager->tick();

        HyprCtl::tickHyprCtl();  // so that we dont get that race condition multithread bullshit

        g_pConfigManager->dispatchExecOnce();  // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_bWantsMonitorReload)
            g_pConfigManager->performMonitorReload();

        g_pHyprRenderer->ensureCursorRenderingMode();  // so that the cursor gets hidden/shown if the user requested timeouts
    }
    //       //

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

    if (PMONITOR->forceFullFrames > 0)
        PMONITOR->forceFullFrames -= 1;

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

    SMonitor* pMonitor = nullptr;

    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m->szName == OUTPUT->name) {
            pMonitor = m.get();
            break;
        }
    }

    if (!pMonitor)
        return;

    // Cleanup everything. Move windows back, snap cursor, shit.
    const auto BACKUPMON = g_pCompositor->m_vMonitors.front().get();

    if (!BACKUPMON) {
        Debug::log(CRIT, "No monitors! Unplugged last! Exiting.");
        g_pCompositor->cleanup();
        return;
    }

    const auto BACKUPWORKSPACE = BACKUPMON->activeWorkspace > 0 ? std::to_string(BACKUPMON->activeWorkspace) : "name:" + g_pCompositor->getWorkspaceByID(BACKUPMON->activeWorkspace)->m_szName;

    // snap cursor
    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, BACKUPMON->vecPosition.x + BACKUPMON->vecTransformedSize.x / 2.f, BACKUPMON->vecPosition.y + BACKUPMON->vecTransformedSize.y / 2.f);

    // move workspaces
    std::deque<CWorkspace*> wspToMove;
    for (auto& w : g_pCompositor->m_vWorkspaces) {
        if (w->m_iMonitorID == pMonitor->ID) {
            wspToMove.push_back(w.get());
        }
    }

    for (auto& w : wspToMove) {
        g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
        w->startAnim(true, true, true);
    }

    pMonitor->activeWorkspace = -1;

    g_pCompositor->m_vWorkspaces.erase(std::remove_if(g_pCompositor->m_vWorkspaces.begin(), g_pCompositor->m_vWorkspaces.end(), [&](std::unique_ptr<CWorkspace>& el) { return el->m_iMonitorID == pMonitor->ID; }));

    Debug::log(LOG, "Removed monitor %s!", pMonitor->szName.c_str());

    g_pEventManager->postEvent(SHyprIPCEvent{"monitorremoved", pMonitor->szName});

    g_pCompositor->m_vMonitors.erase(std::remove_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](std::unique_ptr<SMonitor>& el) { return el.get() == pMonitor; }));

    if (pMostHzMonitor == pMonitor) {
        int mostHz = 0;
        SMonitor* pMonitorMostHz = nullptr;

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->refreshRate > mostHz) {
                pMonitorMostHz = m.get();
                mostHz = m->refreshRate;
            }
        }

        pMostHzMonitor = pMonitorMostHz;
    }
}
