#include "Monitor.hpp"

#include "MiscFunctions.hpp"

#include "../Compositor.hpp"

int ratHandler(void* data) {
    g_pHyprRenderer->renderMonitor((CMonitor*)data);

    return 1;
}

CMonitor::CMonitor() : state(this) {
    wlr_damage_ring_init(&damage);
}

CMonitor::~CMonitor() {
    wlr_damage_ring_finish(&damage);

    hyprListener_monitorDestroy.removeCallback();
    hyprListener_monitorFrame.removeCallback();
    hyprListener_monitorStateRequest.removeCallback();
    hyprListener_monitorDamage.removeCallback();
    hyprListener_monitorNeedsFrame.removeCallback();
    hyprListener_monitorCommit.removeCallback();
    hyprListener_monitorBind.removeCallback();
}

void CMonitor::onConnect(bool noRule) {
    hyprListener_monitorDestroy.removeCallback();
    hyprListener_monitorFrame.removeCallback();
    hyprListener_monitorStateRequest.removeCallback();
    hyprListener_monitorDamage.removeCallback();
    hyprListener_monitorNeedsFrame.removeCallback();
    hyprListener_monitorCommit.removeCallback();
    hyprListener_monitorBind.removeCallback();
    hyprListener_monitorFrame.initCallback(&output->events.frame, &Events::listener_monitorFrame, this);
    hyprListener_monitorDestroy.initCallback(&output->events.destroy, &Events::listener_monitorDestroy, this);
    hyprListener_monitorStateRequest.initCallback(&output->events.request_state, &Events::listener_monitorStateRequest, this);
    hyprListener_monitorDamage.initCallback(&output->events.damage, &Events::listener_monitorDamage, this);
    hyprListener_monitorNeedsFrame.initCallback(&output->events.needs_frame, &Events::listener_monitorNeedsFrame, this);
    hyprListener_monitorCommit.initCallback(&output->events.commit, &Events::listener_monitorCommit, this);
    hyprListener_monitorBind.initCallback(&output->events.bind, &Events::listener_monitorBind, this);

    tearingState.canTear = wlr_backend_is_drm(output->backend); // tearing only works on drm

    if (m_bEnabled) {
        wlr_output_state_set_enabled(state.wlr(), true);
        state.commit();
        return;
    }

    szName = output->name;

    szDescription = output->description ? output->description : "";
    // remove comma character from description. This allow monitor specific rules to work on monitor with comma on their description
    szDescription.erase(std::remove(szDescription.begin(), szDescription.end(), ','), szDescription.end());

    // field is backwards-compatible with intended usage of `szDescription` but excludes the parenthesized DRM node name suffix
    szShortDescription =
        removeBeginEndSpacesTabs(std::format("{} {} {}", output->make ? output->make : "", output->model ? output->model : "", output->serial ? output->serial : ""));

    if (!wlr_backend_is_drm(output->backend))
        createdByUser = true; // should be true. WL, X11 and Headless backends should be addable / removable

    // get monitor rule that matches
    SMonitorRule monitorRule = g_pConfigManager->getMonitorRuleFor(*this);

    // if it's disabled, disable and ignore
    if (monitorRule.disabled) {

        wlr_output_state_set_scale(state.wlr(), 1);
        wlr_output_state_set_transform(state.wlr(), WL_OUTPUT_TRANSFORM_NORMAL);

        auto PREFSTATE = wlr_output_preferred_mode(output);

        if (!PREFSTATE) {
            wlr_output_mode* mode;

            wl_list_for_each(mode, &output->modes, link) {
                wlr_output_state_set_mode(state.wlr(), mode);

                if (!wlr_output_test_state(output, state.wlr()))
                    continue;

                PREFSTATE = mode;
                break;
            }
        }

        if (PREFSTATE)
            wlr_output_state_set_mode(state.wlr(), PREFSTATE);
        else
            Debug::log(WARN, "No mode found for disabled output {}", output->name);

        wlr_output_state_set_enabled(state.wlr(), 0);

        if (!state.commit())
            Debug::log(ERR, "Couldn't commit disabled state on output {}", output->name);

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

    std::shared_ptr<CMonitor>* thisWrapper = nullptr;

    // find the wrap
    for (auto& m : g_pCompositor->m_vRealMonitors) {
        if (m->ID == ID) {
            thisWrapper = &m;
            break;
        }
    }

    RASSERT(thisWrapper->get(), "CMonitor::onConnect: Had no wrapper???");

    if (std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& other) { return other.get() == this; }) == g_pCompositor->m_vMonitors.end())
        g_pCompositor->m_vMonitors.push_back(*thisWrapper);

    m_bEnabled = true;

    wlr_output_state_set_enabled(state.wlr(), 1);

    // set mode, also applies
    if (!noRule)
        g_pHyprRenderer->applyMonitorRule(this, &monitorRule, true);

    for (const auto& PTOUCHDEV : g_pInputManager->m_lTouchDevices) {
        if (matchesStaticSelector(PTOUCHDEV.boundOutput)) {
            Debug::log(LOG, "Binding touch device {} to output {}", PTOUCHDEV.name, szName);
            wlr_cursor_map_input_to_output(g_pCompositor->m_sWLRCursor, PTOUCHDEV.pWlrDevice, output);
        }
    }

    for (const auto& PTABLET : g_pInputManager->m_lTablets) {
        if (matchesStaticSelector(PTABLET.boundOutput)) {
            Debug::log(LOG, "Binding tablet {} to output {}", PTABLET.name, szName);
            wlr_cursor_map_input_to_output(g_pCompositor->m_sWLRCursor, PTABLET.wlrDevice, output);
        }
    }

    if (!state.commit())
        Debug::log(WARN, "wlr_output_commit_state failed in CMonitor::onCommit");

    wlr_damage_ring_set_bounds(&damage, vecTransformedSize.x, vecTransformedSize.y);

    wlr_xcursor_manager_load(g_pCompositor->m_sWLRXCursorMgr, scale);

    Debug::log(LOG, "Added new monitor with name {} at {:j0} with size {:j0}, pointer {:x}", output->name, vecPosition, vecPixelSize, (uintptr_t)output);

    setupDefaultWS(monitorRule);

    for (auto& ws : g_pCompositor->m_vWorkspaces) {
        if (ws->m_szLastMonitor == szName || g_pCompositor->m_vMonitors.size() == 1 /* avoid lost workspaces on recover */) {
            g_pCompositor->moveWorkspaceToMonitor(ws.get(), this);
            ws->startAnim(true, true, true);
            ws->m_szLastMonitor = "";
        }
    }

    scale = monitorRule.scale;
    if (scale < 0.1)
        scale = getDefaultScale();

    forceFullFrames = 3; // force 3 full frames to make sure there is no blinking due to double-buffering.
    //

    g_pEventManager->postEvent(SHyprIPCEvent{"monitoradded", szName});
    g_pEventManager->postEvent(SHyprIPCEvent{"monitoraddedv2", std::format("{},{},{}", ID, szName, szShortDescription)});
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

    g_pCompositor->scheduleFrameForMonitor(this);
}

