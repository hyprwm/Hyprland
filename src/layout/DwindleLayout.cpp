#include "DwindleLayout.hpp"
#include "../Compositor.hpp"

void SDwindleNodeData::recalcSizePosRecursive(bool force) {

    // check the group, if we are in one and not active, ignore.
    if (pGroupParent && pGroupParent->groupMembers[pGroupParent->groupMemberActive] != this) {
        if (pWindow)
            pWindow->m_bHidden = true;
        return;
    } else {
        if (pWindow)
            pWindow->m_bHidden = false;
    }

    if (pGroupParent) {
        // means we are in a group and focused. let's just act like the full window in this
        size = pGroupParent->size;
        position = pGroupParent->position;
    }

    if (children[0]) {

        const auto REVERSESPLITRATIO = 2.f - splitRatio;

        if (g_pConfigManager->getInt("dwindle:preserve_split") == 0) {
            const auto WIDTHMULTIPLIER = g_pConfigManager->getFloat("dwindle:split_width_multiplier");
            splitTop = size.y * WIDTHMULTIPLIER > size.x;
        }

        const auto SPLITSIDE = !splitTop;

        if (SPLITSIDE) {
            // split left/right
            children[0]->position = position;
            children[0]->size = Vector2D(size.x / 2.f * splitRatio, size.y);
            children[1]->position = Vector2D(position.x + size.x / 2.f * splitRatio, position.y);
            children[1]->size = Vector2D(size.x / 2.f * REVERSESPLITRATIO, size.y);
        } else {
            // split top/bottom
            children[0]->position = position;
            children[0]->size = Vector2D(size.x, size.y / 2.f * splitRatio);
            children[1]->position = Vector2D(position.x, position.y + size.y / 2.f * splitRatio);
            children[1]->size = Vector2D(size.x, size.y / 2.f * REVERSESPLITRATIO);
        }

        children[0]->recalcSizePosRecursive(force);
        children[1]->recalcSizePosRecursive(force);
    } else {
        layout->applyNodeDataToWindow(this, force);
    }
}

void SDwindleNodeData::getAllChildrenRecursive(std::deque<SDwindleNodeData*>* pDeque) {
    if (children[0]) {
        children[0]->getAllChildrenRecursive(pDeque);
        children[1]->getAllChildrenRecursive(pDeque);
    } else {
        pDeque->push_back(this);
    }
}

int CHyprDwindleLayout::getNodesOnWorkspace(const int& id) {
    int no = 0;
    for (auto& n : m_lDwindleNodesData) {
        if (n.workspaceID == id && n.valid)
            ++no;
    }
    return no;
}

SDwindleNodeData* CHyprDwindleLayout::getFirstNodeOnWorkspace(const int& id) {
    for (auto& n : m_lDwindleNodesData) {
        if (n.workspaceID == id && n.pWindow && g_pCompositor->windowValidMapped(n.pWindow))
            return &n;
    }
    return nullptr;
}

SDwindleNodeData* CHyprDwindleLayout::getNodeFromWindow(CWindow* pWindow) {
    for (auto& n : m_lDwindleNodesData) {
        if (n.pWindow == pWindow && !n.isNode)
            return &n;
    }

    return nullptr;
}

SDwindleNodeData* CHyprDwindleLayout::getMasterNodeOnWorkspace(const int& id) {
    for (auto& n : m_lDwindleNodesData) {
        if (!n.pParent && n.workspaceID == id)
            return &n;
    }
    return nullptr;
}

