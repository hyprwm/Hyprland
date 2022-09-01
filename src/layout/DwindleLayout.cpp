#include "DwindleLayout.hpp"
#include "../Compositor.hpp"

void SDwindleNodeData::recalcSizePosRecursive(bool force) {
    if (children[0]) {

        const auto REVERSESPLITRATIO = 2.f - splitRatio;

        static auto *const PPRESERVESPLIT = &g_pConfigManager->getConfigValuePtr("dwindle:preserve_split")->intValue;
        static auto *const PFLMULT = &g_pConfigManager->getConfigValuePtr("dwindle:split_width_multiplier")->floatValue;

        if (*PPRESERVESPLIT == 0) {
            splitTop = size.y * *PFLMULT > size.x;
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

bool SDwindleNodeData::isGroupMember() {
    return pNextGroupMember && pNextGroupMember != this;
}

SDwindleNodeData* SDwindleNodeData::getGroupHead() {
    SDwindleNodeData* current = this->pNextGroupMember;

    while (current != this) {
        if (current->groupHead) {
            return current;
        }

        current = current->pNextGroupMember;
    }

    this->groupHead = true;

    return this;
}

SDwindleNodeData* SDwindleNodeData::getGroupVisible() {
    SDwindleNodeData* current = this->pNextGroupMember;

    while (current != this) {
        if (!current->pWindow->m_bHidden) {
            return current;
        }

        current = current->pNextGroupMember;
    }

    return this;
}

void SDwindleNodeData::setGroupFocusedNode(SDwindleNodeData* pMember) {
    SDwindleNodeData* current = this->pNextGroupMember;

    while (current != this) {
        current->pWindow->m_bHidden = current != pMember;
        current = current->pNextGroupMember;
    }

    this->pWindow->m_bHidden = pMember != this;
}

int SDwindleNodeData::getGroupMemberCount() {
    SDwindleNodeData* current = this->pNextGroupMember;

    int no = 1;

    while (current != this) {
        current = current->pNextGroupMember;
        no++;
    }

    return no;
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

    if (!g_pCompositor->windowExists(PWINDOW) || !PWINDOW->m_bIsMapped) {
        Debug::log(ERR, "Node %x holding invalid window %x!!", pNode, PWINDOW);
        onWindowRemovedTiling(PWINDOW);
        return;
    }

    PWINDOW->m_vSize = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    static auto *const PNOGAPSWHENONLY = &g_pConfigManager->getConfigValuePtr("dwindle:no_gaps_when_only")->intValue;

    auto calcPos = PWINDOW->m_vPosition + Vector2D(BORDERSIZE, BORDERSIZE);
    auto calcSize = PWINDOW->m_vSize - Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE);

    const auto NODESONWORKSPACE = getNodesOnWorkspace(PWINDOW->m_iWorkspaceID);

    if (*PNOGAPSWHENONLY && PWINDOW->m_iWorkspaceID != SPECIAL_WORKSPACE_ID && (NODESONWORKSPACE == 1 || (pNode->isGroupMember() && pNode->getGroupMemberCount() == NODESONWORKSPACE))) {
        PWINDOW->m_vRealPosition = calcPos - Vector2D(BORDERSIZE, BORDERSIZE);
        PWINDOW->m_vRealSize = calcSize + Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE);

        PWINDOW->updateWindowDecos();

        PWINDOW->m_sSpecialRenderData.rounding = false;
        PWINDOW->m_sSpecialRenderData.border = false;

        return;
    }

    PWINDOW->m_sSpecialRenderData.rounding = true;
    PWINDOW->m_sSpecialRenderData.border = true;

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
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    if (pNode->isGroupMember() && pNode->groupHead) {
        // update visible node
        const auto PVISNODE = pNode->getGroupVisible();

        PVISNODE->pWindow->m_vRealSize = PWINDOW->m_vRealSize.goalv();
        PVISNODE->pWindow->m_vRealPosition = PWINDOW->m_vRealPosition.goalv();
    }

    PWINDOW->updateWindowDecos();
}

void CHyprDwindleLayout::onWindowCreatedTiling(CWindow* pWindow) {
    if (pWindow->m_bIsFloating)
        return;

    m_lDwindleNodesData.push_back(SDwindleNodeData());
    const auto PNODE = &m_lDwindleNodesData.back();

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    static auto *const PUSEACTIVE = &g_pConfigManager->getConfigValuePtr("dwindle:use_active_for_splits")->intValue;

    // Populate the node with our window's data
    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow = pWindow;
    PNODE->isNode = false;
    PNODE->layout = this;

    SDwindleNodeData* OPENINGON;
    const auto MONFROMCURSOR = g_pCompositor->getMonitorFromCursor();

    if (PMONITOR->ID == MONFROMCURSOR->ID && (PNODE->workspaceID == PMONITOR->activeWorkspace || (PNODE->workspaceID == SPECIAL_WORKSPACE_ID && PMONITOR->specialWorkspaceOpen)) && !*PUSEACTIVE) {
        OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowTiled(g_pInputManager->getMouseCoordsInternal()));

        // happens on reserved area
        if (!OPENINGON && g_pCompositor->getWindowsOnWorkspace(PNODE->workspaceID) > 0)
            OPENINGON = getFirstNodeOnWorkspace(PMONITOR->activeWorkspace);
            
    } else if (*PUSEACTIVE) {
        if (g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow) && !g_pCompositor->m_pLastWindow->m_bIsFloating && g_pCompositor->m_pLastWindow != pWindow && g_pCompositor->m_pLastWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID && g_pCompositor->m_pLastWindow->m_bIsMapped) {
            OPENINGON = getNodeFromWindow(g_pCompositor->m_pLastWindow);
        } else {
            OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowTiled(g_pInputManager->getMouseCoordsInternal()));
        }

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

    // if it's a group, add the window
    if (OPENINGON->isGroupMember()) {
        const auto PHEAD = OPENINGON->getGroupHead();

        const auto PTAIL = PHEAD->pPreviousGroupMember;

        PHEAD->pPreviousGroupMember = PNODE;
        PTAIL->pNextGroupMember = PNODE;

        PNODE->pNextGroupMember = PHEAD;
        PNODE->pPreviousGroupMember = PTAIL;

        PHEAD->setGroupFocusedNode(PNODE);

        PNODE->position = PHEAD->position;
        PNODE->size = PHEAD->size;

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

    NEWPARENT->recalcSizePosRecursive();

    applyNodeDataToWindow(PNODE);
    applyNodeDataToWindow(OPENINGON);
}

