#include "DwindleLayout.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"

void SDwindleNodeData::recalcSizePosRecursive(bool force, bool horizontalOverride, bool verticalOverride) {
    if (children[0]) {
        static auto PSMARTSPLIT    = CConfigValue<Hyprlang::INT>("dwindle:smart_split");
        static auto PPRESERVESPLIT = CConfigValue<Hyprlang::INT>("dwindle:preserve_split");
        static auto PFLMULT        = CConfigValue<Hyprlang::FLOAT>("dwindle:split_width_multiplier");

        if (*PPRESERVESPLIT == 0 && *PSMARTSPLIT == 0)
            splitTop = box.h * *PFLMULT > box.w;

        if (verticalOverride == true)
            splitTop = true;
        else if (horizontalOverride == true)
            splitTop = false;

        const auto SPLITSIDE = !splitTop;

        if (SPLITSIDE) {
            // split left/right
            const float FIRSTSIZE = box.w / 2.0 * splitRatio;
            children[0]->box      = CBox{box.x, box.y, FIRSTSIZE, box.h}.noNegativeSize();
            children[1]->box      = CBox{box.x + FIRSTSIZE, box.y, box.w - FIRSTSIZE, box.h}.noNegativeSize();
        } else {
            // split top/bottom
            const float FIRSTSIZE = box.h / 2.0 * splitRatio;
            children[0]->box      = CBox{box.x, box.y, box.w, FIRSTSIZE}.noNegativeSize();
            children[1]->box      = CBox{box.x, box.y + FIRSTSIZE, box.w, box.h - FIRSTSIZE}.noNegativeSize();
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

SDwindleNodeData* CHyprDwindleLayout::getClosestNodeOnWorkspace(const int& id, const Vector2D& point) {
    SDwindleNodeData* res         = nullptr;
    double            distClosest = -1;
    for (auto& n : m_lDwindleNodesData) {
        if (n.workspaceID == id && n.pWindow && g_pCompositor->windowValidMapped(n.pWindow)) {
            auto distAnother = vecToRectDistanceSquared(point, n.box.pos(), n.box.pos() + n.box.size());
            if (!res || distAnother < distClosest) {
                res         = &n;
                distClosest = distAnother;
            }
        }
    }
    return res;
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

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto& m : g_pCompositor->m_vMonitors) {
            if (m->specialWorkspaceID == pNode->workspaceID) {
                PMONITOR = m.get();
                break;
            }
        }
    } else {
        PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_iMonitorID);
    }

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node {}!!", pNode);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->box.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->box.x + pNode->box.w, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->box.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->box.y + pNode->box.h, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PWINDOW = pNode->pWindow;
    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(pNode->workspaceID));

    if (!g_pCompositor->windowExists(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid {}!!", pNode, PWINDOW);
        onWindowRemovedTiling(PWINDOW);
        return;
    }

    if (PWINDOW->m_bIsFullscreen && !pNode->ignoreFullscreenChecks)
        return;

    PWINDOW->updateSpecialRenderData();

    static auto PNOGAPSWHENONLY = CConfigValue<Hyprlang::INT>("dwindle:no_gaps_when_only");
    static auto PGAPSINDATA     = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto PGAPSOUTDATA    = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const PGAPSIN         = (CCssGapData*)(PGAPSINDATA.ptr())->getData();
    auto* const PGAPSOUT        = (CCssGapData*)(PGAPSOUTDATA.ptr())->getData();

    auto        gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto        gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);

    CBox        nodeBox = pNode->box;
    nodeBox.round();

    PWINDOW->m_vSize     = nodeBox.size();
    PWINDOW->m_vPosition = nodeBox.pos();

    const auto NODESONWORKSPACE = getNodesOnWorkspace(PWINDOW->m_iWorkspaceID);

    if (*PNOGAPSWHENONLY && !g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID) &&
        (NODESONWORKSPACE == 1 || (PWINDOW->m_bIsFullscreen && g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_efFullscreenMode == FULLSCREEN_MAXIMIZED))) {

        PWINDOW->m_sSpecialRenderData.border   = WORKSPACERULE.border.value_or(*PNOGAPSWHENONLY == 2);
        PWINDOW->m_sSpecialRenderData.decorate = WORKSPACERULE.decorate.value_or(true);
        PWINDOW->m_sSpecialRenderData.rounding = false;
        PWINDOW->m_sSpecialRenderData.shadow   = false;

        PWINDOW->updateWindowDecos();

        const auto RESERVED = PWINDOW->getFullWindowReservedArea();

        PWINDOW->m_vRealPosition = PWINDOW->m_vPosition + RESERVED.topLeft;
        PWINDOW->m_vRealSize     = PWINDOW->m_vSize - (RESERVED.topLeft + RESERVED.bottomRight);

        g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goal());

        return;
    }

    auto       calcPos  = PWINDOW->m_vPosition;
    auto       calcSize = PWINDOW->m_vSize;

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? gapsOut.left : gapsIn.left, DISPLAYTOP ? gapsOut.top : gapsIn.top);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? gapsOut.right : gapsIn.right, DISPLAYBOTTOM ? gapsOut.bottom : gapsIn.bottom);

    calcPos  = calcPos + OFFSETTOPLEFT;
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
            calcSize   = PWINDOW->m_vPseudoSize * scale;
            calcPos    = calcPos + DELTA / 2.f; // center
        } else {
            auto DELTA = calcSize - PWINDOW->m_vPseudoSize;
            calcPos    = calcPos + DELTA / 2.f; // center
            calcSize   = PWINDOW->m_vPseudoSize;
        }
    }

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID) && !PWINDOW->m_bIsFullscreen) {
        // if special, we adjust the coords a bit
        static auto PSCALEFACTOR = CConfigValue<Hyprlang::FLOAT>("dwindle:special_scale_factor");

        CBox        wb = {calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f, calcSize * *PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        PWINDOW->m_vRealPosition = wb.pos();
        PWINDOW->m_vRealSize     = wb.size();

        g_pXWaylandManager->setWindowSize(PWINDOW, wb.size());
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round(); // avoid rounding mess

        PWINDOW->m_vRealSize     = wb.size();
        PWINDOW->m_vRealPosition = wb.pos();

        g_pXWaylandManager->setWindowSize(PWINDOW, wb.size());
    }

    if (force) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

void CHyprDwindleLayout::onWindowCreatedTiling(CWindow* pWindow, eDirection direction) {
    if (pWindow->m_bIsFloating)
        return;

    m_lDwindleNodesData.push_back(SDwindleNodeData());
    const auto  PNODE = &m_lDwindleNodesData.back();

    const auto  PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    static auto PUSEACTIVE    = CConfigValue<Hyprlang::INT>("dwindle:use_active_for_splits");
    static auto PDEFAULTSPLIT = CConfigValue<Hyprlang::FLOAT>("dwindle:default_split_ratio");

    if (direction != DIRECTION_DEFAULT && overrideDirection == DIRECTION_DEFAULT)
        overrideDirection = direction;

    // Populate the node with our window's data
    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow     = pWindow;
    PNODE->isNode      = false;
    PNODE->layout      = this;

    SDwindleNodeData* OPENINGON;

    const auto        MOUSECOORDS   = m_vOverrideFocalPoint.value_or(g_pInputManager->getMouseCoordsInternal());
    const auto        MONFROMCURSOR = g_pCompositor->getMonitorFromVector(MOUSECOORDS);

    if (PMONITOR->ID == MONFROMCURSOR->ID &&
        (PNODE->workspaceID == PMONITOR->activeWorkspace || (g_pCompositor->isWorkspaceSpecial(PNODE->workspaceID) && PMONITOR->specialWorkspaceID)) && !*PUSEACTIVE) {
        OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS));

        if (!OPENINGON && g_pCompositor->isPointOnReservedArea(MOUSECOORDS, PMONITOR))
            OPENINGON = getClosestNodeOnWorkspace(PNODE->workspaceID, MOUSECOORDS);

    } else if (*PUSEACTIVE) {
        if (g_pCompositor->m_pLastWindow && !g_pCompositor->m_pLastWindow->m_bIsFloating && g_pCompositor->m_pLastWindow != pWindow &&
            g_pCompositor->m_pLastWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID && g_pCompositor->m_pLastWindow->m_bIsMapped) {
            OPENINGON = getNodeFromWindow(g_pCompositor->m_pLastWindow);
        } else {
            OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS));
        }

        if (!OPENINGON && g_pCompositor->isPointOnReservedArea(MOUSECOORDS, PMONITOR))
            OPENINGON = getClosestNodeOnWorkspace(PNODE->workspaceID, MOUSECOORDS);

    } else
        OPENINGON = getFirstNodeOnWorkspace(pWindow->m_iWorkspaceID);

    Debug::log(LOG, "OPENINGON: {}, Monitor: {}", OPENINGON, PMONITOR->ID);

    if (OPENINGON && OPENINGON->workspaceID != PNODE->workspaceID) {
        // special workspace handling
        OPENINGON = getFirstNodeOnWorkspace(PNODE->workspaceID);
    }

    // first, check if OPENINGON isn't too big.
    const auto PREDSIZEMAX = OPENINGON ? Vector2D(OPENINGON->box.w, OPENINGON->box.h) : PMONITOR->vecSize;
    if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow); MAXSIZE.x < PREDSIZEMAX.x || MAXSIZE.y < PREDSIZEMAX.y) {
        // we can't continue. make it floating.
        pWindow->m_bIsFloating = true;
        m_lDwindleNodesData.remove(*PNODE);
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
        return;
    }

    // last fail-safe to avoid duplicate fullscreens
    if ((!OPENINGON || OPENINGON->pWindow == pWindow) && getNodesOnWorkspace(PNODE->workspaceID) > 1) {
        for (auto& node : m_lDwindleNodesData) {
            if (node.workspaceID == PNODE->workspaceID && node.pWindow != nullptr && node.pWindow != pWindow) {
                OPENINGON = &node;
                break;
            }
        }
    }

    // if it's the first, it's easy. Make it fullscreen.
    if (!OPENINGON || OPENINGON->pWindow == pWindow) {
        PNODE->box = CBox{PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft, PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight};

        applyNodeDataToWindow(PNODE);

        pWindow->applyGroupRules();

        return;
    }

    if (!m_vOverrideFocalPoint && g_pInputManager->m_bWasDraggingWindow) {
        if (OPENINGON->pWindow->checkInputOnDecos(INPUT_TYPE_DRAG_END, MOUSECOORDS, pWindow))
            return;
    }

    // if it's a group, add the window
    if (OPENINGON->pWindow->m_sGroupData.pNextWindow                                  // target is group
        && pWindow->canBeGroupedInto(OPENINGON->pWindow) && !m_vOverrideFocalPoint) { // we are not moving window
        m_lDwindleNodesData.remove(*PNODE);

        static auto USECURRPOS = CConfigValue<Hyprlang::INT>("group:insert_after_current");
        (*USECURRPOS ? OPENINGON->pWindow : OPENINGON->pWindow->getGroupTail())->insertWindowToGroup(pWindow);

        OPENINGON->pWindow->setGroupCurrent(pWindow);
        pWindow->applyGroupRules();
        pWindow->updateWindowDecos();
        recalculateWindow(pWindow);

        if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
            pWindow->addWindowDeco(std::make_unique<CHyprGroupBarDecoration>(pWindow));

        return;
    }

    // If it's not, get the node under our cursor

    m_lDwindleNodesData.push_back(SDwindleNodeData());
    const auto NEWPARENT = &m_lDwindleNodesData.back();

    // make the parent have the OPENINGON's stats
    NEWPARENT->box         = OPENINGON->box;
    NEWPARENT->workspaceID = OPENINGON->workspaceID;
    NEWPARENT->pParent     = OPENINGON->pParent;
    NEWPARENT->isNode      = true; // it is a node
    NEWPARENT->splitRatio  = std::clamp(*PDEFAULTSPLIT, 0.1f, 1.9f);

    static auto PWIDTHMULTIPLIER = CConfigValue<Hyprlang::FLOAT>("dwindle:split_width_multiplier");

    // if cursor over first child, make it first, etc
    const auto SIDEBYSIDE = NEWPARENT->box.w > NEWPARENT->box.h * *PWIDTHMULTIPLIER;
    NEWPARENT->splitTop   = !SIDEBYSIDE;

    static auto PFORCESPLIT                = CConfigValue<Hyprlang::INT>("dwindle:force_split");
    static auto PERMANENTDIRECTIONOVERRIDE = CConfigValue<Hyprlang::INT>("dwindle:permanent_direction_override");
    static auto PSMARTSPLIT                = CConfigValue<Hyprlang::INT>("dwindle:smart_split");

    bool        horizontalOverride = false;
    bool        verticalOverride   = false;

    // let user select position -> top, right, bottom, left
    if (overrideDirection != DIRECTION_DEFAULT) {

        // this is horizontal
        if (overrideDirection % 2 == 0)
            verticalOverride = true;
        else
            horizontalOverride = true;

        // 0 -> top and left | 1,2 -> right and bottom
        if (overrideDirection % 3 == 0) {
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }

        // whether or not the override persists after opening one window
        if (*PERMANENTDIRECTIONOVERRIDE == 0)
            overrideDirection = DIRECTION_DEFAULT;
    } else if (*PSMARTSPLIT == 1) {
        const auto PARENT_CENTER      = NEWPARENT->box.pos() + NEWPARENT->box.size() / 2;
        const auto PARENT_PROPORTIONS = NEWPARENT->box.h / NEWPARENT->box.w;
        const auto DELTA              = MOUSECOORDS - PARENT_CENTER;
        const auto DELTA_SLOPE        = DELTA.y / DELTA.x;

        if (abs(DELTA_SLOPE) < PARENT_PROPORTIONS) {
            if (DELTA.x > 0) {
                // right
                NEWPARENT->splitTop    = false;
                NEWPARENT->children[0] = OPENINGON;
                NEWPARENT->children[1] = PNODE;
            } else {
                // left
                NEWPARENT->splitTop    = false;
                NEWPARENT->children[0] = PNODE;
                NEWPARENT->children[1] = OPENINGON;
            }
        } else {
            if (DELTA.y > 0) {
                // bottom
                NEWPARENT->splitTop    = true;
                NEWPARENT->children[0] = OPENINGON;
                NEWPARENT->children[1] = PNODE;
            } else {
                // top
                NEWPARENT->splitTop    = true;
                NEWPARENT->children[0] = PNODE;
                NEWPARENT->children[1] = OPENINGON;
            }
        }
    } else if (*PFORCESPLIT == 0 || !pWindow->m_bFirstMap) {
        if ((SIDEBYSIDE &&
             VECINRECT(MOUSECOORDS, NEWPARENT->box.x, NEWPARENT->box.y / *PWIDTHMULTIPLIER, NEWPARENT->box.x + NEWPARENT->box.w / 2.f, NEWPARENT->box.y + NEWPARENT->box.h)) ||
            (!SIDEBYSIDE &&
             VECINRECT(MOUSECOORDS, NEWPARENT->box.x, NEWPARENT->box.y / *PWIDTHMULTIPLIER, NEWPARENT->box.x + NEWPARENT->box.w, NEWPARENT->box.y + NEWPARENT->box.h / 2.f))) {
            // we are hovering over the first node, make PNODE first.
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            // we are hovering over the second node, make PNODE second.
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }
    } else {
        if (*PFORCESPLIT == 1) {
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
    if (!verticalOverride && (NEWPARENT->box.w * *PWIDTHMULTIPLIER > NEWPARENT->box.h || horizontalOverride)) {
        // split left/right -> forced
        OPENINGON->box = {NEWPARENT->box.pos(), Vector2D(NEWPARENT->box.w / 2.f, NEWPARENT->box.h)};
        PNODE->box     = {Vector2D(NEWPARENT->box.x + NEWPARENT->box.w / 2.f, NEWPARENT->box.y), Vector2D(NEWPARENT->box.w / 2.f, NEWPARENT->box.h)};
    } else {
        // split top/bottom
        OPENINGON->box = {NEWPARENT->box.pos(), Vector2D(NEWPARENT->box.w, NEWPARENT->box.h / 2.f)};
        PNODE->box     = {Vector2D(NEWPARENT->box.x, NEWPARENT->box.y + NEWPARENT->box.h / 2.f), Vector2D(NEWPARENT->box.w, NEWPARENT->box.h / 2.f)};
    }

    OPENINGON->pParent = NEWPARENT;
    PNODE->pParent     = NEWPARENT;

    NEWPARENT->recalcSizePosRecursive(false, horizontalOverride, verticalOverride);

    recalculateMonitor(pWindow->m_iMonitorID);

    pWindow->applyGroupRules();
}

void CHyprDwindleLayout::onWindowRemovedTiling(CWindow* pWindow) {

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE) {
        Debug::log(ERR, "onWindowRemovedTiling node null?");
        return;
    }

    pWindow->updateSpecialRenderData();

    if (pWindow->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

    const auto PPARENT = PNODE->pParent;

    if (!PPARENT) {
        Debug::log(LOG, "Removing last node (dwindle)");
        m_lDwindleNodesData.remove(*PNODE);
        return;
    }

    const auto PSIBLING = PPARENT->children[0] == PNODE ? PPARENT->children[1] : PPARENT->children[0];

    PSIBLING->box     = PPARENT->box;
    PSIBLING->pParent = PPARENT->pParent;

    if (PPARENT->pParent != nullptr) {
        if (PPARENT->pParent->children[0] == PPARENT) {
            PPARENT->pParent->children[0] = PSIBLING;
        } else {
            PPARENT->pParent->children[1] = PSIBLING;
        }
    }

    PPARENT->valid = false;
    PNODE->valid   = false;

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

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->specialWorkspaceID)
        calculateWorkspace(PMONITOR->specialWorkspaceID);

    calculateWorkspace(PMONITOR->activeWorkspace);
}

