#include "MasterLayout.hpp"
#include "../Compositor.hpp"

SMasterNodeData* CHyprMasterLayout::getNodeFromWindow(CWindow* pWindow) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd.pWindow == pWindow)
            return &nd;
    }

    return nullptr;
}

int CHyprMasterLayout::getNodesOnWorkspace(const int& ws) {
    int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n.workspaceID == ws)
            no++;
    }

    return no;
}

std::string CHyprMasterLayout::getLayoutName() {
    return "Master";
}

SMasterNodeData* CHyprMasterLayout::getMasterNodeOnWorkspace(const int& ws) {
    for (auto& n : m_lMasterNodesData) {
        if (n.isMaster && n.workspaceID == ws)
            return &n;
    }

    return nullptr;
}

void CHyprMasterLayout::onWindowCreatedTiling(CWindow* pWindow) {
    if (pWindow->m_bIsFloating)
        return;

    static auto *const PNEWTOP = &g_pConfigManager->getConfigValuePtr("master:new_on_top")->intValue;

    const auto PNODE = *PNEWTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();

    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow = pWindow;

    static auto *const PNEWISMASTER = &g_pConfigManager->getConfigValuePtr("master:new_is_master")->intValue;

    const auto WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    float lastSplitPercent = 0.5f;

    if (*PNEWISMASTER || WINDOWSONWORKSPACE == 1) {
        for (auto& nd : m_lMasterNodesData) {
            if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                nd.isMaster = false;
                lastSplitPercent = nd.percMaster;
                break;
            }
        }

        PNODE->isMaster = true;
        PNODE->percMaster = lastSplitPercent;
    } else {
        PNODE->isMaster = false;
    }

    // recalc
    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprMasterLayout::onWindowRemovedTiling(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    if (PNODE->isMaster) {
        // find new one
        for (auto& nd : m_lMasterNodesData) {
            if (!nd.isMaster) {
                nd.isMaster = true;
                break;
            }
        }
    }

    m_lMasterNodesData.remove(*PNODE);

    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprMasterLayout::recalculateMonitor(const int& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->specialWorkspaceOpen) {
        calculateWorkspace(SPECIAL_WORKSPACE_ID);
    }

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) 
            return;

        // massive hack from the fullscreen func
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        SMasterNodeData fakeNode;
        fakeNode.pWindow = PFULLWINDOW;
        fakeNode.position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        fakeNode.size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        fakeNode.workspaceID = PWORKSPACE->m_iID;
        PFULLWINDOW->m_vPosition = fakeNode.position;
        PFULLWINDOW->m_vSize = fakeNode.size;

        applyNodeDataToWindow(&fakeNode);

        return;
    }

    // calc the WS
    calculateWorkspace(PWORKSPACE->m_iID);
}

void CHyprMasterLayout::calculateWorkspace(const int& ws) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(ws);

    if (!PWORKSPACE)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);

    const auto PMASTERNODE = getMasterNodeOnWorkspace(PWORKSPACE->m_iID);

    if (!PMASTERNODE)
        return;

    if (getNodesOnWorkspace(PWORKSPACE->m_iID) < 2) {
        PMASTERNODE->position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
        PMASTERNODE->size = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
        applyNodeDataToWindow(PMASTERNODE);
        return;
    } else {
        PMASTERNODE->position = PMONITOR->vecReservedTopLeft + PMONITOR->vecPosition;
        PMASTERNODE->size = Vector2D((PMONITOR->vecSize.x - PMONITOR->vecReservedTopLeft.x - PMONITOR->vecReservedBottomRight.x) * PMASTERNODE->percMaster, PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y - PMONITOR->vecReservedTopLeft.y);
    }

    const auto SLAVESIZE = 1.f / (getNodesOnWorkspace(PWORKSPACE->m_iID) - 1) * (PMASTERNODE->size.y);
    int slavesDone = 0;

    for (auto& nd : m_lMasterNodesData) {
        if (nd.workspaceID != PWORKSPACE->m_iID)
            continue;

        if (nd == *PMASTERNODE) {
            applyNodeDataToWindow(PMASTERNODE);
            continue;
        }

        nd.position = Vector2D(PMASTERNODE->size.x + PMASTERNODE->position.x, slavesDone * SLAVESIZE + PMASTERNODE->position.y);
        nd.size = Vector2D(PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x - PMONITOR->vecReservedTopLeft.x - PMASTERNODE->size.x, SLAVESIZE);

        slavesDone++;

        applyNodeDataToWindow(&nd);
    }
}