void CHyprDwindleLayout::applyNodeDataToWindow(SDwindleNodeData* pNode, bool force) {
    // Don't set nodes, only windows.
    if (pNode->isNode) 
        return;

    CMonitor* PMONITOR = nullptr;

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

    static auto *const PNOGAPSWHENONLY = &g_pConfigManager->getConfigValuePtr("dwindle:no_gaps_when_only")->intValue;

    auto calcPos = PWINDOW->m_vPosition + Vector2D(BORDERSIZE, BORDERSIZE);
    auto calcSize = PWINDOW->m_vSize - Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE);

    if (*PNOGAPSWHENONLY && PWINDOW->m_iWorkspaceID != SPECIAL_WORKSPACE_ID && getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) == 1) {
        PWINDOW->m_vRealPosition = calcPos - Vector2D(BORDERSIZE, BORDERSIZE);
        PWINDOW->m_vRealSize = calcSize + Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE);

        PWINDOW->updateWindowDecos();

        PWINDOW->m_sSpecialRenderData.rounding = false;

        return;
    }

    PWINDOW->m_sSpecialRenderData.rounding = true;

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? GAPSOUT : GAPSIN,
                                        DISPLAYTOP ? GAPSOUT : GAPSIN);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? GAPSOUT : GAPSIN,
                                            DISPLAYBOTTOM ? GAPSOUT : GAPSIN);

    calcPos = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    if (PWINDOW->m_bIsPseudotiled) {
        // Calculate pseudo
        float scale = 1;

        // adjust if doesnt fit
        if (PWINDOW->m_vPseudoSize.x > calcSize.x || PWINDOW->m_vPseudoSize.y > calcSize.y) {
            if (PWINDOW->m_vPseudoSize.x > calcSize.x) {
                scale = calcSize.x / PWINDOW->m_vPseudoSize.x;
            }

            if (PWINDOW->m_vPseudoSize.y * scale > calcSize.y) {
                scale = calcSize.y / PWINDOW->m_vPseudoSize.y;
            }

            auto DELTA = calcSize - PWINDOW->m_vPseudoSize * scale;
            calcSize = PWINDOW->m_vPseudoSize * scale;
            calcPos = calcPos + DELTA / 2.f;  // center
        } else {
            auto DELTA = calcSize - PWINDOW->m_vPseudoSize;
            calcPos = calcPos + DELTA / 2.f;  // center
            calcSize = PWINDOW->m_vPseudoSize;
        }
    }

    if (PWINDOW->m_iWorkspaceID == SPECIAL_WORKSPACE_ID) {
        // if special, we adjust the coords a bit
        static auto *const PSCALEFACTOR = &g_pConfigManager->getConfigValuePtr("dwindle:special_scale_factor")->floatValue;

        PWINDOW->m_vRealPosition = calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f;
        PWINDOW->m_vRealSize = calcSize * *PSCALEFACTOR;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize * *PSCALEFACTOR);
    } else {
        PWINDOW->m_vRealSize = calcSize;
        PWINDOW->m_vRealPosition = calcPos;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize);
    }

    if (force) {
        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();
    }

    PWINDOW->updateWindowDecos();
}