void CHyprDwindleLayout::calculateWorkspace(const int& ws) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(ws);

    if (!PWORKSPACE)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);

    if (!PMONITOR)
        return;

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        // massive hack from the fullscreen func
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) {
            PFULLWINDOW->m_vRealPosition = PMONITOR->vecPosition;
            PFULLWINDOW->m_vRealSize     = PMONITOR->vecSize;
        } else if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_MAXIMIZED) {
            SDwindleNodeData fakeNode;
            fakeNode.pWindow         = PFULLWINDOW;
            fakeNode.box             = {PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft, PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight};
            fakeNode.workspaceID     = PWORKSPACE->m_iID;
            PFULLWINDOW->m_vPosition = fakeNode.box.pos();
            PFULLWINDOW->m_vSize     = fakeNode.box.size();
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }

        // if has fullscreen, don't calculate the rest
        return;
    }

    const auto TOPNODE = getMasterNodeOnWorkspace(ws);

    if (TOPNODE) {
        TOPNODE->box = {PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft, PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight};
        TOPNODE->recalcSizePosRecursive();
    }
}

bool CHyprDwindleLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprDwindleLayout::onBeginDragWindow() {
    m_PseudoDragFlags.started = false;
    m_PseudoDragFlags.pseudo  = false;
    IHyprLayout::onBeginDragWindow();
}

void CHyprDwindleLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, CWindow* pWindow) {

    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        PWINDOW->m_vRealSize = Vector2D(std::max((PWINDOW->m_vRealSize.goal() + pixResize).x, 20.0), std::max((PWINDOW->m_vRealSize.goal() + pixResize).y, 20.0));
        PWINDOW->updateWindowDecos();
        return;
    }

    static auto PANIMATE       = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");
    static auto PSMARTRESIZING = CConfigValue<Hyprlang::INT>("dwindle:smart_resizing");

    // get some data about our window
    const auto PMONITOR      = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
    const bool DISPLAYLEFT   = STICKS(PWINDOW->m_vPosition.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(PWINDOW->m_vPosition.x + PWINDOW->m_vSize.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(PWINDOW->m_vPosition.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(PWINDOW->m_vPosition.y + PWINDOW->m_vSize.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    if (PWINDOW->m_bIsPseudotiled) {
        if (!m_PseudoDragFlags.started) {
            m_PseudoDragFlags.started = true;

            const auto pseudoSize  = PWINDOW->m_vRealSize.goal();
            const auto mouseOffset = g_pInputManager->getMouseCoordsInternal() - (PNODE->box.pos() + ((PNODE->box.size() / 2) - (pseudoSize / 2)));

            if (mouseOffset.x > 0 && mouseOffset.x < pseudoSize.x && mouseOffset.y > 0 && mouseOffset.y < pseudoSize.y) {
                m_PseudoDragFlags.pseudo  = true;
                m_PseudoDragFlags.xExtent = mouseOffset.x > pseudoSize.x / 2;
                m_PseudoDragFlags.yExtent = mouseOffset.y > pseudoSize.y / 2;

                PWINDOW->m_vPseudoSize = pseudoSize;
            } else {
                m_PseudoDragFlags.pseudo = false;
            }
        }

        if (m_PseudoDragFlags.pseudo) {
            if (m_PseudoDragFlags.xExtent)
                PWINDOW->m_vPseudoSize.x += pixResize.x * 2;
            else
                PWINDOW->m_vPseudoSize.x -= pixResize.x * 2;
            if (m_PseudoDragFlags.yExtent)
                PWINDOW->m_vPseudoSize.y += pixResize.y * 2;
            else
                PWINDOW->m_vPseudoSize.y -= pixResize.y * 2;

            CBox wbox = PNODE->box;
            wbox.round();

            PWINDOW->m_vPseudoSize = {std::clamp(PWINDOW->m_vPseudoSize.x, 30.0, wbox.w), std::clamp(PWINDOW->m_vPseudoSize.y, 30.0, wbox.h)};

            PWINDOW->m_vLastFloatingSize = PWINDOW->m_vPseudoSize;
            PNODE->recalcSizePosRecursive(*PANIMATE == 0);

            return;
        }
    }

    // construct allowed movement
    Vector2D allowedMovement = pixResize;
    if (DISPLAYLEFT && DISPLAYRIGHT)
        allowedMovement.x = 0;

    if (DISPLAYBOTTOM && DISPLAYTOP)
        allowedMovement.y = 0;

    if (*PSMARTRESIZING == 1) {
        // Identify inner and outer nodes for both directions
        SDwindleNodeData* PVOUTER = nullptr;
        SDwindleNodeData* PVINNER = nullptr;
        SDwindleNodeData* PHOUTER = nullptr;
        SDwindleNodeData* PHINNER = nullptr;

        const auto        LEFT   = corner == CORNER_TOPLEFT || corner == CORNER_BOTTOMLEFT || DISPLAYRIGHT;
        const auto        TOP    = corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT || DISPLAYBOTTOM;
        const auto        RIGHT  = corner == CORNER_TOPRIGHT || corner == CORNER_BOTTOMRIGHT || DISPLAYLEFT;
        const auto        BOTTOM = corner == CORNER_BOTTOMLEFT || corner == CORNER_BOTTOMRIGHT || DISPLAYTOP;
        const auto        NONE   = corner == CORNER_NONE;

        for (auto PCURRENT = PNODE; PCURRENT && PCURRENT->pParent; PCURRENT = PCURRENT->pParent) {
            const auto PPARENT = PCURRENT->pParent;

            if (!PVOUTER && PPARENT->splitTop && (NONE || (TOP && PPARENT->children[1] == PCURRENT) || (BOTTOM && PPARENT->children[0] == PCURRENT)))
                PVOUTER = PCURRENT;
            else if (!PVOUTER && !PVINNER && PPARENT->splitTop)
                PVINNER = PCURRENT;
            else if (!PHOUTER && !PPARENT->splitTop && (NONE || (LEFT && PPARENT->children[1] == PCURRENT) || (RIGHT && PPARENT->children[0] == PCURRENT)))
                PHOUTER = PCURRENT;
            else if (!PHOUTER && !PHINNER && !PPARENT->splitTop)
                PHINNER = PCURRENT;

            if (PVOUTER && PHOUTER)
                break;
        }

        if (PHOUTER) {
            PHOUTER->pParent->splitRatio = std::clamp(PHOUTER->pParent->splitRatio + allowedMovement.x * 2.f / PHOUTER->pParent->box.w, 0.1, 1.9);

            if (PHINNER) {
                const auto ORIGINAL = PHINNER->box.w;
                PHOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
                if (PHINNER->pParent->children[0] == PHINNER)
                    PHINNER->pParent->splitRatio = std::clamp((ORIGINAL - allowedMovement.x) / PHINNER->pParent->box.w * 2.f, 0.1, 1.9);
                else
                    PHINNER->pParent->splitRatio = std::clamp(2 - (ORIGINAL + allowedMovement.x) / PHINNER->pParent->box.w * 2.f, 0.1, 1.9);
                PHINNER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
            } else
                PHOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
        }

        if (PVOUTER) {
            PVOUTER->pParent->splitRatio = std::clamp(PVOUTER->pParent->splitRatio + allowedMovement.y * 2.f / PVOUTER->pParent->box.h, 0.1, 1.9);

            if (PVINNER) {
                const auto ORIGINAL = PVINNER->box.h;
                PVOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
                if (PVINNER->pParent->children[0] == PVINNER)
                    PVINNER->pParent->splitRatio = std::clamp((ORIGINAL - allowedMovement.y) / PVINNER->pParent->box.h * 2.f, 0.1, 1.9);
                else
                    PVINNER->pParent->splitRatio = std::clamp(2 - (ORIGINAL + allowedMovement.y) / PVINNER->pParent->box.h * 2.f, 0.1, 1.9);
                PVINNER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
            } else
                PVOUTER->pParent->recalcSizePosRecursive(*PANIMATE == 0);
        }
    } else {
        // get the correct containers to apply splitratio to
        const auto PPARENT = PNODE->pParent;

        if (!PPARENT)
            return; // the only window on a workspace, ignore

        const bool PARENTSIDEBYSIDE = !PPARENT->splitTop;

        // Get the parent's parent
        auto PPARENT2 = PPARENT->pParent;

        // No parent means we have only 2 windows, and thus one axis of freedom
        if (!PPARENT2) {
            if (PARENTSIDEBYSIDE) {
                allowedMovement.x *= 2.f / PPARENT->box.w;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.x, 0.1, 1.9);
                PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
            } else {
                allowedMovement.y *= 2.f / PPARENT->box.h;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, 0.1, 1.9);
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
                allowedMovement.x *= 2.f / PPARENT->box.w;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.x, 0.1, 1.9);
                PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
            } else {
                allowedMovement.y *= 2.f / PPARENT->box.h;
                PPARENT->splitRatio = std::clamp(PPARENT->splitRatio + allowedMovement.y, 0.1, 1.9);
                PPARENT->recalcSizePosRecursive(*PANIMATE == 0);
            }

            return;
        }

        // 2 axes of freedom
        const auto SIDECONTAINER = PARENTSIDEBYSIDE ? PPARENT : PPARENT2;
        const auto TOPCONTAINER  = PARENTSIDEBYSIDE ? PPARENT2 : PPARENT;

        allowedMovement.x *= 2.f / SIDECONTAINER->box.w;
        allowedMovement.y *= 2.f / TOPCONTAINER->box.h;

        SIDECONTAINER->splitRatio = std::clamp(SIDECONTAINER->splitRatio + allowedMovement.x, 0.1, 1.9);
        TOPCONTAINER->splitRatio  = std::clamp(TOPCONTAINER->splitRatio + allowedMovement.y, 0.1, 1.9);
        SIDECONTAINER->recalcSizePosRecursive(*PANIMATE == 0);
        TOPCONTAINER->recalcSizePosRecursive(*PANIMATE == 0);
    }
}

void CHyprDwindleLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen)
        return; // ignore

    const auto PMONITOR   = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // save position and size if floating
    if (pWindow->m_bIsFloating && on) {
        pWindow->m_vLastFloatingSize     = pWindow->m_vRealSize.goal();
        pWindow->m_vLastFloatingPosition = pWindow->m_vRealPosition.goal();
        pWindow->m_vPosition             = pWindow->m_vRealPosition.goal();
        pWindow->m_vSize                 = pWindow->m_vRealSize.goal();
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen           = on;
    PWORKSPACE->m_bHasFullscreenWindow = !PWORKSPACE->m_bHasFullscreenWindow;

    pWindow->updateDynamicRules();
    pWindow->updateWindowDecos();

    g_pEventManager->postEvent(SHyprIPCEvent{"fullscreen", std::to_string((int)on)});
    EMIT_HOOK_EVENT("fullscreen", pWindow);

    if (!pWindow->m_bIsFullscreen) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            pWindow->m_vRealPosition = pWindow->m_vLastFloatingPosition;
            pWindow->m_vRealSize     = pWindow->m_vLastFloatingSize;

            pWindow->updateSpecialRenderData();
        }
    } else {
        // if it now got fullscreen, make it fullscreen

        PWORKSPACE->m_efFullscreenMode = fullscreenMode;

        // apply new pos and size being monitors' box
        if (fullscreenMode == FULLSCREEN_FULL) {
            pWindow->m_vRealPosition = PMONITOR->vecPosition;
            pWindow->m_vRealSize     = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SDwindleNodeData fakeNode;
            fakeNode.pWindow     = pWindow;
            fakeNode.box         = {PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft, PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight};
            fakeNode.workspaceID = pWindow->m_iWorkspaceID;
            pWindow->m_vPosition = fakeNode.box.pos();
            pWindow->m_vSize     = fakeNode.box.size();
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goal());

    g_pCompositor->changeWindowZOrder(pWindow, true);

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

SWindowRenderLayoutHints CHyprDwindleLayout::requestRenderHints(CWindow* pWindow) {
    // window should be valid, insallah
    SWindowRenderLayoutHints hints;

    const auto               PNODE = getNodeFromWindow(pWindow);
    if (!PNODE)
        return hints; // left for the future, maybe floating funkiness

    return hints;
}

void CHyprDwindleLayout::moveWindowTo(CWindow* pWindow, const std::string& dir) {
    if (!isDirection(dir))
        return;

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    Vector2D focalPoint;

    switch (dir[0]) {
        case 't':
        case 'u': focalPoint = pWindow->m_vPosition + Vector2D{pWindow->m_vSize.x / 2.f, -1}; break;
        case 'd':
        case 'b': focalPoint = pWindow->m_vPosition + Vector2D{pWindow->m_vSize.x / 2.f, pWindow->m_vSize.y + 1}; break;
        case 'l': focalPoint = pWindow->m_vPosition + Vector2D{-1, pWindow->m_vSize.y / 2.f}; break;
        case 'r': focalPoint = pWindow->m_vPosition + Vector2D{pWindow->m_vSize.x + 1, pWindow->m_vSize.y / 2.f}; break;
        default: UNREACHABLE();
    }

    onWindowRemovedTiling(pWindow);

    m_vOverrideFocalPoint = focalPoint;

    const auto PMONITORFOCAL = g_pCompositor->getMonitorFromVector(focalPoint);

    if (PMONITORFOCAL->ID != pWindow->m_iMonitorID) {
        pWindow->moveToWorkspace(PMONITORFOCAL->activeWorkspace);
        pWindow->m_iMonitorID = PMONITORFOCAL->ID;
    }

    onWindowCreatedTiling(pWindow);

    m_vOverrideFocalPoint.reset();
}

void CHyprDwindleLayout::switchWindows(CWindow* pWindow, CWindow* pWindow2) {
    // windows should be valid, insallah

    auto PNODE  = getNodeFromWindow(pWindow);
    auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    const bool FS1 = pWindow->m_bIsFullscreen;
    const bool FS2 = pWindow2->m_bIsFullscreen;

    if (FS1)
        g_pCompositor->setWindowFullscreen(pWindow, false);
    if (FS2)
        g_pCompositor->setWindowFullscreen(pWindow2, false);

    SDwindleNodeData* ACTIVE1 = nullptr;
    SDwindleNodeData* ACTIVE2 = nullptr;

    // swap the windows and recalc
    PNODE2->pWindow = pWindow;
    PNODE->pWindow  = pWindow2;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_iMonitorID, pWindow->m_iMonitorID);
        std::swap(pWindow2->m_iWorkspaceID, pWindow->m_iWorkspaceID);
    }

    // recalc the workspace
    getMasterNodeOnWorkspace(PNODE->workspaceID)->recalcSizePosRecursive();

    if (PNODE2->workspaceID != PNODE->workspaceID)
        getMasterNodeOnWorkspace(PNODE2->workspaceID)->recalcSizePosRecursive();

    if (ACTIVE1) {
        ACTIVE1->box                  = PNODE->box;
        ACTIVE1->pWindow->m_vPosition = ACTIVE1->box.pos();
        ACTIVE1->pWindow->m_vSize     = ACTIVE1->box.size();
    }

    if (ACTIVE2) {
        ACTIVE2->box                  = PNODE2->box;
        ACTIVE2->pWindow->m_vPosition = ACTIVE2->box.pos();
        ACTIVE2->pWindow->m_vSize     = ACTIVE2->box.size();
    }

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);

    if (FS1)
        g_pCompositor->setWindowFullscreen(pWindow2, true);
    if (FS2)
        g_pCompositor->setWindowFullscreen(pWindow, true);
}

