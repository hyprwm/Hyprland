#include "Monitor.hpp"

#include "../Compositor.hpp"

int ratHandler(void* data) {
    g_pHyprRenderer->renderMonitor((CMonitor*)data);

    return 1;
}

CMonitor::CMonitor() {
    wlr_damage_ring_init(&damage);
}

CMonitor::~CMonitor() {
    wlr_damage_ring_finish(&damage);
}

void CMonitor::onConnect(bool noRule) {
    hyprListener_monitorDestroy.removeCallback();
    hyprListener_monitorFrame.removeCallback();
    hyprListener_monitorStateRequest.removeCallback();
    hyprListener_monitorDamage.removeCallback();
    hyprListener_monitorNeedsFrame.removeCallback();
    hyprListener_monitorFrame.initCallback(&output->events.frame, &Events::listener_monitorFrame, this);
    hyprListener_monitorDestroy.initCallback(&output->events.destroy, &Events::listener_monitorDestroy, this);
    hyprListener_monitorStateRequest.initCallback(&output->events.request_state, &Events::listener_monitorStateRequest, this);
    hyprListener_monitorDamage.initCallback(&output->events.damage, &Events::listener_monitorDamage, this);
    hyprListener_monitorNeedsFrame.initCallback(&output->events.needs_frame, &Events::listener_monitorNeedsFrame, this);

    if (m_bEnabled) {
        wlr_output_enable(output, 1);
        wlr_output_commit(output);
        return;
    }

    szName = output->name;

    if (!wlr_backend_is_drm(output->backend))
        createdByUser = true; // should be true. WL, X11 and Headless backends should be addable / removable

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(output->name, output->description ? output->description : "");

    // if it's disabled, disable and ignore
    if (monitorRule.disabled) {

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
        output->allocator = nullptr;
        output->renderer  = nullptr;
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

    if (std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_vMonitors.end()) {
        g_pCompositor->m_vMonitors.push_back(*m_pThisWrap);
    }

    m_bEnabled = true;

    // create it in the arr
    vecPosition = monitorRule.offset;
    vecSize     = monitorRule.resolution;
    refreshRate = monitorRule.refreshRate;

    wlr_output_enable(output, 1);

    // set mode, also applies
    if (!noRule)
        g_pHyprRenderer->applyMonitorRule(this, &monitorRule, true);

    wlr_damage_ring_set_bounds(&damage, vecTransformedSize.x, vecTransformedSize.y);

    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, scale);

    Debug::log(LOG, "Added new monitor with name %s at %i,%i with size %ix%i, pointer %x", output->name, (int)vecPosition.x, (int)vecPosition.y, (int)vecPixelSize.x,
               (int)vecPixelSize.y, output);

    // add a WLR workspace group
    if (!pWLRWorkspaceGroupHandle) {
        pWLRWorkspaceGroupHandle = wlr_ext_workspace_group_handle_v1_create(g_pCompositor->m_sWLREXTWorkspaceMgr);
    }

    wlr_ext_workspace_group_handle_v1_output_enter(pWLRWorkspaceGroupHandle, output);

    setupDefaultWS(monitorRule);

    scale = monitorRule.scale;
    if (scale < 0.1)
        scale = getDefaultScale();

    m_pThisWrap = nullptr;

    forceFullFrames = 3; // force 3 full frames to make sure there is no blinking due to double-buffering.
    //

    g_pEventManager->postEvent(SHyprIPCEvent{"monitoradded", szName});
    EMIT_HOOK_EVENT("monitorAdded", this);

    if (!g_pCompositor->m_pLastMonitor) // set the last monitor if it isnt set yet
        g_pCompositor->setActiveMonitor(this);

    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, scale);

    g_pHyprRenderer->arrangeLayersForMonitor(ID);
    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

    // ensure VRR (will enable if necessary)
    g_pConfigManager->ensureVRR(this);

    // verify last mon valid
    bool found = false;
    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m.get() == g_pCompositor->m_pLastMonitor) {
            found = true;
            break;
        }
    }

    if (!found)
        g_pCompositor->setActiveMonitor(this);

    renderTimer = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, ratHandler, this);
}