void CHyprDwindleLayout::onWindowCreatedTiling(CWindow* pWindow) {
    if (pWindow->m_bIsFloating)
        return;

    m_lDwindleNodesData.push_back(SDwindleNodeData());
    const auto PNODE = &m_lDwindleNodesData.back();

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    // Populate the node with our window's data
    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow = pWindow;
    PNODE->isNode = false;
    PNODE->layout = this;

    SDwindleNodeData* OPENINGON;
    const auto MONFROMCURSOR = g_pCompositor->getMonitorFromCursor();

    if (PMONITOR->ID == MONFROMCURSOR->ID && (PNODE->workspaceID == PMONITOR->activeWorkspace || (PNODE->workspaceID == SPECIAL_WORKSPACE_ID && PMONITOR->specialWorkspaceOpen))) {
        OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowTiled(g_pInputManager->getMouseCoordsInternal()));

        // happens on reserved area
        if (!OPENINGON && g_pCompositor->getWindowsOnWorkspace(PNODE->workspaceID) > 0)
            OPENINGON = getFirstNodeOnWorkspace(PMONITOR->activeWorkspace);
            
    } else
        OPENINGON = getFirstNodeOnWorkspace(pWindow->m_iWorkspaceID);

    Debug::log(LOG, "OPENINGON: %x, Workspace: %i, Monitor: %i", OPENINGON, PNODE->workspaceID, PMONITOR->ID);

    if (OPENINGON && OPENINGON->workspaceID != PNODE->workspaceID) {
        // special workspace handling
        OPENINGON = getFirstNodeOnWorkspace(PNODE->workspaceID);
    }

    // first, check if OPENINGON isn't too big.
    const auto PREDSIZEMAX = OPENINGON ? Vector2D(OPENINGON->size.x, OPENINGON->size.y) : PMONITOR->vecSize;
    if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow); MAXSIZE.x < PREDSIZEMAX.x || MAXSIZE.y < PREDSIZEMAX.y) {
        // we can't continue. make it floating.
        pWindow->m_bIsFloating = true;
        m_lDwindleNodesData.remove(*PNODE);
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
        return;
    }

    // if it's the first, it's easy. Make it fullscreen.
    if (!OPENINGON || OPENINGON->pWindow == pWindow) {
        PNODE->position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        PNODE->size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;

        applyNodeDataToWindow(PNODE);

        return;
    }
    
    // If it's not, get the node under our cursor

    m_lDwindleNodesData.push_back(SDwindleNodeData());
    const auto NEWPARENT = &m_lDwindleNodesData.back();

    // make the parent have the OPENINGON's stats
    NEWPARENT->position = OPENINGON->position;
    NEWPARENT->size = OPENINGON->size;
    NEWPARENT->workspaceID = OPENINGON->workspaceID;
    NEWPARENT->pParent = OPENINGON->pParent;
    NEWPARENT->isNode = true; // it is a node

    const auto WIDTHMULTIPLIER = g_pConfigManager->getFloat("dwindle:split_width_multiplier");

    // if cursor over first child, make it first, etc
    const auto SIDEBYSIDE = NEWPARENT->size.x > NEWPARENT->size.y * WIDTHMULTIPLIER;
    NEWPARENT->splitTop = !SIDEBYSIDE;

    const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();

    const auto FORCESPLIT = g_pConfigManager->getInt("dwindle:force_split");

    if (FORCESPLIT == 0) {
        if ((SIDEBYSIDE && VECINRECT(MOUSECOORDS, NEWPARENT->position.x, NEWPARENT->position.y / WIDTHMULTIPLIER, NEWPARENT->position.x + NEWPARENT->size.x / 2.f, NEWPARENT->position.y + NEWPARENT->size.y))
        || (!SIDEBYSIDE && VECINRECT(MOUSECOORDS, NEWPARENT->position.x, NEWPARENT->position.y / WIDTHMULTIPLIER, NEWPARENT->position.x + NEWPARENT->size.x, NEWPARENT->position.y + NEWPARENT->size.y / 2.f))) {
            // we are hovering over the first node, make PNODE first.
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            // we are hovering over the second node, make PNODE second.
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }
    } else {
        if (FORCESPLIT == 1) {
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }
    }
    
    // and update the previous parent if it exists
    if (OPENINGON->pParent) {
        if (OPENINGON->pParent->children[0] == OPENINGON) {
            OPENINGON->pParent->children[0] = NEWPARENT;
        } else {
            OPENINGON->pParent->children[1] = NEWPARENT;
        }
    }

    // Update the children
    

    if (NEWPARENT->size.x * WIDTHMULTIPLIER > NEWPARENT->size.y) {
        // split left/right
        OPENINGON->position = NEWPARENT->position;
        OPENINGON->size = Vector2D(NEWPARENT->size.x / 2.f, NEWPARENT->size.y);
        PNODE->position = Vector2D(NEWPARENT->position.x + NEWPARENT->size.x / 2.f, NEWPARENT->position.y);
        PNODE->size = Vector2D(NEWPARENT->size.x / 2.f, NEWPARENT->size.y);
    } else {
        // split top/bottom
        OPENINGON->position = NEWPARENT->position;
        OPENINGON->size = Vector2D(NEWPARENT->size.x, NEWPARENT->size.y / 2.f);
        PNODE->position = Vector2D(NEWPARENT->position.x, NEWPARENT->position.y + NEWPARENT->size.y / 2.f);
        PNODE->size = Vector2D(NEWPARENT->size.x, NEWPARENT->size.y / 2.f);
    }

    OPENINGON->pParent = NEWPARENT;
    PNODE->pParent = NEWPARENT;

    if (OPENINGON->pGroupParent) {
        // means we opened on a group

        // add the group deco
        pWindow->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(pWindow));

        PNODE->pGroupParent = OPENINGON->pGroupParent;
        PNODE->pGroupParent->groupMembers.push_back(PNODE);
        PNODE->pGroupParent->groupMemberActive = PNODE->pGroupParent->groupMembers.size() - 1;

        PNODE->pGroupParent->recalcSizePosRecursive();
    } else {
        NEWPARENT->recalcSizePosRecursive();

        applyNodeDataToWindow(PNODE);
        applyNodeDataToWindow(OPENINGON);
    }
}