void CHyprDwindleLayout::alterSplitRatio(CWindow* pWindow, float ratio, bool exact) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    float newRatio             = exact ? ratio : PNODE->pParent->splitRatio + ratio;
    PNODE->pParent->splitRatio = std::clamp(newRatio, 0.1f, 1.9f);

    PNODE->pParent->recalcSizePosRecursive();
}

std::any CHyprDwindleLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    const auto ARGS = CVarList(message, 0, ' ');
    if (ARGS[0] == "togglesplit") {
        toggleSplit(header.pWindow);
    } else if (ARGS[0] == "swapsplit") {
        swapSplit(header.pWindow);
    } else if (ARGS[0] == "preselect") {
        std::string direction = ARGS[1];

        if (direction.empty()) {
            Debug::log(ERR, "Expected direction for preselect");
            return "";
        }

        switch (direction.front()) {
            case 'u':
            case 't': {
                overrideDirection = DIRECTION_UP;
                break;
            }
            case 'd':
            case 'b': {
                overrideDirection = DIRECTION_DOWN;
                break;
            }
            case 'r': {
                overrideDirection = DIRECTION_RIGHT;
                break;
            }
            case 'l': {
                overrideDirection = DIRECTION_LEFT;
                break;
            }
            default: {
                // any other character resets the focus direction
                // needed for the persistent mode
                overrideDirection = DIRECTION_DEFAULT;
                break;
            }
        }
    }

    return "";
}