void CHyprDwindleLayout::onWindowRemovedTiling(CWindow* pWindow) {

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE) {
        Debug::log(ERR, "onWindowRemovedTiling node null?");
        return;
    }

    // check if it was grouped
    if (PNODE->isGroupMember()) {
        // get shit
        const auto PPREV = PNODE->pPreviousGroupMember;
        const auto PNEXT = PNODE->pNextGroupMember;

        PPREV->pNextGroupMember = PNEXT;
        PNEXT->pPreviousGroupMember = PPREV;

        if (PNODE->groupHead) {
            PNEXT->groupHead = true;
            PNEXT->pParent = PNODE->pParent;
            
            if (PNODE->pParent) {
                if (PNODE->pParent->children[0] == PNODE) {
                    PNODE->pParent->children[0] = PNEXT;
                } else {
                    PNODE->pParent->children[1] = PNEXT;
                }
            }

            PNEXT->position = PNODE->position;
            PNEXT->size = PNODE->size;

            applyNodeDataToWindow(PNEXT);
        } else {
            const auto PHEAD = PNODE->getGroupHead();

            PNEXT->position = PHEAD->position;
            PNEXT->size = PHEAD->size;

            applyNodeDataToWindow(PNEXT);
        }

        PNEXT->setGroupFocusedNode(PNEXT);
        PNEXT->pWindow->m_bHidden = false;

        m_lDwindleNodesData.remove(*PNODE);
        return;
    }

    const auto PPARENT = PNODE->pParent;

    if (!PPARENT) {
        Debug::log(LOG, "Removing last node (dwindle)");
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

    if (!PMONITOR)
        return; // ???

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

        if (!PFULLWINDOW) { // ????
            PWORKSPACE->m_bHasFullscreenWindow = false;
        } else {
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

    const auto PANIMATE = &g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes")->intValue;

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
            PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
        } else {
            allowedMovement.y *= 2.f / PPARENT->size.y;
            PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, (double)0.1f, (double)1.9f);
            PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
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
            PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
        } else {
            allowedMovement.y *= 2.f / PPARENT->size.y;
            PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, (double)0.1f, (double)1.9f);
            PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
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
    SIDECONTAINER->recalcSizePosRecursive(*PANIMATE == 0);
    TOPCONTAINER->recalcSizePosRecursive(*PANIMATE == 0);
}

void CHyprDwindleLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (!g_pCompositor->isWorkspaceVisible(pWindow->m_iWorkspaceID))
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

    recalculateMonitor(PMONITOR->ID);
}

void CHyprDwindleLayout::recalculateWindow(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    PNODE->recalcSizePosRecursive();
}

void addToDequeRecursive(std::deque<SDwindleNodeData*>* pDeque, std::deque<SDwindleNodeData*>* pParents, SDwindleNodeData* node) {
    if (node->isNode) {
        pParents->push_back(node);
        addToDequeRecursive(pDeque, pParents, node->children[0]);
        addToDequeRecursive(pDeque, pParents, node->children[1]);
    } else {
        pDeque->emplace_back(node);
    }
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

    if (PNODE->isGroupMember()) {
        // dissolve group

        const auto PHEAD = PNODE->getGroupHead();

        SDwindleNodeData* current = PNODE->pNextGroupMember;

        PNODE->pWindow->m_bIsFloating = PHEAD->pWindow->m_bIsFloating;

        std::deque<CWindow*> toAddWindows;
        
        const auto PWINDOWNODE = PNODE->pWindow;
        toAddWindows.push_back(PWINDOWNODE);

        while (current != PNODE) {
            const auto PWINDOW = current->pWindow;
            current = current->pNextGroupMember;

            toAddWindows.push_back(PWINDOW);

            PWINDOW->m_bHidden = false;
        }

        PHEAD->pPreviousGroupMember = nullptr;
        PHEAD->pNextGroupMember = nullptr;
        onWindowRemoved(PHEAD->pWindow);

        for (auto& pw : toAddWindows) {
            const auto PNODE = getNodeFromWindow(pw);
            if (PNODE)
                m_lDwindleNodesData.remove(*PNODE);

            pw->m_vPosition = Vector2D(-1000000, -1000000);
        }

        for (auto& pw : toAddWindows) {
            onWindowCreated(pw);
            pw->removeDecorationByType(DECORATION_GROUPBAR);
        }

        recalculateMonitor(PWORKSPACE->m_iMonitorID);
    } else {
        // create group

        if (!PNODE->pParent)
            return;

        PNODE->groupHead = true;

        std::deque<SDwindleNodeData*> newGroupMembers;
        std::deque<SDwindleNodeData*> nodesToRemove;

        newGroupMembers.emplace_back(PNODE);

        addToDequeRecursive(&newGroupMembers, &nodesToRemove, PNODE->pParent->children[0] == PNODE ? PNODE->pParent->children[1] : PNODE->pParent->children[0]);

        for (auto& n : newGroupMembers) {
            if (n->isGroupMember())
                return; // reject nested groups
        }

        for (auto& nd : nodesToRemove) {
            m_lDwindleNodesData.remove(*nd);
        }

        PNODE->position = PNODE->pParent->position;
        PNODE->size = PNODE->pParent->size;

        applyNodeDataToWindow(PNODE);

        if (PNODE->pParent->pParent) {
            if (PNODE->pParent->pParent->children[0] == PNODE->pParent) {
                PNODE->pParent->pParent->children[0] = PNODE;
            } else {
                PNODE->pParent->pParent->children[1] = PNODE;
            }
        }

        const auto PPARENT2 = PNODE->pParent->pParent;

        m_lDwindleNodesData.remove(*PNODE->pParent);

        PNODE->pParent = PPARENT2;

        // now remove everyone but head from tree
        // and set order
        for (int i = 0; i < (int)newGroupMembers.size(); ++i) {
            if (i != 0) {
                newGroupMembers[i]->groupHead = false;
                newGroupMembers[i]->pParent = PNODE->pParent;
            }

            const auto PREVMEMBER = i == 0 ? newGroupMembers[newGroupMembers.size() - 1] : newGroupMembers[i - 1];
            const auto NEXTMEMBER = i == (int)newGroupMembers.size() - 1 ? newGroupMembers[0] : newGroupMembers[i + 1];

            newGroupMembers[i]->pPreviousGroupMember = PREVMEMBER;
            newGroupMembers[i]->pNextGroupMember = NEXTMEMBER;

            // add the deco
            newGroupMembers[i]->pWindow->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(newGroupMembers[i]->pWindow));
        }

        // focus
        PNODE->setGroupFocusedNode(PNODE);

        // required for no_gaps_when_only to work
        applyNodeDataToWindow(PNODE);
    }

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    g_pInputManager->refocus();
}