void CMonitor::onDisconnect(bool destroy) {

    if (renderTimer) {
        wl_event_source_remove(renderTimer);
        renderTimer = nullptr;
    }

    if (!m_bEnabled || g_pCompositor->m_bIsShuttingDown)
        return;

    Debug::log(LOG, "onDisconnect called for {}", output->name);

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

    hyprListener_monitorFrame.removeCallback();
    hyprListener_monitorDamage.removeCallback();
    hyprListener_monitorNeedsFrame.removeCallback();
    hyprListener_monitorCommit.removeCallback();
    hyprListener_monitorBind.removeCallback();

    for (size_t i = 0; i < 4; ++i) {
        for (auto& ls : m_aLayerSurfaceLayers[i]) {
            if (ls->layerSurface && !ls->fadingOut)
                wlr_layer_surface_v1_destroy(ls->layerSurface);
        }
        m_aLayerSurfaceLayers[i].clear();
    }

    Debug::log(LOG, "Removed monitor {}!", szName);

    g_pEventManager->postEvent(SHyprIPCEvent{"monitorremoved", szName});
    EMIT_HOOK_EVENT("monitorRemoved", this);

    if (!BACKUPMON) {
        Debug::log(WARN, "Unplugged last monitor, entering an unsafe state. Good luck my friend.");
        g_pCompositor->enterUnsafeState();
    }

    m_bEnabled             = false;
    m_bRenderingInitPassed = false;

    if (BACKUPMON) {
        // snap cursor
        wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, BACKUPMON->vecPosition.x + BACKUPMON->vecTransformedSize.x / 2.f,
                        BACKUPMON->vecPosition.y + BACKUPMON->vecTransformedSize.y / 2.f);

        // move workspaces
        std::deque<CWorkspace*> wspToMove;
        for (auto& w : g_pCompositor->m_vWorkspaces) {
            if (w->m_iMonitorID == ID || !g_pCompositor->getMonitorFromID(w->m_iMonitorID)) {
                wspToMove.push_back(w.get());
            }
        }

        for (auto& w : wspToMove) {
            w->m_szLastMonitor = szName;
            g_pCompositor->moveWorkspaceToMonitor(w, BACKUPMON);
            w->startAnim(true, true, true);
        }
    } else {
        g_pCompositor->m_pLastFocus   = nullptr;
        g_pCompositor->m_pLastWindow  = nullptr;
        g_pCompositor->m_pLastMonitor = nullptr;
    }

    activeWorkspace = -1;

    if (!destroy)
        wlr_output_layout_remove(g_pCompositor->m_sWLROutputLayout, output);

    wlr_output_state_set_enabled(state.wlr(), false);

    if (!state.commit())
        Debug::log(WARN, "wlr_output_commit_state failed in CMonitor::onDisconnect");

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
    static auto* const PZOOMFACTOR = (Hyprlang::FLOAT* const*)g_pConfigManager->getConfigValuePtr("misc:cursor_zoom_factor");
    if (**PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == this) {
        wlr_damage_ring_add_whole(&damage);
        g_pCompositor->scheduleFrameForMonitor(this);
    } else if (wlr_damage_ring_add(&damage, rg))
        g_pCompositor->scheduleFrameForMonitor(this);
}

