#include "DwindleLayout.hpp"
#include "../Compositor.hpp"

void SDwindleNodeData::recalcSizePosRecursive(bool force, bool apply) {

    if (!isTerminal()) {

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

        const auto& applyToChildren { isNode() };
        children[0]->recalcSizePosRecursive(force, applyToChildren);
        children[1]->recalcSizePosRecursive(force, applyToChildren);
    }

    if (apply) {
        layout->applyNodeDataToWindow(this, force);
    }
}

void SDwindleNodeData::addChildrenRecursive(std::deque<SDwindleNodeData *> &pDeque, bool groupStop) {
    if (isTerminal() || (isGroup() && groupStop)) {
        pDeque.push_back(this);
    } else {
        children[0]->addChildrenRecursive(pDeque, groupStop);
        children[1]->addChildrenRecursive(pDeque, groupStop);
    }
}

std::deque<SDwindleNodeData*> SDwindleNodeData::getChildrenRecursive(bool groupStop) {
    std::deque<SDwindleNodeData*> deque;
    addChildrenRecursive(deque, groupStop);
    return deque;
}


SDwindleNodeData *SDwindleNodeData::getNextNode(SDwindleNodeData *topNode, bool forward, bool cyclic) {

    SDwindleNodeData *node{this};
    size_t check_idx{0}, ret_idx{1};
    if (!forward) {
        std::swap(check_idx, ret_idx);
    }

    do {
        if (node->pParent->children[check_idx] == node) {
            //we can go one step to the "right"
            node = node->pParent->children[ret_idx];
            break;
        } else {
            //we have to go up
            node = node->pParent;
        }
    } while (node != topNode);

    if (node == topNode && !cyclic) { //nothing found
        return nullptr;
    } //else: just stay with the topNode, it will be fine

    if (node == this) { //we found ourselfs..
       return nullptr; //.. so there is no other node
    }

    return node;
}

SDwindleNodeData *SDwindleNodeData::getNextTerminalNode(SDwindleNodeData *topNode, bool forward, bool cyclic) {
    SDwindleNodeData* node {getNextNode(topNode, forward, cyclic)};
    while (!node->isTerminal()) {
        node = node->children[forward ? 0 : 1];
    }
    return node;
}

SDwindleNodeData *SDwindleNodeData::getNextWindowNode(SDwindleNodeData *topNode, bool forward, bool cyclic) {
    SDwindleNodeData* node {getNextNode(topNode, forward, cyclic)};
    while (node->isNode()) {
        node = node->children[forward ? 0 : 1];
    }
    return node;
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
        CWindow *win = n.getWindow();
        if (n.workspaceID == id && win && g_pCompositor->windowValidMapped(win)) {
            return &n;
        }
    }
    return nullptr;
}

SDwindleNodeData * CHyprDwindleLayout::getNodeFromWindow(CWindow *pWindow, bool reportGroups) {
    for (auto&& n : m_lDwindleNodesData) {
        if (n.isTerminal() && n.getWindow() == pWindow) {
            if (reportGroups) {
                // check if a group exposes this window
                if (SDwindleNodeData* group = n.getOuterGroup()) {
                    return group;
                }
            }
            return &n;
        }
    }
    return nullptr;
}


SDwindleNodeData* SDwindleNodeData::getOuterGroup() {
    SDwindleNodeData *node {this}, *outerMostGroup { nullptr };
    while (node != nullptr) {
        if (node->isGroup()) {
            outerMostGroup = node;
        }
        node = node->pParent;
    }
    return outerMostGroup;
}

CWindow* SDwindleNodeData::getWindow() {
    return isTerminal() ? pWindow : activeGroupNode->pWindow;
}

SDwindleNodeData *SDwindleNodeData::nextInGroup(bool forward) {
   if (!isGroup())
       return nullptr;

    activeGroupNode = activeGroupNode->getNextTerminalNode(this, forward, true);
    return activeGroupNode;
}

void SDwindleNodeData::turnIntoGroup(SDwindleNodeData *activeNode) {
    activeGroupNode = activeNode;
}

SDwindleNodeData SDwindleNodeData::newTerminalNode(SDwindleNodeData *parent, CWindow *window) {
    SDwindleNodeData ret{};
    ret.pParent = parent;
    ret.pWindow = window;

    return ret;
}

SDwindleNodeData SDwindleNodeData::newNode(SDwindleNodeData *parent, std::array<SDwindleNodeData *, 2>&& children) {
    SDwindleNodeData ret{};
    ret.pParent = parent;
    ret.children = children;

    return ret;
}

void SDwindleNodeData::ungroup() {
    activeGroupNode = nullptr;
}