void CHyprDwindleLayout::toggleSplit(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    if (pWindow->m_bIsFullscreen)
        return;

    PNODE->pParent->splitTop = !PNODE->pParent->splitTop;

    PNODE->pParent->recalcSizePosRecursive();
}

void CHyprDwindleLayout::swapSplit(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    if (pWindow->m_bIsFullscreen)
        return;

    std::swap(PNODE->pParent->children[0], PNODE->pParent->children[1]);

    PNODE->pParent->recalcSizePosRecursive();
}

void CHyprDwindleLayout::replaceWindowDataWith(CWindow* from, CWindow* to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE, true);
}

std::string CHyprDwindleLayout::getLayoutName() {
    return "dwindle";
}

void CHyprDwindleLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CHyprDwindleLayout::onDisable() {
    m_lDwindleNodesData.clear();
}

Vector2D CHyprDwindleLayout::predictSizeForNewWindow() {
    if (!g_pCompositor->m_pLastMonitor)
        return {};

    // get window candidate
    CWindow* candidate = g_pCompositor->m_pLastWindow;

    if (!candidate)
        candidate = g_pCompositor->getFirstWindowOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspace);

    // create a fake node
    SDwindleNodeData node;

    if (!candidate)
        return g_pCompositor->m_pLastMonitor->vecSize;
    else {
        const auto PNODE = getNodeFromWindow(candidate);

        if (!PNODE)
            return {};

        node         = *PNODE;
        node.pWindow = nullptr;

        CBox        box = PNODE->box;

        static auto PFLMULT = CConfigValue<Hyprlang::FLOAT>("dwindle:split_width_multiplier");

        bool        splitTop = box.h * *PFLMULT > box.w;

        const auto  SPLITSIDE = !splitTop;

        if (SPLITSIDE)
            node.box = {{}, {box.w / 2.0, box.h}};
        else
            node.box = {{}, {box.w, box.h / 2.0}};

        // TODO: make this better and more accurate

        return node.box.size();
    }

    return {};
}