void CMonitor::addDamage(const CRegion* rg) {
    addDamage(const_cast<CRegion*>(rg)->pixman());
}

void CMonitor::addDamage(const CBox* box) {
    static auto* const PZOOMFACTOR = (Hyprlang::FLOAT* const*)g_pConfigManager->getConfigValuePtr("misc:cursor_zoom_factor");
    if (**PZOOMFACTOR != 1.f && g_pCompositor->getMonitorFromCursor() == this) {
        wlr_damage_ring_add_whole(&damage);
        g_pCompositor->scheduleFrameForMonitor(this);
    }

    if (wlr_damage_ring_add_box(&damage, const_cast<CBox*>(box)->pWlr()))
        g_pCompositor->scheduleFrameForMonitor(this);
}

bool CMonitor::isMirror() {
    return pMirrorOf != nullptr;
}

bool CMonitor::matchesStaticSelector(const std::string& selector) const {
    if (selector.starts_with("desc:")) {
        // match by description
        const auto DESCRIPTIONSELECTOR = selector.substr(5);
        const auto DESCRIPTION         = removeBeginEndSpacesTabs(szDescription.substr(0, szDescription.find_first_of('(')));

        return DESCRIPTIONSELECTOR == szDescription || DESCRIPTIONSELECTOR == DESCRIPTION;
    } else {
        // match by selector
        return szName == selector;
    }
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

    if (WORKSPACEID == WORKSPACE_INVALID || (WORKSPACEID >= SPECIAL_WORKSPACE_START && WORKSPACEID <= -2)) {
        WORKSPACEID             = g_pCompositor->m_vWorkspaces.size() + 1;
        newDefaultWorkspaceName = std::to_string(WORKSPACEID);

        Debug::log(LOG, "Invalid workspace= directive name in monitor parsing, workspace name \"{}\" is invalid.", g_pConfigManager->getDefaultWorkspaceFor(szName));
    }

    auto PNEWWORKSPACE = g_pCompositor->getWorkspaceByID(WORKSPACEID);

    Debug::log(LOG, "New monitor: WORKSPACEID {}, exists: {}", WORKSPACEID, (int)(PNEWWORKSPACE != nullptr));

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

        PNEWWORKSPACE->m_iID = WORKSPACEID;
    }

    activeWorkspace = PNEWWORKSPACE->m_iID;

    PNEWWORKSPACE->setActive(true);
    PNEWWORKSPACE->m_szLastMonitor = "";
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
        const auto RULE = g_pConfigManager->getMonitorRuleFor(*this);

        vecPosition = RULE.offset;

        // push to mvmonitors

        std::shared_ptr<CMonitor>* thisWrapper = nullptr;

        // find the wrap
        for (auto& m : g_pCompositor->m_vRealMonitors) {
            if (m->ID == ID) {
                thisWrapper = &m;
                break;
            }
        }

        RASSERT(thisWrapper->get(), "CMonitor::setMirror: Had no wrapper???");

        if (std::find_if(g_pCompositor->m_vMonitors.begin(), g_pCompositor->m_vMonitors.end(), [&](auto& other) { return other.get() == this; }) ==
            g_pCompositor->m_vMonitors.end()) {
            g_pCompositor->m_vMonitors.push_back(*thisWrapper);
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

        g_pCompositor->arrangeMonitors();

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

void CMonitor::changeWorkspace(CWorkspace* const pWorkspace, bool internal, bool noMouseMove, bool noFocus, std::optional<bool> animateLeftOverride) {
    if (!pWorkspace)
        return;

    if (pWorkspace->m_bIsSpecialWorkspace) {
        if (specialWorkspaceID != pWorkspace->m_iID) {
            Debug::log(LOG, "changeworkspace on special, togglespecialworkspace to id {}", pWorkspace->m_iID);
            g_pKeybindManager->m_mDispatchers["togglespecialworkspace"](pWorkspace->m_szName == "special" ? "" : pWorkspace->m_szName);
        }
        return;
    }

    if (pWorkspace->m_iID == activeWorkspace)
        return;

    const auto POLDWORKSPACE = g_pCompositor->getWorkspaceByID(activeWorkspace);

    activeWorkspace = pWorkspace->m_iID;

    if (!internal) {
        const auto ANIMTOLEFT = animateLeftOverride.value_or(pWorkspace->m_iID > POLDWORKSPACE->m_iID);
        POLDWORKSPACE->startAnim(false, ANIMTOLEFT);
        pWorkspace->startAnim(true, ANIMTOLEFT);

        // move pinned windows
        for (auto& w : g_pCompositor->m_vWindows) {
            if (w->m_iWorkspaceID == POLDWORKSPACE->m_iID && w->m_bPinned) {
                w->m_iWorkspaceID = pWorkspace->m_iID;
            }
        }

        if (!noFocus && !g_pCompositor->m_pLastMonitor->specialWorkspaceID) {
            static auto* const PFOLLOWMOUSE = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("input:follow_mouse");
            CWindow*           pWindow      = pWorkspace->getLastFocusedWindow();

            if (!pWindow) {
                if (**PFOLLOWMOUSE == 1)
                    pWindow = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), RESERVED_EXTENTS | INPUT_EXTENTS | ALLOW_FLOATING);

                if (!pWindow)
                    pWindow = g_pCompositor->getTopLeftWindowOnWorkspace(pWorkspace->m_iID);

                if (!pWindow)
                    pWindow = g_pCompositor->getFirstWindowOnWorkspace(pWorkspace->m_iID);
            }

            g_pCompositor->focusWindow(pWindow);
        }

        if (!noMouseMove)
            g_pInputManager->simulateMouseMovement();

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

        g_pEventManager->postEvent(SHyprIPCEvent{"workspace", pWorkspace->m_szName});
        EMIT_HOOK_EVENT("workspace", pWorkspace);
    }

    g_pHyprRenderer->damageMonitor(this);

    g_pCompositor->updateFullscreenFadeOnWorkspace(pWorkspace);

    g_pConfigManager->ensureVRR(this);

    g_pCompositor->updateSuspendedStates();
}