void CHyprDwindleLayout::onWindowRemovedTiling(CWindow* pWindow) {

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PPARENT = PNODE->pParent;

    if (!PPARENT) {
        m_lDwindleNodesData.remove(*PNODE);
        return;
    }

    const auto PSIBLING = PPARENT->children[0] == PNODE ? PPARENT->children[1] : PPARENT->children[0];

    PSIBLING->position = PPARENT->position;
    PSIBLING->size = PPARENT->size;
    PSIBLING->pParent = PPARENT->pParent;

    if (PPARENT->pParent != nullptr) {
        if (PPARENT->pParent->children[0] == PPARENT) {
            PPARENT->pParent->children[0] = PSIBLING;
        } else {
            PPARENT->pParent->children[1] = PSIBLING;
        }
    }

    // check if it was grouped
    if (PNODE->pGroupParent) {
        PNODE->pGroupParent->groupMembers.erase(PNODE->pGroupParent->groupMembers.begin() + PNODE->pGroupParent->groupMemberActive);

        if ((long unsigned int)PNODE->pGroupParent->groupMemberActive >= PNODE->pGroupParent->groupMembers.size())
            PNODE->pGroupParent->groupMemberActive = 0;

        if (PNODE->pGroupParent->groupMembers.size() <= 1) {
            PNODE->pGroupParent->isGroup = false;
            PSIBLING->pGroupParent = nullptr;
            PNODE->pGroupParent->groupMembers.clear();

            PSIBLING->recalcSizePosRecursive();
        } else {
            PNODE->pGroupParent->recalcSizePosRecursive();
        }

        // if the parent is to be removed, remove the group
        if (PPARENT == PNODE->pGroupParent) {
            toggleWindowGroup(PPARENT->groupMembers[PPARENT->groupMemberActive]->pWindow);
        }
    }

    PPARENT->valid = false;
    PNODE->valid = false;

    if (PSIBLING->pParent)
        PSIBLING->pParent->recalcSizePosRecursive();
    else 
        PSIBLING->recalcSizePosRecursive();

    m_lDwindleNodesData.remove(*PPARENT);
    m_lDwindleNodesData.remove(*PNODE);
}

void CHyprDwindleLayout::recalculateMonitor(const int& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->specialWorkspaceOpen) {
        const auto TOPNODE = getMasterNodeOnWorkspace(SPECIAL_WORKSPACE_ID);

        if (TOPNODE && PMONITOR) {
            TOPNODE->position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            TOPNODE->size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            TOPNODE->recalcSizePosRecursive();
        }
    }

    // Ignore any recalc events if we have a fullscreen window, but process if fullscreen mode 2
    if (PWORKSPACE->m_bHasFullscreenWindow) {
        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL)
            return;

        // massive hack from the fullscreen func
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        SDwindleNodeData fakeNode;
        fakeNode.pWindow = PFULLWINDOW;
        fakeNode.position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        fakeNode.size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        fakeNode.workspaceID = PWORKSPACE->m_iID;
        PFULLWINDOW->m_vPosition = fakeNode.position;
        PFULLWINDOW->m_vSize = fakeNode.size;

        applyNodeDataToWindow(&fakeNode);

        return;
    }

    const auto TOPNODE = getMasterNodeOnWorkspace(PMONITOR->activeWorkspace);

    if (TOPNODE && PMONITOR) {
        TOPNODE->position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        TOPNODE->size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        TOPNODE->recalcSizePosRecursive();
    }
}