void CMonitor::onDisconnect() {

    if (renderTimer) {
        wl_event_source_remove(renderTimer);
        renderTimer = nullptr;
    }

    if (!m_bEnabled || g_pCompositor->m_bIsShuttingDown)
        return;

    Debug::log(LOG, "onDisconnect called for %s", output->name);

    // Cleanup everything. Move windows back, snap cursor, shit.
    CMonitor* BACKUPMON = nullptr;
    for (auto& m : g_pCompositor->m_vMonitors) {
        if (m.get() != this) {
            BACKUPMON = m.get();
            break;
        }
    }

    // remove mirror
    if (pMirrorOf) {
        pMirrorOf->mirrors.erase(std::find_if(pMirrorOf->mirrors.begin(), pMirrorOf->mirrors.end(), [&](const auto& other) { return other == this; }));
        pMirrorOf = nullptr;
    }

    if (!mirrors.empty()) {
        for (auto& m : mirrors) {
            m->setMirror("");
        }

        g_pConfigManager->m_bWantsMonitorReload = true;
    }

    m_bEnabled             = false;
    m_bRenderingInitPassed = false;

    hyprListener_monitorFrame.removeCallback();
    hyprListener_monitorDamage.removeCallback();
    hyprListener_monitorNeedsFrame.removeCallback();

    for (size_t i = 0; i < 4; ++i) {
        for (auto& ls : m_aLayerSurfaceLayers[i]) {
            if (ls->layerSurface && !ls->fadingOut)
                wlr_layer_surface_v1_destroy(ls->layerSurface);
        }
        m_aLayerSurfaceLayers[i].clear();
    }

    Debug::log(LOG, "Removed monitor %s!", szName.c_str());

    g_pEventManager->postEvent(SHyprIPCEvent{"monitorremoved", szName});
    EMIT_HOOK_EVENT("monitorRemoved", this);

    if (!BACKUPMON) {
        Debug::log(WARN, "Unplugged last monitor, entering an unsafe state. Good luck my friend.");

        hyprListener_monitorStateRequest.removeCallback();
        hyprListener_monitorDestroy.removeCallback();

        g_pCompositor->m_bUnsafeState = true;

        std::erase_if(g_pCompositor->m_vMonitors, [&](std::shared_ptr<CMonitor>& el) { return el.get() == this; });

        return;
    }

    // snap cursor
    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, BACKUPMON->vecPosition.x + BACKUPMON->vecTransformedSize.x / 2.f,
                    BACKUPMON->vecPosition.y + BACKUPMON->vecTransformedSize.y / 2.f);

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

    wlr_output_layout_remove(g_pCompositor->m_sWLROutputLayout, output);

    wlr_output_enable(output, false);

    wlr_output_commit(output);

    std::erase_if(g_pCompositor->m_vWorkspaces, [&](std::unique_ptr<CWorkspace>& el) { return el->m_iMonitorID == ID; });

    if (g_pCompositor->m_pLastMonitor == this)
        g_pCompositor->setActiveMonitor(BACKUPMON);

    if (g_pHyprRenderer->m_pMostHzMonitor == this) {
        int       mostHz         = 0;
        CMonitor* pMonitorMostHz = nullptr;

        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->refreshRate > mostHz && m.get() != this) {
                pMonitorMostHz = m.get();
                mostHz         = m->refreshRate;
            }
        }

        g_pHyprRenderer->m_pMostHzMonitor = pMonitorMostHz;
    }

    std::erase_if(g_pCompositor->m_vMonitors, [&](std::shared_ptr<CMonitor>& el) { return el.get() == this; });
}

void CMonitor::addDamage(const pixman_region32_t* rg) {
    if (wlr_damage_ring_add(&damage, rg))
        g_pCompositor->scheduleFrameForMonitor(this);
}

void CMonitor::addDamage(const wlr_box* box) {
    if (wlr_damage_ring_add_box(&damage, box))
        g_pCompositor->scheduleFrameForMonitor(this);
}

bool CMonitor::isMirror() {
    return pMirrorOf != nullptr;
}

int CMonitor::findAvailableDefaultWS() {
    for (size_t i = 1; i < INT32_MAX; ++i) {
        if (g_pCompositor->getWorkspaceByID(i))
            continue;

        if (const auto BOUND = g_pConfigManager->getBoundMonitorStringForWS(std::to_string(i)); !BOUND.empty() && BOUND != szName)
            continue;

        return i;
    }

    return INT32_MAX; // shouldn't be reachable
}