void CMonitor::changeWorkspace(const int& id, bool internal, bool noMouseMove, bool noFocus) {
    changeWorkspace(g_pCompositor->getWorkspaceByID(id), internal, noMouseMove, noFocus);
}

void CMonitor::setSpecialWorkspace(CWorkspace* const pWorkspace) {
    g_pHyprRenderer->damageMonitor(this);

    if (!pWorkspace) {
        // remove special if exists
        if (const auto EXISTINGSPECIAL = g_pCompositor->getWorkspaceByID(specialWorkspaceID); EXISTINGSPECIAL) {
            EXISTINGSPECIAL->startAnim(false, false);
            g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + szName});
        }
        specialWorkspaceID = 0;

        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

        const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(activeWorkspace);
        if (const auto PLAST = PWORKSPACE->getLastFocusedWindow(); PLAST)
            g_pCompositor->focusWindow(PLAST);
        else
            g_pInputManager->refocus();

        g_pCompositor->updateSuspendedStates();

        return;
    }

    if (specialWorkspaceID) {
        if (const auto EXISTINGSPECIAL = g_pCompositor->getWorkspaceByID(specialWorkspaceID); EXISTINGSPECIAL)
            EXISTINGSPECIAL->startAnim(false, false);
    }

    bool animate = true;
    //close if open elsewhere
    const auto PMONITORWORKSPACEOWNER = g_pCompositor->getMonitorFromID(pWorkspace->m_iMonitorID);
    if (PMONITORWORKSPACEOWNER->specialWorkspaceID == pWorkspace->m_iID) {
        PMONITORWORKSPACEOWNER->specialWorkspaceID = 0;
        g_pLayoutManager->getCurrentLayout()->recalculateMonitor(PMONITORWORKSPACEOWNER->ID);
        g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", "," + PMONITORWORKSPACEOWNER->szName});
        animate = false;
    }

    // open special
    pWorkspace->m_iMonitorID = ID;
    specialWorkspaceID       = pWorkspace->m_iID;
    if (animate)
        pWorkspace->startAnim(true, true);

    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID == pWorkspace->m_iID) {
            w->m_iMonitorID = ID;
            w->updateSurfaceScaleTransformDetails();

            const auto MIDDLE = w->middle();
            if (w->m_bIsFloating && !VECINRECT(MIDDLE, vecPosition.x, vecPosition.y, vecPosition.x + vecSize.x, vecPosition.y + vecSize.y) && w->m_iX11Type != 2) {
                // if it's floating and the middle isnt on the current mon, move it to the center
                const auto PMONFROMMIDDLE = g_pCompositor->getMonitorFromVector(MIDDLE);
                Vector2D   pos            = w->m_vRealPosition.goalv();
                if (!VECINRECT(MIDDLE, PMONFROMMIDDLE->vecPosition.x, PMONFROMMIDDLE->vecPosition.y, PMONFROMMIDDLE->vecPosition.x + PMONFROMMIDDLE->vecSize.x,
                               PMONFROMMIDDLE->vecPosition.y + PMONFROMMIDDLE->vecSize.y)) {
                    // not on any monitor, center
                    pos = middle() / 2.f - w->m_vRealSize.goalv() / 2.f;
                } else
                    pos = pos - PMONFROMMIDDLE->vecPosition + vecPosition;

                w->m_vRealPosition = pos;
                w->m_vPosition     = pos;
            }
        }
    }

    g_pLayoutManager->getCurrentLayout()->recalculateMonitor(ID);

    if (const auto PLAST = pWorkspace->getLastFocusedWindow(); PLAST)
        g_pCompositor->focusWindow(PLAST);
    else
        g_pInputManager->refocus();

    g_pEventManager->postEvent(SHyprIPCEvent{"activespecial", pWorkspace->m_szName + "," + szName});

    g_pHyprRenderer->damageMonitor(this);

    g_pCompositor->updateSuspendedStates();
}

