#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/InputManager.hpp"
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

    for (auto& m : g_pCompositor->m_lMonitors) {
        const auto CONFIGHEAD = wlr_output_configuration_head_v1_create(CONFIG, m.output);

        // TODO: clients off of disabled
        wlr_box BOX;
        wlr_output_layout_get_box(g_pCompositor->m_sWLROutputLayout, m.output, &BOX);

        //m.vecSize.x = BOX.width;
       // m.vecSize.y = BOX.height;
        m.vecPosition.x = BOX.x;
        m.vecPosition.y = BOX.y;

        CONFIGHEAD->state.enabled = m.output->enabled;
        CONFIGHEAD->state.mode = m.output->current_mode;
        CONFIGHEAD->state.x = m.vecPosition.x;
        CONFIGHEAD->state.y = m.vecPosition.y;

        wlr_output_set_custom_mode(m.output, m.vecPixelSize.x, m.vecPixelSize.y, (int)(round(m.refreshRate * 1000)));
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

    SMonitor newMonitor;
    newMonitor.output = OUTPUT;
    newMonitor.ID = g_pCompositor->getNextAvailableMonitorID();
    newMonitor.szName = OUTPUT->name;

    wlr_output_init_render(OUTPUT, g_pCompositor->m_sWLRAllocator, g_pCompositor->m_sWLRRenderer);

    wlr_output_set_scale(OUTPUT, monitorRule.scale);
    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, monitorRule.scale);
    wlr_output_set_transform(OUTPUT, WL_OUTPUT_TRANSFORM_NORMAL);  // TODO: support other transforms

    wlr_output_enable_adaptive_sync(OUTPUT, 1);

    // create it in the arr
    newMonitor.vecPosition = monitorRule.offset;
    newMonitor.vecSize = monitorRule.resolution;
    newMonitor.refreshRate = monitorRule.refreshRate;

    g_pCompositor->m_lMonitors.push_back(newMonitor);
    const auto PNEWMONITOR = &g_pCompositor->m_lMonitors.back();

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
    const auto WORKSPACEID = monitorRule.defaultWorkspaceID == -1 ? g_pCompositor->m_lWorkspaces.size() + 1 /* Cuz workspaces doesnt have the new one yet and we start with 1 */ : monitorRule.defaultWorkspaceID;
    
    auto PNEWWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    Debug::log(LOG, "New monitor: WORKSPACEID %d, exists: %d", WORKSPACEID, (int)(PNEWWORKSPACE != nullptr));
    
    if (PNEWWORKSPACE) {
        // workspace exists, move it to the newly connected monitor
        g_pCompositor->moveWorkspaceToMonitor(PNEWWORKSPACE, PNEWMONITOR);
    } else {
        g_pCompositor->m_lWorkspaces.emplace_back(newMonitor.ID);
        PNEWWORKSPACE = &g_pCompositor->m_lWorkspaces.back();

        // We are required to set the name here immediately
        wlr_ext_workspace_handle_v1_set_name(PNEWWORKSPACE->m_pWlrHandle, std::to_string(WORKSPACEID).c_str());

        PNEWWORKSPACE->m_iID = WORKSPACEID;
        PNEWWORKSPACE->m_szName = std::to_string(WORKSPACEID);
    }

    PNEWMONITOR->activeWorkspace = PNEWWORKSPACE->m_iID;
    PNEWMONITOR->scale = monitorRule.scale;

    g_pCompositor->deactivateAllWLRWorkspaces(PNEWWORKSPACE->m_pWlrHandle);
    PNEWWORKSPACE->setActive(true);

    if (!pMostHzMonitor || monitorRule.refreshRate > pMostHzMonitor->refreshRate)
        pMostHzMonitor = PNEWMONITOR;
    //

    if (!g_pCompositor->m_pLastMonitor) // set the last monitor if it isnt set yet
        g_pCompositor->m_pLastMonitor = PNEWMONITOR;

    g_pEventManager->postEvent(SHyprIPCEvent("monitoradded", PNEWMONITOR->szName));

    // ready to process cuz we have a monitor
    g_pCompositor->m_bReadyToProcess = true;
}