bool CHyprDwindleLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprDwindleLayout::resizeActiveWindow(const Vector2D& pixResize, CWindow* pWindow) {

    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        PWINDOW->m_vRealSize = Vector2D(std::clamp((PWINDOW->m_vRealSize.goalv() + pixResize).x, (double)20, (double)999999), std::clamp((PWINDOW->m_vRealSize.goalv() + pixResize).y, (double)20, (double)999999));
        PWINDOW->updateWindowDecos();
        return;
    }

    // get some data about our window
    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
    const bool DISPLAYLEFT = STICKS(PWINDOW->m_vPosition.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT = STICKS(PWINDOW->m_vPosition.x + PWINDOW->m_vSize.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP = STICKS(PWINDOW->m_vPosition.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(PWINDOW->m_vPosition.y + PWINDOW->m_vSize.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    // construct allowed movement
    Vector2D allowedMovement = pixResize;
    if (DISPLAYLEFT && DISPLAYRIGHT)
        allowedMovement.x = 0;

    if (DISPLAYBOTTOM && DISPLAYTOP)
        allowedMovement.y = 0;

    // get the correct containers to apply splitratio to
    const auto PPARENT = PNODE->pParent;

    if (!PPARENT)
        return;  // the only window on a workspace, ignore

    const bool PARENTSIDEBYSIDE = !PPARENT->splitTop;

    // Get the parent's parent
    auto PPARENT2 = PPARENT->pParent;

    // No parent means we have only 2 windows, and thus one axis of freedom
    if (!PPARENT2) {
        if (PARENTSIDEBYSIDE) {
            allowedMovement.x *= 2.f / PPARENT->size.x;
            PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.x, (double)0.1f, (double)1.9f);
            PPARENT->recalcSizePosRecursive(true);
        } else {
            allowedMovement.y *= 2.f / PPARENT->size.y;
            PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, (double)0.1f, (double)1.9f);
            PPARENT->recalcSizePosRecursive(true);
        }

        return;
    }

    // Get first parent with other split
    while (PPARENT2 && PPARENT2->splitTop == !PARENTSIDEBYSIDE)
        PPARENT2 = PPARENT2->pParent;

    // no parent, one axis of freedom
    if (!PPARENT2) {
        if (PARENTSIDEBYSIDE) {
            allowedMovement.x *= 2.f / PPARENT->size.x;
            PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.x, (double)0.1f, (double)1.9f);
            PPARENT->recalcSizePosRecursive(true);
        } else {
            allowedMovement.y *= 2.f / PPARENT->size.y;
            PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, (double)0.1f, (double)1.9f);
            PPARENT->recalcSizePosRecursive(true);
        }

        return;
    }

    // 2 axes of freedom
    const auto SIDECONTAINER = PARENTSIDEBYSIDE ? PPARENT : PPARENT2;
    const auto TOPCONTAINER = PARENTSIDEBYSIDE ? PPARENT2 : PPARENT;

    allowedMovement.x *= 2.f / SIDECONTAINER->size.x;
    allowedMovement.y *= 2.f / TOPCONTAINER->size.y;

    SIDECONTAINER->splitRatio = std::clamp(SIDECONTAINER->splitRatio + allowedMovement.x, (double)0.1f, (double)1.9f);
    TOPCONTAINER->splitRatio = std::clamp(TOPCONTAINER->splitRatio + allowedMovement.y, (double)0.1f, (double)1.9f);
    SIDECONTAINER->recalcSizePosRecursive(true);
    TOPCONTAINER->recalcSizePosRecursive(true);
}

void CHyprDwindleLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen)
        return; // ignore

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

            SDwindleNodeData fakeNode;
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

void CHyprDwindleLayout::recalculateWindow(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    PNODE->recalcSizePosRecursive();
}