std::deque<CWindow*> CHyprDwindleLayout::getGroupMembers(CWindow* pWindow) {
    
    std::deque<CWindow*> result;

    if (!g_pCompositor->windowExists(pWindow))
        return result; // reject with empty

    // get the node
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->isGroupMember())
        return result;  // reject with empty

    const auto HEAD = PNODE->getGroupHead();
    SDwindleNodeData* current = HEAD->pNextGroupMember;

    result.push_back(HEAD->pWindow);

    while (current != HEAD) {
        result.push_back(current->pWindow);
        current = current->pNextGroupMember;
    }

    return result;
}

void CHyprDwindleLayout::switchGroupWindow(CWindow* pWindow, bool forward) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return; // reject

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->isGroupMember())
        return; // reject

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PNODE->workspaceID);

    SDwindleNodeData* pNewNode;

    if (forward)
        pNewNode = PNODE->pNextGroupMember;
    else
        pNewNode = PNODE->pPreviousGroupMember;

    PNODE->setGroupFocusedNode(pNewNode);

    pNewNode->position = PNODE->position;
    pNewNode->size = PNODE->size;

    applyNodeDataToWindow(pNewNode);

    pNewNode->pWindow->m_vRealSize.warp();
    pNewNode->pWindow->m_vRealPosition.warp();

    g_pCompositor->focusWindow(pNewNode->pWindow);

    pNewNode->pWindow->m_bIsFloating = PNODE->pWindow->m_bIsFloating;
    
    if (PNODE->pWindow->m_bIsFullscreen) {
        PNODE->pWindow->m_bHidden = false;
        g_pCompositor->setWindowFullscreen(PNODE->pWindow, false, PWORKSPACE->m_efFullscreenMode);
        PNODE->pWindow->m_bHidden = true;
        g_pCompositor->setWindowFullscreen(pNewNode->pWindow, true, PWORKSPACE->m_efFullscreenMode);

        pNewNode->pWindow->m_vRealSize.warp();
        pNewNode->pWindow->m_vRealPosition.warp();
    }

    pNewNode->pWindow->updateWindowDecos();
    PNODE->pWindow->updateWindowDecos();
}

SWindowRenderLayoutHints CHyprDwindleLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    const auto PNODE = getNodeFromWindow(pWindow);
    if (!PNODE)
        return hints; // left for the future, maybe floating funkiness

    if (PNODE->isGroupMember()) {
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
