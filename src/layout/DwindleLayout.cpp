#include "DwindleLayout.hpp"
#include "../Compositor.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "../render/Renderer.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/EventManager.hpp"

void SDwindleNodeData::recalcSizePosRecursive(bool force, bool horizontalOverride, bool verticalOverride) {
    if (children[0]) {
        static auto PSMARTSPLIT    = CConfigValue<Hyprlang::INT>("dwindle:smart_split");
        static auto PPRESERVESPLIT = CConfigValue<Hyprlang::INT>("dwindle:preserve_split");
        static auto PFLMULT        = CConfigValue<Hyprlang::FLOAT>("dwindle:split_width_multiplier");

        if (*PPRESERVESPLIT == 0 && *PSMARTSPLIT == 0)
            splitTop = box.h * *PFLMULT > box.w;

        if (verticalOverride)
            splitTop = true;
        else if (horizontalOverride)
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

int CHyprDwindleLayout::getNodesOnWorkspace(const WORKSPACEID& id) {
    int no = 0;
    for (auto const& n : m_dwindleNodesData) {
        if (n.workspaceID == id && n.valid)
            ++no;
    }
    return no;
}

SDwindleNodeData* CHyprDwindleLayout::getFirstNodeOnWorkspace(const WORKSPACEID& id) {
    for (auto& n : m_dwindleNodesData) {
        if (n.workspaceID == id && validMapped(n.pWindow))
            return &n;
    }
    return nullptr;
}

SDwindleNodeData* CHyprDwindleLayout::getClosestNodeOnWorkspace(const WORKSPACEID& id, const Vector2D& point) {
    SDwindleNodeData* res         = nullptr;
    double            distClosest = -1;
    for (auto& n : m_dwindleNodesData) {
        if (n.workspaceID == id && validMapped(n.pWindow)) {
            auto distAnother = vecToRectDistanceSquared(point, n.box.pos(), n.box.pos() + n.box.size());
            if (!res || distAnother < distClosest) {
                res         = &n;
                distClosest = distAnother;
            }
        }
    }
    return res;
}

SDwindleNodeData* CHyprDwindleLayout::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& n : m_dwindleNodesData) {
        if (n.pWindow.lock() == pWindow && !n.isNode)
            return &n;
    }

    return nullptr;
}

SDwindleNodeData* CHyprDwindleLayout::getMasterNodeOnWorkspace(const WORKSPACEID& id) {
    for (auto& n : m_dwindleNodesData) {
        if (!n.pParent && n.workspaceID == id)
            return &n;
    }
    return nullptr;
}