SDwindleNodeData* CHyprDwindleLayout::getMasterNodeOnWorkspace(const int& id) {
    for (auto& n : m_lDwindleNodesData) {
        if (!n.pParent && n.workspaceID == id)
            return &n;
    }
    return nullptr;
}

void CHyprDwindleLayout::applyNodeDataToWindow(SDwindleNodeData* pNode, bool force) {
    // Don't set nodes, only windows or groups
    if (pNode->isNode())
        return;

    const auto PWINDOW = pNode->getWindow();

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
        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

void CHyprDwindleLayout::onWindowCreatedTiling(CWindow* pWindow) {
    if (pWindow->m_bIsFloating)
        return;

    static auto *const PUSEACTIVE = &g_pConfigManager->getConfigValuePtr("dwindle:use_active_for_splits")->intValue;
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto MONFROMCURSOR = g_pCompositor->getMonitorFromCursor();

    m_lDwindleNodesData.push_back(SDwindleNodeData::newTerminalNode(nullptr, pWindow));
    const auto PNODE = &m_lDwindleNodesData.back();
    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->layout = this;

    SDwindleNodeData* OPENINGON = nullptr;
    if (PMONITOR->ID == MONFROMCURSOR->ID && (pWindow->m_iWorkspaceID == PMONITOR->activeWorkspace ||
            (pWindow->m_iWorkspaceID == SPECIAL_WORKSPACE_ID && PMONITOR->specialWorkspaceOpen)) && !*PUSEACTIVE) {
        OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowTiled(g_pInputManager->getMouseCoordsInternal()));
        // happens on reserved area
        if (!OPENINGON && g_pCompositor->getWindowsOnWorkspace(pWindow->m_iWorkspaceID) > 0)
            OPENINGON = getFirstNodeOnWorkspace(PMONITOR->activeWorkspace);
            
    } else if (*PUSEACTIVE) {
        if (g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow) && !g_pCompositor->m_pLastWindow->m_bIsFloating &&
                g_pCompositor->m_pLastWindow != pWindow && g_pCompositor->m_pLastWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID &&
                g_pCompositor->m_pLastWindow->m_bIsMapped) {
            OPENINGON = getNodeFromWindow(g_pCompositor->m_pLastWindow);
        } else {
            OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowTiled(g_pInputManager->getMouseCoordsInternal()));
        }

        if (!OPENINGON && g_pCompositor->getWindowsOnWorkspace(pWindow->m_iWorkspaceID) > 0)
            OPENINGON = getFirstNodeOnWorkspace(PMONITOR->activeWorkspace);
    } else {
        OPENINGON = getFirstNodeOnWorkspace(pWindow->m_iWorkspaceID);
    }

    Debug::log(LOG, "OPENINGON: %x, Workspace: %i, Monitor: %i", OPENINGON, pWindow->m_iWorkspaceID, PMONITOR->ID);

    if (OPENINGON && OPENINGON->workspaceID != pWindow->m_iWorkspaceID) {
        // special workspace handling
        OPENINGON = getFirstNodeOnWorkspace(pWindow->m_iWorkspaceID);
    }

    // first, check if OPENINGON isn't too big.
    const auto PREDSIZEMAX = OPENINGON ? Vector2D(OPENINGON->size.x, OPENINGON->size.y) : PMONITOR->vecSize;
    if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow); MAXSIZE.x < PREDSIZEMAX.x || MAXSIZE.y < PREDSIZEMAX.y) {
        // we can't continue. make it floating.
        pWindow->m_bIsFloating = true;
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
        return;
    }

    // if it's the first, it's easy. Make it fullscreen.
    if (!OPENINGON || OPENINGON->getWindow() == pWindow) {
        PNODE->position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        PNODE->size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;

        applyNodeDataToWindow(PNODE);

        return;
    }
    
    // If it's not, get the node under our cursor

    m_lDwindleNodesData.push_back(SDwindleNodeData::newNode(OPENINGON->pParent, {OPENINGON, PNODE}));
    const auto NEWPARENT = &m_lDwindleNodesData.back();

    // make the parent have the OPENINGON's stats
    NEWPARENT->position = OPENINGON->position;
    NEWPARENT->size = OPENINGON->size;
    NEWPARENT->workspaceID = OPENINGON->workspaceID;

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
            std::swap(NEWPARENT->children[0], NEWPARENT->children[1]);
        } else {
            // we are hovering over the second node, keep PNODE second.
        }
    } else {
        if (FORCESPLIT == 1) {
            std::swap(NEWPARENT->children[0], NEWPARENT->children[1]);
        } else {
            // keep it as it is
        }
    }
    // and update the previous parent (if it exists)
    if (auto parent = OPENINGON->pParent) {
        std::replace(parent->children.begin(), parent->children.end(), OPENINGON, NEWPARENT);
    }
    OPENINGON->pParent = NEWPARENT;
    PNODE->pParent = NEWPARENT;

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


    if (auto group = OPENINGON->getOuterGroup()) {
        //pWindow->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(pWindow));

        //close old group
        toggleWindowGroup(group->getWindow());

        //and open a group one level above
        toggleWindowGroup(pWindow);
        RASSERT(NEWPARENT->isGroup(), "new parent should be a group")
    }
    NEWPARENT->recalcSizePosRecursive();
}