void CHyprDwindleLayout::toggleWindowGroup(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    // get the node
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return; // reject

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PNODE->workspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow)
        fullscreenRequestForWindow(g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID), FULLSCREEN_FULL, false);

    const auto PGROUPPARENT = PNODE->pGroupParent;

    if (PGROUPPARENT) {
        // if there is a parent, release it
        const auto INACTIVEBORDERCOL = CColor(g_pConfigManager->getInt("general:col.inactive_border"));
        for (auto& node : PGROUPPARENT->groupMembers) {
            node->pGroupParent = nullptr;
            node->pWindow->m_cRealBorderColor.setValueAndWarp(INACTIVEBORDERCOL); // no anim here because they pop in

            for (auto& wd : node->pWindow->m_dWindowDecorations) {
                wd->updateWindow(node->pWindow);
            }
        }   
        
        PGROUPPARENT->groupMembers.clear();

        PGROUPPARENT->isGroup = false;

        PGROUPPARENT->recalcSizePosRecursive();

        if (g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
            g_pCompositor->m_pLastWindow->m_cRealBorderColor = CColor(g_pConfigManager->getInt("general:col.active_border"));
    } else {
        // if there is no parent, let's make one

        const auto PPARENT = PNODE->pParent;

        if (!PPARENT)
            return; // reject making group on single window

        
        PPARENT->isGroup = true;
        
        // recursively get all members
        std::deque<SDwindleNodeData*> allChildren;
        PPARENT->getAllChildrenRecursive(&allChildren);

        PPARENT->groupMembers = allChildren;

        const auto GROUPINACTIVEBORDERCOL = CColor(g_pConfigManager->getInt("dwinle:col.group_border"));
        for (auto& c : PPARENT->groupMembers) {
            c->pGroupParent = PPARENT;
            c->pWindow->m_cRealBorderColor = GROUPINACTIVEBORDERCOL;

            c->pWindow->m_dWindowDecorations.push_back(std::make_unique<CHyprGroupBarDecoration>(c->pWindow));

            if (c->pWindow == g_pCompositor->m_pLastWindow)
                c->pWindow->m_cRealBorderColor = CColor(g_pConfigManager->getInt("dwindle:col.group_border_active"));
        }

        PPARENT->groupMemberActive = 0;

        PPARENT->recalcSizePosRecursive();
    }

    g_pInputManager->refocus();
}

std::deque<CWindow*> CHyprDwindleLayout::getGroupMembers(CWindow* pWindow) {
    
    std::deque<CWindow*> result;

    if (!g_pCompositor->windowExists(pWindow))
        return result; // reject with empty

    // get the node
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return result;  // reject with empty

    const auto PGROUPPARENT = PNODE->pGroupParent;

    if (!PGROUPPARENT)
        return result;  // reject with empty

    for (auto& node : PGROUPPARENT->groupMembers) {
        result.push_back(node->pWindow);
    }

    return result;
}

void CHyprDwindleLayout::switchGroupWindow(CWindow* pWindow, bool forward) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return; // reject

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return; // reject

    if (!PNODE->pGroupParent)
        return; // reject

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PNODE->workspaceID);

    if (forward)
        PNODE->pGroupParent->groupMemberActive++;
    else
        PNODE->pGroupParent->groupMemberActive--;

    if (PNODE->pGroupParent->groupMemberActive < 0)
        PNODE->pGroupParent->groupMemberActive = PNODE->pGroupParent->groupMembers.size() - 1;

    if ((long unsigned int)PNODE->pGroupParent->groupMemberActive >= PNODE->pGroupParent->groupMembers.size())
        PNODE->pGroupParent->groupMemberActive = 0;

    bool restoreFullscreen = false;

    if (PNODE->pWindow->m_bIsFullscreen) {
        fullscreenRequestForWindow(PNODE->pWindow, PWORKSPACE->m_efFullscreenMode, false);
        restoreFullscreen = true;
    }

    PNODE->pGroupParent->recalcSizePosRecursive();

    for (auto& gm : PNODE->pGroupParent->groupMembers) {
        for (auto& deco : gm->pWindow->m_dWindowDecorations) {
            deco->updateWindow(gm->pWindow);
        }
    }

    // focus
    g_pCompositor->focusWindow(PNODE->pGroupParent->groupMembers[PNODE->pGroupParent->groupMemberActive]->pWindow);

    if (restoreFullscreen) {
         fullscreenRequestForWindow(PNODE->pGroupParent->groupMembers[PNODE->pGroupParent->groupMemberActive]->pWindow, PWORKSPACE->m_efFullscreenMode, true);
    }
}