void CHyprDwindleLayout::applyNodeDataToWindow(SDwindleNodeData* pNode, bool force) {
    // Don't set nodes, only windows.
    if (pNode->isNode)
        return;

    PHLMONITOR PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto const& m : g_pCompositor->m_monitors) {
            if (m->activeSpecialWorkspaceID() == pNode->workspaceID) {
                PMONITOR = m;
                break;
            }
        }
    } else if (const auto WS = g_pCompositor->getWorkspaceByID(pNode->workspaceID); WS)
        PMONITOR = WS->m_monitor.lock();

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node {}!!", pNode);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->box.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->box.x + pNode->box.w, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->box.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->box.y + pNode->box.h, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);

    const auto PWINDOW = pNode->pWindow.lock();
    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(pNode->workspaceID));

    if (!validMapped(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid {}!!", pNode, PWINDOW);
        onWindowRemovedTiling(PWINDOW);
        return;
    }

    if (PWINDOW->isFullscreen() && !pNode->ignoreFullscreenChecks)
        return;

    PWINDOW->unsetWindowData(PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();

    static auto PGAPSINDATA  = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto* const PGAPSIN      = sc<CCssGapData*>((PGAPSINDATA.ptr())->getData());
    auto* const PGAPSOUT     = sc<CCssGapData*>((PGAPSOUTDATA.ptr())->getData());

    auto        gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto        gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);
    CBox        nodeBox = pNode->box;
    nodeBox.round();

    PWINDOW->m_size     = nodeBox.size();
    PWINDOW->m_position = nodeBox.pos();

    PWINDOW->updateWindowDecos();

    auto              calcPos  = PWINDOW->m_position;
    auto              calcSize = PWINDOW->m_size;

    const static auto REQUESTEDRATIO          = CConfigValue<Hyprlang::VEC2>("dwindle:single_window_aspect_ratio");
    const static auto REQUESTEDRATIOTOLERANCE = CConfigValue<Hyprlang::FLOAT>("dwindle:single_window_aspect_ratio_tolerance");

    Vector2D          ratioPadding;

    if ((*REQUESTEDRATIO).y != 0 && !pNode->pParent) {
        const Vector2D originalSize = PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight;

        const double   requestedRatio = (*REQUESTEDRATIO).x / (*REQUESTEDRATIO).y;
        const double   originalRatio  = originalSize.x / originalSize.y;

        if (requestedRatio > originalRatio) {
            double padding = originalSize.y - (originalSize.x / requestedRatio);

            if (padding / 2 > (*REQUESTEDRATIOTOLERANCE) * originalSize.y)
                ratioPadding = Vector2D{0., padding};
        } else if (requestedRatio < originalRatio) {
            double padding = originalSize.x - (originalSize.y * requestedRatio);

            if (padding / 2 > (*REQUESTEDRATIOTOLERANCE) * originalSize.x)
                ratioPadding = Vector2D{padding, 0.};
        }
    }

    const auto GAPOFFSETTOPLEFT = Vector2D(sc<double>(DISPLAYLEFT ? gapsOut.m_left : gapsIn.m_left), sc<double>(DISPLAYTOP ? gapsOut.m_top : gapsIn.m_top));

    const auto GAPOFFSETBOTTOMRIGHT = Vector2D(sc<double>(DISPLAYRIGHT ? gapsOut.m_right : gapsIn.m_right), sc<double>(DISPLAYBOTTOM ? gapsOut.m_bottom : gapsIn.m_bottom));

    calcPos  = calcPos + GAPOFFSETTOPLEFT + ratioPadding / 2;
    calcSize = calcSize - GAPOFFSETTOPLEFT - GAPOFFSETBOTTOMRIGHT - ratioPadding;

    if (PWINDOW->m_isPseudotiled) {
        // Calculate pseudo
        float scale = 1;

        // adjust if doesn't fit
        if (PWINDOW->m_pseudoSize.x > calcSize.x || PWINDOW->m_pseudoSize.y > calcSize.y) {
            if (PWINDOW->m_pseudoSize.x > calcSize.x) {
                scale = calcSize.x / PWINDOW->m_pseudoSize.x;
            }

            if (PWINDOW->m_pseudoSize.y * scale > calcSize.y) {
                scale = calcSize.y / PWINDOW->m_pseudoSize.y;
            }

            auto DELTA = calcSize - PWINDOW->m_pseudoSize * scale;
            calcSize   = PWINDOW->m_pseudoSize * scale;
            calcPos    = calcPos + DELTA / 2.f; // center
        } else {
            auto DELTA = calcSize - PWINDOW->m_pseudoSize;
            calcPos    = calcPos + DELTA / 2.f; // center
            calcSize   = PWINDOW->m_pseudoSize;
        }
    }

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    Vector2D    availableSpace = calcSize;

    static auto PCLAMP_TILED = CConfigValue<Hyprlang::INT>("misc:size_limits_tiled");

    if (*PCLAMP_TILED) {
        Vector2D minSize = PWINDOW->m_windowData.minSize.valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});
        Vector2D maxSize = PWINDOW->isFullscreen() ? Vector2D{INFINITY, INFINITY} : PWINDOW->m_windowData.maxSize.valueOr(Vector2D{INFINITY, INFINITY});
        calcSize         = calcSize.clamp(minSize, maxSize);

        if (!PWINDOW->onSpecialWorkspace() && !PWINDOW->m_isPseudotiled && (calcSize.x < availableSpace.x || calcSize.y < availableSpace.y))
            calcPos += (availableSpace - calcSize) / 2.0;
    }

    if (PWINDOW->onSpecialWorkspace() && !PWINDOW->isFullscreen()) {
        // if special, we adjust the coords a bit
        static auto PSCALEFACTOR = CConfigValue<Hyprlang::FLOAT>("dwindle:special_scale_factor");

        CBox        wb = {calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f, calcSize * *PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realPosition = wb.pos();
        *PWINDOW->m_realSize     = wb.size();
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realSize     = wb.size();
        *PWINDOW->m_realPosition = wb.pos();
    }

    if (force) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_realPosition->warp();
        PWINDOW->m_realSize->warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

void CHyprDwindleLayout::onWindowCreatedTiling(PHLWINDOW pWindow, eDirection direction) {
    if (pWindow->m_isFloating)
        return;

    m_dwindleNodesData.emplace_back();
    const auto  PNODE = &m_dwindleNodesData.back();

    const auto  PMONITOR = pWindow->m_monitor.lock();

    static auto PUSEACTIVE    = CConfigValue<Hyprlang::INT>("dwindle:use_active_for_splits");
    static auto PDEFAULTSPLIT = CConfigValue<Hyprlang::FLOAT>("dwindle:default_split_ratio");

    if (direction != DIRECTION_DEFAULT && m_overrideDirection == DIRECTION_DEFAULT)
        m_overrideDirection = direction;

    // Populate the node with our window's data
    PNODE->workspaceID = pWindow->workspaceID();
    PNODE->pWindow     = pWindow;
    PNODE->isNode      = false;
    PNODE->layout      = this;

    SDwindleNodeData* OPENINGON;

    const auto        MOUSECOORDS   = m_overrideFocalPoint.value_or(g_pInputManager->getMouseCoordsInternal());
    const auto        MONFROMCURSOR = g_pCompositor->getMonitorFromVector(MOUSECOORDS);

    if (PMONITOR->m_id == MONFROMCURSOR->m_id &&
        (PNODE->workspaceID == PMONITOR->activeWorkspaceID() || (g_pCompositor->isWorkspaceSpecial(PNODE->workspaceID) && PMONITOR->m_activeSpecialWorkspace)) && !*PUSEACTIVE) {
        OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS | SKIP_FULLSCREEN_PRIORITY));

        if (!OPENINGON && g_pCompositor->isPointOnReservedArea(MOUSECOORDS, PMONITOR))
            OPENINGON = getClosestNodeOnWorkspace(PNODE->workspaceID, MOUSECOORDS);

    } else if (*PUSEACTIVE) {
        if (g_pCompositor->m_lastWindow.lock() && !g_pCompositor->m_lastWindow->m_isFloating && g_pCompositor->m_lastWindow.lock() != pWindow &&
            g_pCompositor->m_lastWindow->m_workspace == pWindow->m_workspace && g_pCompositor->m_lastWindow->m_isMapped) {
            OPENINGON = getNodeFromWindow(g_pCompositor->m_lastWindow.lock());
        } else {
            OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowUnified(MOUSECOORDS, RESERVED_EXTENTS | INPUT_EXTENTS));
        }

        if (!OPENINGON && g_pCompositor->isPointOnReservedArea(MOUSECOORDS, PMONITOR))
            OPENINGON = getClosestNodeOnWorkspace(PNODE->workspaceID, MOUSECOORDS);

    } else
        OPENINGON = getFirstNodeOnWorkspace(pWindow->workspaceID());

    Debug::log(LOG, "OPENINGON: {}, Monitor: {}", OPENINGON, PMONITOR->m_id);

    if (OPENINGON && OPENINGON->workspaceID != PNODE->workspaceID) {
        // special workspace handling
        OPENINGON = getFirstNodeOnWorkspace(PNODE->workspaceID);
    }

    // first, check if OPENINGON isn't too big.
    const auto PREDSIZEMAX = OPENINGON ? Vector2D(OPENINGON->box.w, OPENINGON->box.h) : PMONITOR->m_size;
    if (const auto MAXSIZE = pWindow->requestedMaxSize(); MAXSIZE.x < PREDSIZEMAX.x || MAXSIZE.y < PREDSIZEMAX.y) {
        // we can't continue. make it floating.
        pWindow->m_isFloating = true;
        m_dwindleNodesData.remove(*PNODE);
        g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
        return;
    }

    // last fail-safe to avoid duplicate fullscreens
    if ((!OPENINGON || OPENINGON->pWindow.lock() == pWindow) && getNodesOnWorkspace(PNODE->workspaceID) > 1) {
        for (auto& node : m_dwindleNodesData) {
            if (node.workspaceID == PNODE->workspaceID && node.pWindow.lock() && node.pWindow.lock() != pWindow) {
                OPENINGON = &node;
                break;
            }
        }
    }

    // if it's the first, it's easy. Make it fullscreen.
    if (!OPENINGON || OPENINGON->pWindow.lock() == pWindow) {
        PNODE->box = CBox{PMONITOR->m_position + PMONITOR->m_reservedTopLeft, PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight};

        applyNodeDataToWindow(PNODE);

        return;
    }

    // get the node under our cursor

    m_dwindleNodesData.emplace_back();
    const auto NEWPARENT = &m_dwindleNodesData.back();

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
    static auto PSPLITBIAS                 = CConfigValue<Hyprlang::INT>("dwindle:split_bias");

    bool        horizontalOverride = false;
    bool        verticalOverride   = false;

    // let user select position -> top, right, bottom, left
    if (m_overrideDirection != DIRECTION_DEFAULT) {

        // this is horizontal
        if (m_overrideDirection % 2 == 0)
            verticalOverride = true;
        else
            horizontalOverride = true;

        // 0 -> top and left | 1,2 -> right and bottom
        if (m_overrideDirection % 3 == 0) {
            NEWPARENT->children[1] = OPENINGON;
            NEWPARENT->children[0] = PNODE;
        } else {
            NEWPARENT->children[0] = OPENINGON;
            NEWPARENT->children[1] = PNODE;
        }

        // whether or not the override persists after opening one window
        if (*PERMANENTDIRECTIONOVERRIDE == 0)
            m_overrideDirection = DIRECTION_DEFAULT;
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
    } else if (*PFORCESPLIT == 0 || !pWindow->m_firstMap) {
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

    // split in favor of a specific window
    if (*PSPLITBIAS && NEWPARENT->children[0] == PNODE)
        NEWPARENT->splitRatio = 2.f - NEWPARENT->splitRatio;

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

    recalculateMonitor(pWindow->monitorID());
}