void CHyprDwindleLayout::onWindowRemovedTiling(CWindow* pWindow) {

    const auto PNODE = getNodeFromWindow(pWindow, false);

    if (!PNODE) {
        Debug::log(ERR, "onWindowRemovedTiling node null?");
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

    if (auto parent2 = PPARENT->pParent) {
        std::replace(parent2->children.begin(), parent2->children.end(), PPARENT, PSIBLING);
    }

    // check if it was grouped
    if (auto group = PNODE->getOuterGroup()) {
        if (group == PPARENT) {
            //close group as window removal will result in group with single member
            toggleWindowGroup(pWindow);
            PSIBLING->recalcSizePosRecursive();
        } else {
            //go onee window back in group
            switchGroupWindow(pWindow, false);
            group->recalcSizePosRecursive();
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
//        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);
//
//        if (!PFULLWINDOW) { // ????
//            PWORKSPACE->m_bHasFullscreenWindow = false;
//        } else {
//            SDwindleNodeData fakeNode;
//            fakeNode.pWindow = PFULLWINDOW;
//            fakeNode.position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
//            fakeNode.size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
//            fakeNode.workspaceID = PWORKSPACE->m_iID;
//            PFULLWINDOW->m_vPosition = fakeNode.position;
//            PFULLWINDOW->m_vSize = fakeNode.size;
//
//            applyNodeDataToWindow(&fakeNode);
//
//            return;
//        }
    }

    const auto TOPNODE = getMasterNodeOnWorkspace(PMONITOR->activeWorkspace);

    if (TOPNODE && PMONITOR) {
        TOPNODE->position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        TOPNODE->size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        TOPNODE->recalcSizePosRecursive();
    }
}

bool CHyprDwindleLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow, false) != nullptr;
}

void CHyprDwindleLayout::resizeActiveWindow(const Vector2D& pixResize, CWindow* pWindow) {

    auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;


    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    auto PNODE = getNodeFromWindow(PWINDOW, true);

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
        const auto PNODE = getNodeFromWindow(pWindow, false);
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
            // TODO: This is now just a special kind of group,
            //  i.e. created at the topNode and with the currently active window made visible
            //  in this case: No/other decorations

            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

//            SDwindleNodeData fakeNode;
//            fakeNode.pWindow = pWindow;
//            fakeNode.position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
//            fakeNode.size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
//            fakeNode.workspaceID = pWindow->m_iWorkspaceID;
//            pWindow->m_vPosition = fakeNode.position;
//            pWindow->m_vSize = fakeNode.size;
//
//            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv());

    g_pCompositor->moveWindowToTop(pWindow);

    recalculateMonitor(PMONITOR->ID);
}

void CHyprDwindleLayout::recalculateWindow(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow, false);

    if (!PNODE)
        return;

    PNODE->recalcSizePosRecursive();
}

void CHyprDwindleLayout::toggleWindowGroup(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    // get the node
    const auto PNODE = getNodeFromWindow(pWindow, true);

    if (!PNODE)
        return; // reject

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PNODE->workspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow)
        fullscreenRequestForWindow(g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID), FULLSCREEN_FULL, false);

    if (PNODE->isGroup()) {
//        // if there is a parent, release it
//        const auto INACTIVEBORDERCOL = CColor(g_pConfigManager->getInt("general:col.inactive_border"));
//        for (auto& node : PGROUPPARENT->groupMembers) {
//            node->pGroupParent = nullptr;
//            node->pWindow->m_cRealBorderColor.setValueAndWarp(INACTIVEBORDERCOL); // no anim here because they pop in
//
//            for (auto& wd : node->pWindow->m_dWindowDecorations) {
//                wd->updateWindow(node->pWindow);
//            }
//        }

        PNODE->ungroup();
        for (auto&& child: PNODE->getChildrenRecursive(true)) {
            if (auto win = child->getWindow(); win != pWindow) {
                win->m_bHidden = false;
            }
        }
        PNODE->recalcSizePosRecursive();


//        if (g_pCompositor->windowValidMapped(g_pCompositor->m_pLastWindow))
//            g_pCompositor->m_pLastWindow->m_cRealBorderColor = CColor(g_pConfigManager->getInt("general:col.active_border"));
    } else {
        // if there is no group, let's make one

        const auto PPARENT = PNODE->pParent;

        if (!PPARENT)
            return; // reject making group on single window


//        what to do with this
//        const auto GROUPINACTIVEBORDERCOL = CColor(g_pConfigManager->getInt("dwinle:col.group_border"));
//        for (auto& c : PPARENT->groupMembers) {
//            c->pGroupParent = PPARENT;
//            c->pWindow->m_cRealBorderColor = GROUPINACTIVEBORDERCOL;
//
//            c->pWindow->m_dWindowDecorations.push_back(std::make_unique<CHyprGroupBarDecoration>(c->pWindow));
//
//            if (c->pWindow == g_pCompositor->m_pLastWindow)
//                c->pWindow->m_cRealBorderColor = CColor(g_pConfigManager->getInt("dwindle:col.group_border_active"));
//        }

        PPARENT->turnIntoGroup(PNODE);

        for (auto&& child: PPARENT->getChildrenRecursive()) {
            if (auto win = child->getWindow(); win != pWindow) {
                win->m_bHidden = true;
            }
        }

        PPARENT->recalcSizePosRecursive();
    }

    g_pInputManager->refocus();
}