void Events::listener_monitorFrame(void* owner, void* data) {
    SMonitor* const PMONITOR = (SMonitor*)owner;

    static std::chrono::high_resolution_clock::time_point startRender = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point startRenderOverlay = std::chrono::high_resolution_clock::now();
    static std::chrono::high_resolution_clock::time_point endRenderOverlay = std::chrono::high_resolution_clock::now();

    if (g_pConfigManager->getInt("debug:overlay") == 1) {
        startRender = std::chrono::high_resolution_clock::now();
        g_pDebugOverlay->frameData(PMONITOR);
    }

    // Hack: only check when monitor with top hz refreshes, saves a bit of resources.
    // This is for stuff that should be run every frame
    if (PMONITOR->ID == pMostHzMonitor->ID) {
        g_pCompositor->sanityCheckWorkspaces();
        g_pAnimationManager->tick();
        g_pCompositor->cleanupFadingOut();

        HyprCtl::tickHyprCtl(); // so that we dont get that race condition multithread bullshit

        g_pConfigManager->dispatchExecOnce(); // We exec-once when at least one monitor starts refreshing, meaning stuff has init'd

        if (g_pConfigManager->m_bWantsMonitorReload)
            g_pConfigManager->performMonitorReload();
    }

    if (PMONITOR->needsFrameSkip) {
        PMONITOR->needsFrameSkip = false;
        wlr_output_schedule_frame(PMONITOR->output);
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITOR->ID);
        return;
    }

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // check the damage
    pixman_region32_t damage;
    bool hasChanged;
    pixman_region32_init(&damage);

    const auto DTMODE = g_pConfigManager->getInt("general:damage_tracking_internal");

    if (DTMODE == -1) {
        Debug::log(CRIT, "Damage tracking mode -1 ????");
        return;
    }

    if (!wlr_output_damage_attach_render(PMONITOR->damage, &hasChanged, &damage)){
        Debug::log(ERR, "Couldn't attach render to display %s ???", PMONITOR->szName.c_str());
        return;
    }

    if (!hasChanged && DTMODE != DAMAGE_TRACKING_NONE) {
        pixman_region32_fini(&damage);
        wlr_output_rollback(PMONITOR->output);
        wlr_output_schedule_frame(PMONITOR->output); // we update shit at the monitor's Hz so we need to schedule frames because rollback wont
        return;
    }

    // if we have no tracking or full tracking, invalidate the entire monitor
    if (DTMODE == DAMAGE_TRACKING_NONE || DTMODE == DAMAGE_TRACKING_MONITOR) {
        pixman_region32_union_rect(&damage, &damage, 0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y);

        pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);
    } else {

        // if we use blur we need to expand the damage for proper blurring
        if (g_pConfigManager->getInt("decoration:blur") == 1) {
            // TODO: can this be optimized?
            const auto BLURSIZE = g_pConfigManager->getInt("decoration:blur_size");
            const auto BLURPASSES = g_pConfigManager->getInt("decoration:blur_passes");

            const auto BLURRADIUS = BLURSIZE * pow(2, BLURPASSES); // is this 2^pass? I don't know but it works... I think.

            pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);

            // now, prep the damage, get the extended damage region
            wlr_region_expand(&damage, &damage, BLURRADIUS);                                                   // expand for proper blurring
        } else {
            pixman_region32_copy(&g_pHyprOpenGL->m_rOriginalDamageRegion, &damage);
        }
    }

    // TODO: this is getting called with extents being 0,0,0,0 should it be?
    // potentially can save on resources.

    g_pHyprOpenGL->begin(PMONITOR, &damage);
    g_pHyprOpenGL->clear(CColor(100, 11, 11, 255));
    g_pHyprOpenGL->clearWithTex(); // will apply the hypr "wallpaper"

    g_pHyprRenderer->renderAllClientsForMonitor(PMONITOR->ID, &now);

    // if correct monitor draw hyprerror
    if (PMONITOR->ID == 0)
        g_pHyprError->draw();

    // for drawing the debug overlay
    if (PMONITOR->ID == 0 && g_pConfigManager->getInt("debug:overlay") == 1) {
        startRenderOverlay = std::chrono::high_resolution_clock::now();
        g_pDebugOverlay->draw();
        endRenderOverlay = std::chrono::high_resolution_clock::now();
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

    if (DTMODE == DAMAGE_TRACKING_NONE || DTMODE == DAMAGE_TRACKING_MONITOR)
        pixman_region32_union_rect(&frameDamage, &frameDamage, 0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y);

    wlr_output_set_damage(PMONITOR->output, &frameDamage);
    pixman_region32_fini(&frameDamage);
    pixman_region32_fini(&damage);

    wlr_output_commit(PMONITOR->output);

    wlr_output_schedule_frame(PMONITOR->output);

    if (g_pConfigManager->getInt("debug:overlay") == 1) {
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

    for (auto& m : g_pCompositor->m_lMonitors) {
        if (m.szName == OUTPUT->name) {
            pMonitor = &m;
            break;
        }
    }

    if (!pMonitor)
        return;

    // Cleanup everything. Move windows back, snap cursor, shit.
    const auto BACKUPMON = &g_pCompositor->m_lMonitors.front();
    const auto BACKUPWORKSPACE = BACKUPMON->activeWorkspace > 0 ? std::to_string(BACKUPMON->activeWorkspace) : "name:" + g_pCompositor->getWorkspaceByID(BACKUPMON->activeWorkspace)->m_szName;

    if (!BACKUPMON) {
        Debug::log(CRIT, "No monitors! Unplugged last! Exiting.");
        g_pCompositor->cleanupExit();
        return;
    }

    // snap cursor
    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, BACKUPMON->vecPosition.x + BACKUPMON->vecTransformedSize.x / 2.f, BACKUPMON->vecPosition.y + BACKUPMON->vecTransformedSize.y / 2.f);

    // move workspaces
    std::deque<CWorkspace*> wspToMove;
    for (auto& w : g_pCompositor->m_lWorkspaces) {
        if (w.m_iMonitorID == pMonitor->ID) {
            wspToMove.push_back(&w);
        }
    }

    for (auto& w : wspToMove) {
        g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
        w->startAnim(true, true, true);
    }

    pMonitor->activeWorkspace = -1;

    for (auto it = g_pCompositor->m_lWorkspaces.begin(); it != g_pCompositor->m_lWorkspaces.end(); ++it) {
        if (it->m_iMonitorID == pMonitor->ID) {
            it = g_pCompositor->m_lWorkspaces.erase(it);
        }
    }

    Debug::log(LOG, "Removed monitor %s!", pMonitor->szName.c_str());

    g_pEventManager->postEvent(SHyprIPCEvent("monitorremoved", pMonitor->szName));

    g_pCompositor->m_lMonitors.remove(*pMonitor);

    // update the pMostHzMonitor
    if (pMostHzMonitor == pMonitor) {
        int mostHz = 0;
        SMonitor* pMonitorMostHz = nullptr;

        for (auto& m : g_pCompositor->m_lMonitors) {
            if (m.refreshRate > mostHz) {
                pMonitorMostHz = &m;
                mostHz = m.refreshRate;
            }
        }

        pMostHzMonitor = pMonitorMostHz;
    }
}