void CHyprDwindleLayout::onWindowRemovedTiling(PHLWINDOW pWindow) {

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE) {
        Debug::log(ERR, "onWindowRemovedTiling node null?");
        return;
    }

    pWindow->unsetWindowData(PRIORITY_LAYOUT);
    pWindow->updateWindowData();

    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    const auto PPARENT = PNODE->pParent;

    if (!PPARENT) {
        Debug::log(LOG, "Removing last node (dwindle)");
        m_dwindleNodesData.remove(*PNODE);
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

    m_dwindleNodesData.remove(*PPARENT);
    m_dwindleNodesData.remove(*PNODE);
}

void CHyprDwindleLayout::recalculateMonitor(const MONITORID& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

    if (!PMONITOR || !PMONITOR->m_activeWorkspace)
        return; // ???

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->m_activeSpecialWorkspace)
        calculateWorkspace(PMONITOR->m_activeSpecialWorkspace);

    calculateWorkspace(PMONITOR->m_activeWorkspace);
}

void CHyprDwindleLayout::calculateWorkspace(const PHLWORKSPACE& pWorkspace) {
    const auto PMONITOR = pWorkspace->m_monitor.lock();

    if (!PMONITOR)
        return;

    if (pWorkspace->m_hasFullscreenWindow) {
        // massive hack from the fullscreen func
        const auto PFULLWINDOW = pWorkspace->getFullscreenWindow();

        if (pWorkspace->m_fullscreenMode == FSMODE_FULLSCREEN) {
            *PFULLWINDOW->m_realPosition = PMONITOR->m_position;
            *PFULLWINDOW->m_realSize     = PMONITOR->m_size;
        } else if (pWorkspace->m_fullscreenMode == FSMODE_MAXIMIZED) {
            SDwindleNodeData fakeNode;
            fakeNode.pWindow        = PFULLWINDOW;
            fakeNode.box            = {PMONITOR->m_position + PMONITOR->m_reservedTopLeft, PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight};
            fakeNode.workspaceID    = pWorkspace->m_id;
            PFULLWINDOW->m_position = fakeNode.box.pos();
            PFULLWINDOW->m_size     = fakeNode.box.size();
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }

        // if has fullscreen, don't calculate the rest
        return;
    }

    const auto TOPNODE = getMasterNodeOnWorkspace(pWorkspace->m_id);

    if (TOPNODE) {
        TOPNODE->box = {PMONITOR->m_position + PMONITOR->m_reservedTopLeft, PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight};
        TOPNODE->recalcSizePosRecursive();
    }
}