void CMonitor::setupDefaultWS(const SMonitorRule& monitorRule) {
    // Workspace
    std::string newDefaultWorkspaceName = "";
    int64_t     WORKSPACEID             = g_pConfigManager->getDefaultWorkspaceFor(szName).empty() ?
                        findAvailableDefaultWS() :
                        getWorkspaceIDFromString(g_pConfigManager->getDefaultWorkspaceFor(szName), newDefaultWorkspaceName);

    if (WORKSPACEID == INT_MAX || (WORKSPACEID >= SPECIAL_WORKSPACE_START && WORKSPACEID <= -2)) {
        WORKSPACEID             = g_pCompositor->m_vWorkspaces.size() + 1;
        newDefaultWorkspaceName = std::to_string(WORKSPACEID);

        Debug::log(LOG, "Invalid workspace= directive name in monitor parsing, workspace name \"%s\" is invalid.", g_pConfigManager->getDefaultWorkspaceFor(szName).c_str());
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

    g_pCompositor->deactivateAllWLRWorkspaces(PNEWWORKSPACE->m_pWlrHandle);
    PNEWWORKSPACE->setActive(true);
}

void CMonitor::setMirror(const std::string& mirrorOf) {
    const auto PMIRRORMON = g_pCompositor->getMonitorFromString(mirrorOf);

    if (PMIRRORMON == pMirrorOf)
        return;

    if (PMIRRORMON && PMIRRORMON->isMirror()) {
        Debug::log(ERR, "Cannot mirror a mirror!");
        return;
    }

    if (PMIRRORMON == this) {
        Debug::log(ERR, "Cannot mirror self!");
        return;
    }

    if (!PMIRRORMON) {
        // disable mirroring

        if (pMirrorOf) {
            pMirrorOf->mirrors.erase(std::find_if(pMirrorOf->mirrors.begin(), pMirrorOf->mirrors.end(), [&](const auto& other) { return other == this; }));
        }

        pMirrorOf = nullptr;

        // set rule
        const auto RULE = g_pConfigManager->getMonitorRuleFor(this->szName, this->output->description ? this->output->description : "");

        vecPosition = RULE.offset;

        // push to mvmonitors
        if (!m_pThisWrap) {
            // find the wrap
            for (auto& m : g_pCompositor->m_vRealMonitors) {
                if (m->ID == ID) {
                    m_pThisWrap = &m;
                    break;
                }
            }
        }

        if (std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& other) { return other.get() == this; }) ==
            g_pCompositor->m_vMonitors.end()) {
            g_pCompositor->m_vMonitors.push_back(*m_pThisWrap);
        }

        setupDefaultWS(RULE);

        g_pHyprRenderer->applyMonitorRule(this, (SMonitorRule*)&RULE, true); // will apply the offset and stuff
    } else {
        CMonitor* BACKUPMON = nullptr;
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m.get() != this) {
                BACKUPMON = m.get();
                break;
            }
        }

        // move all the WS
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

        wlr_output_layout_remove(g_pCompositor->m_sWLROutputLayout, output);

        vecPosition = PMIRRORMON->vecPosition;

        pMirrorOf = PMIRRORMON;

        pMirrorOf->mirrors.push_back(this);

        // remove from mvmonitors
        std::erase_if(g_pCompositor->m_vMonitors, [&](const auto& other) { return other.get() == this; });

        g_pCompositor->setActiveMonitor(g_pCompositor->m_vMonitors.front().get());

        g_pCompositor->sanityCheckWorkspaces();
    }
}

float CMonitor::getDefaultScale() {
    if (!m_bEnabled)
        return 1;

    static constexpr double MMPERINCH = 25.4;

    const auto              DIAGONALPX = sqrt(pow(vecPixelSize.x, 2) + pow(vecPixelSize.y, 2));
    const auto              DIAGONALIN = sqrt(pow(output->phys_width / MMPERINCH, 2) + pow(output->phys_height / MMPERINCH, 2));

    const auto              PPI = DIAGONALPX / DIAGONALIN;

    if (PPI > 200 /* High PPI, 2x*/)
        return 2;
    else if (PPI > 140 /* Medium PPI, 1.5x*/)
        return 1.5;
    return 1;
}
