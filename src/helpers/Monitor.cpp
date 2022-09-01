#include "Monitor.hpp"

#include "../Compositor.hpp"

void CMonitor::onConnect(bool noRule) {
    if (m_bEnabled)
        return;

    szName = output->name;

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(output->name);

    hyprListener_monitorFrame.initCallback(&output->events.frame, &Events::listener_monitorFrame, this);
    hyprListener_monitorDestroy.initCallback(&output->events.destroy, &Events::listener_monitorDestroy, this);

    // if it's disabled, disable and ignore
    if (monitorRule.disabled) {

        wlr_output_enable_adaptive_sync(output, 1);
        wlr_output_set_scale(output, 1);
        wlr_output_set_transform(output, WL_OUTPUT_TRANSFORM_NORMAL);

        auto PREFSTATE = wlr_output_preferred_mode(output);

        if (!PREFSTATE) {
            wlr_output_mode* mode;

            wl_list_for_each(mode, &output->modes, link) {
                wlr_output_set_mode(output, PREFSTATE);

                if (!wlr_output_test(output))
                    continue;
                
                PREFSTATE = mode;
                break;
            }
        }

        if (PREFSTATE)
            wlr_output_set_mode(output, PREFSTATE);
        else
            Debug::log(WARN, "No mode found for disabled output %s", output->name);

        wlr_output_enable(output, 0);
        
        if (!wlr_output_commit(output)) {
            Debug::log(ERR, "Couldn't commit disabled state on output %s", output->name);
        }

        Events::listener_change(nullptr, nullptr);

        m_bEnabled = false;

        hyprListener_monitorFrame.removeCallback();
        return;
    }

    if (output->non_desktop) {
        Debug::log(LOG, "Not configuring non-desktop output");
		if (g_pCompositor->m_sWRLDRMLeaseMgr) {
			wlr_drm_lease_v1_manager_offer_output(g_pCompositor->m_sWRLDRMLeaseMgr, output);
		}
		return;
	}

    if (!m_bRenderingInitPassed) {
        wlr_output_init_render(output, g_pCompositor->m_sWLRAllocator, g_pCompositor->m_sWLRRenderer);
        m_bRenderingInitPassed = true;
    }

    if (!m_pThisWrap) {

        // find the wrap
        for (auto& m : g_pCompositor->m_vRealMonitors) {
            if (m->ID == ID) {
                m_pThisWrap = &m;
                break;
            }
        }
    }

    if (std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_vMonitors.end()){
        g_pCompositor->m_vMonitors.push_back(*m_pThisWrap);
    }
    
    m_bEnabled = true;

    wlr_output_set_scale(output, monitorRule.scale);
    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, monitorRule.scale);
    wlr_output_set_transform(output, WL_OUTPUT_TRANSFORM_NORMAL);  // TODO: support other transforms

    // create it in the arr
    vecPosition = monitorRule.offset;
    vecSize = monitorRule.resolution;
    refreshRate = monitorRule.refreshRate;

    wlr_output_enable(output, 1);

    // TODO: this doesn't seem to set the X and Y correctly,
    // wlr_output_layout_output_coords returns invalid values, I think...
    wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, output, monitorRule.offset.x, monitorRule.offset.y);

    // set mode, also applies
    if (!noRule)
        g_pHyprRenderer->applyMonitorRule(this, &monitorRule, true);

    Debug::log(LOG, "Added new monitor with name %s at %i,%i with size %ix%i, pointer %x", output->name, (int)vecPosition.x, (int)vecPosition.y, (int)vecPixelSize.x, (int)vecPixelSize.y, output);

    damage = wlr_output_damage_create(output);

    // add a WLR workspace group
    if (!pWLRWorkspaceGroupHandle) {
        pWLRWorkspaceGroupHandle = wlr_ext_workspace_group_handle_v1_create(g_pCompositor->m_sWLREXTWorkspaceMgr);
    }
    
    wlr_ext_workspace_group_handle_v1_output_enter(pWLRWorkspaceGroupHandle, output);

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
        g_pCompositor->moveWorkspaceToMonitor(PNEWWORKSPACE, this);
        activeWorkspace = PNEWWORKSPACE->m_iID;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);
        PNEWWORKSPACE->startAnim(true, true, true);
    } else {

        if (newDefaultWorkspaceName == "")
            newDefaultWorkspaceName = std::to_string(WORKSPACEID);

        PNEWWORKSPACE = g_pCompositor->m_vWorkspaces.emplace_back(std::make_unique<CWorkspace>(ID, newDefaultWorkspaceName)).get();

        // We are required to set the name here immediately
        wlr_ext_workspace_handle_v1_set_name(PNEWWORKSPACE->m_pWlrHandle, newDefaultWorkspaceName.c_str());

        PNEWWORKSPACE->m_iID = WORKSPACEID;
    }

    activeWorkspace = PNEWWORKSPACE->m_iID;
    scale = monitorRule.scale;

    m_pThisWrap = nullptr;

    forceFullFrames = 3;  // force 3 full frames to make sure there is no blinking due to double-buffering.

    g_pCompositor->deactivateAllWLRWorkspaces(PNEWWORKSPACE->m_pWlrHandle);
    PNEWWORKSPACE->setActive(true);
    //

    if (!g_pCompositor->m_pLastMonitor)  // set the last monitor if it isnt set yet
        g_pCompositor->m_pLastMonitor = this;

    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, scale);

    g_pHyprRenderer->arrangeLayersForMonitor(ID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

    g_pEventManager->postEvent(SHyprIPCEvent{"monitoradded", szName});
}