bool CHyprDwindleLayout::isWindowTiled(PHLWINDOW pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprDwindleLayout::onBeginDragWindow() {
    m_pseudoDragFlags.started = false;
    m_pseudoDragFlags.pseudo  = false;
    IHyprLayout::onBeginDragWindow();
}

void CHyprDwindleLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, PHLWINDOW pWindow) {

    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_lastWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        *PWINDOW->m_realSize =
            (PWINDOW->m_realSize->goal() + pixResize)
                .clamp(PWINDOW->m_windowData.minSize.valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), PWINDOW->m_windowData.maxSize.valueOr(Vector2D{INFINITY, INFINITY}));
        PWINDOW->updateWindowDecos();
        return;
    }

    static auto PANIMATE       = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");
    static auto PSMARTRESIZING = CConfigValue<Hyprlang::INT>("dwindle:smart_resizing");

    // get some data about our window
    const auto PMONITOR      = PWINDOW->m_monitor.lock();
    const bool DISPLAYLEFT   = STICKS(PWINDOW->m_position.x, PMONITOR->m_position.x + PMONITOR->m_reservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(PWINDOW->m_position.x + PWINDOW->m_size.x, PMONITOR->m_position.x + PMONITOR->m_size.x - PMONITOR->m_reservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(PWINDOW->m_position.y, PMONITOR->m_position.y + PMONITOR->m_reservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(PWINDOW->m_position.y + PWINDOW->m_size.y, PMONITOR->m_position.y + PMONITOR->m_size.y - PMONITOR->m_reservedBottomRight.y);

    if (PWINDOW->m_isPseudotiled) {
        if (!m_pseudoDragFlags.started) {
            m_pseudoDragFlags.started = true;

            const auto pseudoSize  = PWINDOW->m_realSize->goal();
            const auto mouseOffset = g_pInputManager->getMouseCoordsInternal() - (PNODE->box.pos() + ((PNODE->box.size() / 2) - (pseudoSize / 2)));

            if (mouseOffset.x > 0 && mouseOffset.x < pseudoSize.x && mouseOffset.y > 0 && mouseOffset.y < pseudoSize.y) {
                m_pseudoDragFlags.pseudo  = true;
                m_pseudoDragFlags.xExtent = mouseOffset.x > pseudoSize.x / 2;
                m_pseudoDragFlags.yExtent = mouseOffset.y > pseudoSize.y / 2;

                PWINDOW->m_pseudoSize = pseudoSize;
            } else {
                m_pseudoDragFlags.pseudo = false;
            }
        }

        if (m_pseudoDragFlags.pseudo) {
            if (m_pseudoDragFlags.xExtent)
                PWINDOW->m_pseudoSize.x += pixResize.x * 2;
            else
                PWINDOW->m_pseudoSize.x -= pixResize.x * 2;
            if (m_pseudoDragFlags.yExtent)
                PWINDOW->m_pseudoSize.y += pixResize.y * 2;
            else
                PWINDOW->m_pseudoSize.y -= pixResize.y * 2;

            CBox wbox = PNODE->box;
            wbox.round();

            Vector2D minSize    = PWINDOW->m_windowData.minSize.valueOr(Vector2D{30.0, 30.0});
            Vector2D maxSize    = PWINDOW->m_windowData.maxSize.valueOr(Vector2D{INFINITY, INFINITY});
            Vector2D upperBound = Vector2D{std::min(maxSize.x, wbox.w), std::min(maxSize.y, wbox.h)};

            PWINDOW->m_pseudoSize = PWINDOW->m_pseudoSize.clamp(minSize, upperBound);

            PWINDOW->m_lastFloatingSize = PWINDOW->m_pseudoSize;
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

void CHyprDwindleLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) {
    const auto PMONITOR   = pWindow->m_monitor.lock();
    const auto PWORKSPACE = pWindow->m_workspace;

    // save position and size if floating
    if (pWindow->m_isFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
        pWindow->m_lastFloatingSize     = pWindow->m_realSize->goal();
        pWindow->m_lastFloatingPosition = pWindow->m_realPosition->goal();
        pWindow->m_position             = pWindow->m_realPosition->goal();
        pWindow->m_size                 = pWindow->m_realSize->goal();
    }

    if (EFFECTIVE_MODE == FSMODE_NONE) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            *pWindow->m_realPosition = pWindow->m_lastFloatingPosition;
            *pWindow->m_realSize     = pWindow->m_lastFloatingSize;

            pWindow->unsetWindowData(PRIORITY_LAYOUT);
            pWindow->updateWindowData();
        }
    } else {
        // apply new pos and size being monitors' box
        if (EFFECTIVE_MODE == FSMODE_FULLSCREEN) {
            *pWindow->m_realPosition = PMONITOR->m_position;
            *pWindow->m_realSize     = PMONITOR->m_size;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SDwindleNodeData fakeNode;
            fakeNode.pWindow     = pWindow;
            fakeNode.box         = {PMONITOR->m_position + PMONITOR->m_reservedTopLeft, PMONITOR->m_size - PMONITOR->m_reservedTopLeft - PMONITOR->m_reservedBottomRight};
            fakeNode.workspaceID = pWindow->workspaceID();
            pWindow->m_position  = fakeNode.box.pos();
            pWindow->m_size      = fakeNode.box.size();
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->changeWindowZOrder(pWindow, true);
}

void CHyprDwindleLayout::recalculateWindow(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    PNODE->recalcSizePosRecursive();
}

SWindowRenderLayoutHints CHyprDwindleLayout::requestRenderHints(PHLWINDOW pWindow) {
    // window should be valid, insallah
    SWindowRenderLayoutHints hints;

    const auto               PNODE = getNodeFromWindow(pWindow);
    if (!PNODE)
        return hints; // left for the future, maybe floating funkiness

    return hints;
}

void CHyprDwindleLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
    if (!isDirection(dir))
        return;

    const auto     PNODE               = getNodeFromWindow(pWindow);
    const auto     originalWorkspaceID = pWindow->workspaceID();
    const Vector2D originalPos         = pWindow->middle();

    if (!PNODE || !pWindow->m_monitor)
        return;

    Vector2D   focalPoint;

    const auto WINDOWIDEALBB = pWindow->isFullscreen() ? CBox{pWindow->m_monitor->m_position, pWindow->m_monitor->m_size} : pWindow->getWindowIdealBoundingBoxIgnoreReserved();

    switch (dir[0]) {
        case 't':
        case 'u': focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, -1.0}; break;
        case 'd':
        case 'b': focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x / 2.0, WINDOWIDEALBB.size().y + 1.0}; break;
        case 'l': focalPoint = WINDOWIDEALBB.pos() + Vector2D{-1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        case 'r': focalPoint = WINDOWIDEALBB.pos() + Vector2D{WINDOWIDEALBB.size().x + 1.0, WINDOWIDEALBB.size().y / 2.0}; break;
        default: UNREACHABLE();
    }

    pWindow->setAnimationsToMove();

    onWindowRemovedTiling(pWindow);

    m_overrideFocalPoint = focalPoint;

    const auto PMONITORFOCAL = g_pCompositor->getMonitorFromVector(focalPoint);

    if (PMONITORFOCAL != pWindow->m_monitor) {
        pWindow->moveToWorkspace(PMONITORFOCAL->m_activeWorkspace);
        pWindow->m_monitor = PMONITORFOCAL;
    }

    pWindow->updateGroupOutputs();
    if (!pWindow->m_groupData.pNextWindow.expired()) {
        PHLWINDOW next = pWindow->m_groupData.pNextWindow.lock();
        while (next != pWindow) {
            next->updateToplevel();
            next = next->m_groupData.pNextWindow.lock();
        }
    }

    onWindowCreatedTiling(pWindow);

    m_overrideFocalPoint.reset();

    // restore focus to the previous position
    if (silent) {
        const auto PNODETOFOCUS = getClosestNodeOnWorkspace(originalWorkspaceID, originalPos);
        if (PNODETOFOCUS && PNODETOFOCUS->pWindow.lock())
            g_pCompositor->focusWindow(PNODETOFOCUS->pWindow.lock());
    }
}

void CHyprDwindleLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
    // windows should be valid, insallah

    auto PNODE  = getNodeFromWindow(pWindow);
    auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    const eFullscreenMode MODE1 = pWindow->m_fullscreenState.internal;
    const eFullscreenMode MODE2 = pWindow2->m_fullscreenState.internal;

    g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);
    g_pCompositor->setWindowFullscreenInternal(pWindow2, FSMODE_NONE);

    SDwindleNodeData* ACTIVE1 = nullptr;
    SDwindleNodeData* ACTIVE2 = nullptr;

    // swap the windows and recalc
    PNODE2->pWindow = pWindow;
    PNODE->pWindow  = pWindow2;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_monitor, pWindow->m_monitor);
        std::swap(pWindow2->m_workspace, pWindow->m_workspace);
    }

    pWindow->setAnimationsToMove();
    pWindow2->setAnimationsToMove();

    // recalc the workspace
    getMasterNodeOnWorkspace(PNODE->workspaceID)->recalcSizePosRecursive();

    if (PNODE2->workspaceID != PNODE->workspaceID)
        getMasterNodeOnWorkspace(PNODE2->workspaceID)->recalcSizePosRecursive();

    if (ACTIVE1) {
        ACTIVE1->box                 = PNODE->box;
        ACTIVE1->pWindow->m_position = ACTIVE1->box.pos();
        ACTIVE1->pWindow->m_size     = ACTIVE1->box.size();
    }

    if (ACTIVE2) {
        ACTIVE2->box                 = PNODE2->box;
        ACTIVE2->pWindow->m_position = ACTIVE2->box.pos();
        ACTIVE2->pWindow->m_size     = ACTIVE2->box.size();
    }

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);

    g_pCompositor->setWindowFullscreenInternal(pWindow2, MODE1);
    g_pCompositor->setWindowFullscreenInternal(pWindow, MODE2);
}

void CHyprDwindleLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
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
    } else if (ARGS[0] == "movetoroot") {
        const auto WINDOW = ARGS[1].empty() ? header.pWindow : g_pCompositor->getWindowByRegex(ARGS[1]);
        const auto STABLE = ARGS[2].empty() || ARGS[2] != "unstable";
        moveToRoot(WINDOW, STABLE);
    } else if (ARGS[0] == "preselect") {
        std::string direction = ARGS[1];

        if (direction.empty()) {
            Debug::log(ERR, "Expected direction for preselect");
            return "";
        }

        switch (direction.front()) {
            case 'u':
            case 't': {
                m_overrideDirection = DIRECTION_UP;
                break;
            }
            case 'd':
            case 'b': {
                m_overrideDirection = DIRECTION_DOWN;
                break;
            }
            case 'r': {
                m_overrideDirection = DIRECTION_RIGHT;
                break;
            }
            case 'l': {
                m_overrideDirection = DIRECTION_LEFT;
                break;
            }
            default: {
                // any other character resets the focus direction
                // needed for the persistent mode
                m_overrideDirection = DIRECTION_DEFAULT;
                break;
            }
        }
    }

    return "";
}

void CHyprDwindleLayout::toggleSplit(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    if (pWindow->isFullscreen())
        return;

    PNODE->pParent->splitTop = !PNODE->pParent->splitTop;

    PNODE->pParent->recalcSizePosRecursive();
}