void CHyprMasterLayout::applyNodeDataToWindow(SMasterNodeData* pNode) {
    SMonitor* PMONITOR = nullptr;

    if (pNode->workspaceID == SPECIAL_WORKSPACE_ID) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->specialWorkspaceOpen) {
                PMONITOR = m.get();
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_iMonitorID);
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node %x (workspace ID: %i)!!", pNode, pNode->workspaceID);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT          = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT         = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP           = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM        = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto BORDERSIZE           = g_pConfigManager->getInt("general:border_size");
    const auto GAPSIN               = g_pConfigManager->getInt("general:gaps_in");
    const auto GAPSOUT              = g_pConfigManager->getInt("general:gaps_out");

    const auto PWINDOW = pNode->pWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW)) {
        Debug::log(ERR, "Node %x holding invalid window %x!!", pNode, PWINDOW);
        return;
    }

    PWINDOW->m_vSize = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    // TODO: special

    auto calcPos = PWINDOW->m_vPosition + Vector2D(BORDERSIZE, BORDERSIZE);
    auto calcSize = PWINDOW->m_vSize - Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE);

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? GAPSOUT : GAPSIN,
                                        DISPLAYTOP ? GAPSOUT : GAPSIN);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? GAPSOUT : GAPSIN,
                                            DISPLAYBOTTOM ? GAPSOUT : GAPSIN);

    calcPos = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    if (PWINDOW->m_iWorkspaceID == SPECIAL_WORKSPACE_ID) {
        static auto *const PSCALEFACTOR = &g_pConfigManager->getConfigValuePtr("master:special_scale_factor")->floatValue;

        PWINDOW->m_vRealPosition = calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f;
        PWINDOW->m_vRealSize = calcSize * *PSCALEFACTOR;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize * *PSCALEFACTOR);
    } else {
        PWINDOW->m_vRealSize = calcSize;
        PWINDOW->m_vRealPosition = calcPos;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize);
    }

    PWINDOW->updateWindowDecos();
}

bool CHyprMasterLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprMasterLayout::resizeActiveWindow(const Vector2D& pixResize, CWindow* pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        PWINDOW->m_vRealSize = Vector2D(std::clamp((PWINDOW->m_vRealSize.goalv() + pixResize).x, (double)20, (double)999999), std::clamp((PWINDOW->m_vRealSize.goalv() + pixResize).y, (double)20, (double)999999));
        PWINDOW->updateWindowDecos();
        return;
    }

    // get master
    const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);
    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    if (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) < 2)
        return;

    float delta = pixResize.x / PMONITOR->vecSize.x;

    PMASTER->percMaster += delta;

    std::clamp(PMASTER->percMaster, 0.05f, 0.95f);

    recalculateMonitor(PMONITOR->ID);
}

void CHyprMasterLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen)
        return;  // ignore

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen = on;
    PWORKSPACE->m_bHasFullscreenWindow = !PWORKSPACE->m_bHasFullscreenWindow;

    g_pEventManager->postEvent(SHyprIPCEvent{"fullscreen", std::to_string((int)on)});

    if (!pWindow->m_bIsFullscreen) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            pWindow->m_vRealPosition = pWindow->m_vPosition;
            pWindow->m_vRealSize = pWindow->m_vSize;
        }
    } else {
        // if it now got fullscreen, make it fullscreen

        PWORKSPACE->m_efFullscreenMode = fullscreenMode;

        // save position and size if floating
        if (pWindow->m_bIsFloating) {
            pWindow->m_vPosition = pWindow->m_vRealPosition.vec();
            pWindow->m_vSize = pWindow->m_vRealSize.vec();
        }

        // apply new pos and size being monitors' box
        if (fullscreenMode == FULLSCREEN_FULL) {
            pWindow->m_vRealPosition = PMONITOR->vecPosition;
            pWindow->m_vRealSize = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SMasterNodeData fakeNode;
            fakeNode.pWindow = pWindow;
            fakeNode.position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID = pWindow->m_iWorkspaceID;
            pWindow->m_vPosition = fakeNode.position;
            pWindow->m_vSize = fakeNode.size;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv());

    g_pCompositor->moveWindowToTop(pWindow);

    // we need to fix XWayland windows by sending them to NARNIA
    // because otherwise they'd still be recieving mouse events
    g_pCompositor->fixXWaylandWindowsOnWorkspace(PMONITOR->activeWorkspace);

    recalculateMonitor(PMONITOR->ID);
}

void CHyprMasterLayout::recalculateWindow(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->m_iMonitorID);
}

SWindowRenderLayoutHints CHyprMasterLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesnt have any hints
}

void CHyprMasterLayout::switchWindows(CWindow* pWindow, CWindow* pWindow2) {
    // windows should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        Debug::log(ERR, "Master: Rejecting a swap between workspaces");
        return;
    }

    // massive hack: just swap window pointers, lol
    const auto PWINDOW1 = PNODE->pWindow;
    PNODE->pWindow = PNODE2->pWindow;
    PNODE2->pWindow = PWINDOW1;

    recalculateMonitor(PWINDOW1->m_iMonitorID);
}

void CHyprMasterLayout::alterSplitRatioBy(CWindow* pWindow, float ratio) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PMASTER = getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);

    PMASTER->percMaster = std::clamp(PMASTER->percMaster + ratio, 0.05f, 0.95f);

    recalculateMonitor(pWindow->m_iMonitorID);
}

std::any CHyprMasterLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    return "";
}

void CHyprMasterLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bMappedX11 || !w->m_bIsMapped || w->m_bHidden)
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CHyprMasterLayout::onDisable() {
    m_lMasterNodesData.clear();
}