void CMonitor::setSpecialWorkspace(const int& id) {
    setSpecialWorkspace(g_pCompositor->getWorkspaceByID(id));
}

void CMonitor::moveTo(const Vector2D& pos) {
    vecPosition = pos;

    if (!isMirror())
        wlr_output_layout_add(g_pCompositor->m_sWLROutputLayout, output, (int)vecPosition.x, (int)vecPosition.y);
}

Vector2D CMonitor::middle() {
    return vecPosition + vecSize / 2.f;
}

void CMonitor::updateMatrix() {
    wlr_matrix_identity(projMatrix.data());
    if (transform != WL_OUTPUT_TRANSFORM_NORMAL) {
        wlr_matrix_translate(projMatrix.data(), vecPixelSize.x / 2.0, vecPixelSize.y / 2.0);
        wlr_matrix_transform(projMatrix.data(), transform);
        wlr_matrix_translate(projMatrix.data(), -vecTransformedSize.x / 2.0, -vecTransformedSize.y / 2.0);
    }
}

CMonitorState::CMonitorState(CMonitor* owner) {
    m_pOwner = owner;
    wlr_output_state_init(&m_state);
}

CMonitorState::~CMonitorState() {
    wlr_output_state_finish(&m_state);
}

wlr_output_state* CMonitorState::wlr() {
    return &m_state;
}

void CMonitorState::clear() {
    wlr_output_state_finish(&m_state);
    m_state = {0};
    wlr_output_state_init(&m_state);
}

bool CMonitorState::commit() {
    bool ret = wlr_output_commit_state(m_pOwner->output, &m_state);
    clear();
    return ret;
}

bool CMonitorState::test() {
    return wlr_output_test_state(m_pOwner->output, &m_state);
}