void CHyprDwindleLayout::swapSplit(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    if (pWindow->isFullscreen())
        return;

    std::swap(PNODE->pParent->children[0], PNODE->pParent->children[1]);

    PNODE->pParent->recalcSizePosRecursive();
}

// goal: maximize the chosen window within current dwindle layout
// impl: swap the selected window with the other sub-tree below root
void CHyprDwindleLayout::moveToRoot(PHLWINDOW pWindow, bool stable) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE || !PNODE->pParent)
        return;

    if (pWindow->isFullscreen())
        return;

    // already at root
    if (!PNODE->pParent->pParent)
        return;

    auto& pNode = PNODE->pParent->children[0] == PNODE ? PNODE->pParent->children[0] : PNODE->pParent->children[1];

    // instead of [getMasterNodeOnWorkspace], we walk back to root since we need
    // to know which children of root is our ancestor
    auto pAncestor = PNODE, pRoot = PNODE->pParent;
    while (pRoot->pParent) {
        pAncestor = pRoot;
        pRoot     = pRoot->pParent;
    }

    auto& pSwap = pRoot->children[0] == pAncestor ? pRoot->children[1] : pRoot->children[0];
    std::swap(pNode, pSwap);
    std::swap(pNode->pParent, pSwap->pParent);

    // [stable] in that the focused window occupies same side of screen
    if (stable)
        std::swap(pRoot->children[0], pRoot->children[1]);

    // if the workspace is visible, recalculate layout
    if (pWindow->m_workspace && pWindow->m_workspace->isVisible())
        pRoot->recalcSizePosRecursive();
}

void CHyprDwindleLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
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
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_isFloating || !w->m_isMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w);
    }
}

void CHyprDwindleLayout::onDisable() {
    m_dwindleNodesData.clear();
}

Vector2D CHyprDwindleLayout::predictSizeForNewWindowTiled() {
    if (!g_pCompositor->m_lastMonitor)
        return {};

    // get window candidate
    PHLWINDOW candidate = g_pCompositor->m_lastWindow.lock();

    if (!candidate)
        candidate = g_pCompositor->m_lastMonitor->m_activeWorkspace->getFirstWindow();

    // create a fake node
    SDwindleNodeData node;

    if (!candidate)
        return g_pCompositor->m_lastMonitor->m_size;
    else {
        const auto PNODE = getNodeFromWindow(candidate);

        if (!PNODE)
            return {};

        node = *PNODE;
        node.pWindow.reset();

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