std::deque<CWindow*> CHyprDwindleLayout::getGroupMembers(CWindow* pWindow) {

    std::deque<CWindow*> result;

    if (!g_pCompositor->windowExists(pWindow))
        return result; // reject with empty

    // get the node
    const auto PNODE = getNodeFromWindow(pWindow, false);

    if (!PNODE)
        return result;  // reject with empty

    if (!PNODE->isGroup())
        return result;  // reject with empty

    std::deque<SDwindleNodeData*> children;
    PNODE->addChildrenRecursive(children, false);

    std::transform(children.begin(), children.end(), result.begin(), [](SDwindleNodeData* node){return node->getWindow();});
    return result;
}

void CHyprDwindleLayout::switchGroupWindow(CWindow* pWindow, bool forward) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return; // reject

    const auto PNODE = getNodeFromWindow(pWindow, true);
    if (!PNODE || !PNODE->isGroup())
        return; // reject

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PNODE->workspaceID);

   pWindow->m_bHidden = true;
   RASSERT(PNODE->nextInGroup(forward), "switchGroup unsuccessful: We should operate on a grouping node");
   pWindow = PNODE->getWindow();
   pWindow->m_bHidden = false;

//
//    if (PNODE->pWindow->m_bIsFullscreen) {
//        fullscreenRequestForWindow(PNODE->pWindow, PWORKSPACE->m_efFullscreenMode, false);
//        restoreFullscreen = true;
//    }

    PNODE->recalcSizePosRecursive();

//    for (auto& gm : PNODE->pGroupParent->groupMembers) {
//        for (auto& deco : gm->pWindow->m_dWindowDecorations) {
//            deco->updateWindow(gm->pWindow);
//        }
//    }

    // focus
    g_pCompositor->focusWindow(pWindow);

//    if (restoreFullscreen) {
//         fullscreenRequestForWindow(PNODE->pGroupParent->groupMembers[PNODE->pGroupParent->groupMemberActive]->pWindow, PWORKSPACE->m_efFullscreenMode, true);
//    }
}

SWindowRenderLayoutHints CHyprDwindleLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    const auto PNODE = getNodeFromWindow(pWindow, false);
    if (!PNODE)
        return hints; // left for the future, maybe floating funkiness

    if (PNODE->isGroup()) {
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

    const auto PNODE = getNodeFromWindow(pWindow, true);
    const auto PNODE2 = getNodeFromWindow(pWindow2, true);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        Debug::log(ERR, "Dwindle: Rejecting a swap between workspaces");
        return;
    }

    // we will not delete the nodes, just fix the tree
    // TODO: move tree operating logic into SDwindleNodeData class
    if (PNODE2->pParent == PNODE->pParent) {
        std::swap(PNODE->pParent->children[0], PNODE->pParent->children[1]);
    } else {
        {
            const auto PPARENT = PNODE->pParent;
            std::replace(PPARENT->children.begin(), PPARENT->children.end(), PNODE, PNODE2);
        } {
            const auto PPARENT = PNODE2->pParent;
            std::replace(PPARENT->children.begin(), PPARENT->children.end(), PNODE2, PNODE);
        }
        std::swap(PNODE->pParent, PNODE2->pParent);
    }


    // recalc the workspace
    getMasterNodeOnWorkspace(PNODE->workspaceID)->recalcSizePosRecursive();
}

void CHyprDwindleLayout::alterSplitRatioBy(CWindow* pWindow, float ratio) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow, false);

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
    const auto PNODE = getNodeFromWindow(pWindow, false);

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