void CMonitor::onDisconnect() {

    if (!m_bEnabled)
        return;

    // Cleanup everything. Move windows back, snap cursor, shit.
    CMonitor* BACKUPMON = nullptr;
    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m.get() != this) {
            BACKUPMON = m.get();
            break;
        }
    }

    m_bEnabled = false;
    m_bRenderingInitPassed = false;

    hyprListener_monitorFrame.removeCallback();

    if (!BACKUPMON) {
        Debug::log(WARN, "Unplugged last monitor, entering an unsafe state. Good luck my friend.");

        hyprListener_monitorMode.removeCallback();
        hyprListener_monitorDestroy.removeCallback();

        g_pCompositor->m_bUnsafeState = true;

        return;
    }

    const auto BACKUPWORKSPACE = BACKUPMON->activeWorkspace > 0 ? std::to_string(BACKUPMON->activeWorkspace) : "name:" + g_pCompositor->getWorkspaceByID(BACKUPMON->activeWorkspace)->m_szName;

    // snap cursor
    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, g_pCompositor->m_sSeat.mouse->mouse, BACKUPMON->vecPosition.x + BACKUPMON->vecTransformedSize.x / 2.f, BACKUPMON->vecPosition.y + BACKUPMON->vecTransformedSize.y / 2.f);

    // move workspaces
    std::deque<CWorkspace*> wspToMove;
    for (auto& w : g_pCompositor->m_vWorkspaces) {
        if (w->m_iMonitorID == ID) {
            wspToMove.push_back(w.get());
        }
    }

    for (auto& w : wspToMove) {
        g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
        w->startAnim(true, true, true);
    }

    activeWorkspace = -1;

    wlr_output_damage_destroy(damage);

    wlr_output_layout_remove(g_pCompositor->m_sWLROutputLayout, output);

    wlr_output_enable(output, false);

    wlr_output_commit(output);

    g_pCompositor->m_vWorkspaces.erase(std::remove_if(g_pCompositor->m_vWorkspaces.begin(), g_pCompositor->m_vWorkspaces.end(), [&](std::unique_ptr<CWorkspace>& el) { return el->m_iMonitorID == ID; }));

    Debug::log(LOG, "Removed monitor %s!", szName.c_str());

    g_pEventManager->postEvent(SHyprIPCEvent{"monitorremoved", szName});

    g_pCompositor->m_vMonitors.erase(std::remove_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](std::shared_ptr<CMonitor>& el) { return el.get() == this; }));
}

void CMonitor::addDamage(pixman_region32_t* rg) {
    wlr_output_damage_add(damage, rg);
}

void CMonitor::addDamage(wlr_box* box) {
    wlr_output_damage_add_box(damage, box);
}