SWindowRenderLayoutHints CHyprDwindleLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    const auto PNODE = getNodeFromWindow(pWindow);
    if (!PNODE)
        return hints; // left for the future, maybe floating funkiness

    if (PNODE->pGroupParent) {
        hints.isBorderColor = true;

        if (pWindow == g_pCompositor->m_pLastWindow)
            hints.borderColor = CColor(g_pConfigManager->getInt("dwindle:col.group_border_active"));
        else
            hints.borderColor = CColor(g_pConfigManager->getInt("dwindle:col.group_border"));
    }

    return hints;
}

void CHyprDwindleLayout::switchWindows(CWindow* pWindow, CWindow* pWindow2) {
    // windows should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        Debug::log(ERR, "Dwindle: Rejecting a swap between workspaces");
        return;
    }

    // we will not delete the nodes, just fix the tree
    if (PNODE2->pParent == PNODE->pParent) {
        const auto PPARENT = PNODE->pParent;

        if (PPARENT->children[0] == PNODE) {
            PPARENT->children[0] = PNODE2;
            PPARENT->children[1] = PNODE;
        } else {
            PPARENT->children[0] = PNODE;
            PPARENT->children[1] = PNODE2;
        }
    } else {
        if (PNODE->pParent) {
            const auto PPARENT = PNODE->pParent;

            if (PPARENT->children[0] == PNODE) {
                PPARENT->children[0] = PNODE2;
            } else {
                PPARENT->children[1] = PNODE2;
            }
        }

        if (PNODE2->pParent) {
            const auto PPARENT = PNODE2->pParent;

            if (PPARENT->children[0] == PNODE2) {
                PPARENT->children[0] = PNODE;
            } else {
                PPARENT->children[1] = PNODE;
            }
        }
    }

    const auto PPARENTNODE2 = PNODE2->pParent;
    PNODE2->pParent = PNODE->pParent;
    PNODE->pParent = PPARENTNODE2;

    // these are window nodes, so no children.

    // recalc the workspace
    getMasterNodeOnWorkspace(PNODE->workspaceID)->recalcSizePosRecursive();
}

void CHyprDwindleLayout::alterSplitRatioBy(CWindow* pWindow, float ratio) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    PNODE->pParent->splitRatio = std::clamp(PNODE->pParent->splitRatio + ratio, 0.1f, 1.9f);

    PNODE->pParent->recalcSizePosRecursive();
}

std::any CHyprDwindleLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    if (message == "togglegroup")
        toggleWindowGroup(header.pWindow);
    else if (message == "changegroupactivef")
        switchGroupWindow(header.pWindow, true);
    else if (message == "changegroupactiveb")
        switchGroupWindow(header.pWindow, false);
    else if (message == "togglesplit")
        toggleSplit(header.pWindow);
    else if (message == "groupinfo") {
        auto res = getGroupMembers(g_pCompositor->m_pLastWindow);
        return res;
    }
    
    return "";
}

void CHyprDwindleLayout::toggleSplit(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    PNODE->pParent->splitTop = !PNODE->pParent->splitTop;

    PNODE->pParent->recalcSizePosRecursive();
}

std::string CHyprDwindleLayout::getLayoutName() {
    return "dwindle";
}

void CHyprDwindleLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bMappedX11 || !w->m_bIsMapped || w->m_bHidden)
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CHyprDwindleLayout::onDisable() {
    m_lDwindleNodesData.clear();
}