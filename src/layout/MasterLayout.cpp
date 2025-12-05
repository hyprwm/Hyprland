#include "MasterLayout.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "config/ConfigDataValues.hpp"
#include <ranges>
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../render/Renderer.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/LayoutManager.hpp"
#include "../managers/EventManager.hpp"
#include "../desktop/state/FocusState.hpp"
#include "xwayland/XWayland.hpp"

SMasterNodeData* CHyprMasterLayout::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& nd : m_masterNodesData) {
        if (nd.pWindow.lock() == pWindow)
            return &nd;
    }

    return nullptr;
}

int CHyprMasterLayout::getNodesOnWorkspace(const WORKSPACEID& ws) {
    int no = 0;
    for (auto const& n : m_masterNodesData) {
        if (n.workspaceID == ws)
            no++;
    }

    return no;
}

int CHyprMasterLayout::getMastersOnWorkspace(const WORKSPACEID& ws) {
    int no = 0;
    for (auto const& n : m_masterNodesData) {
        if (n.workspaceID == ws && n.isMaster)
            no++;
    }

    return no;
}

SMasterWorkspaceData* CHyprMasterLayout::getMasterWorkspaceData(const WORKSPACEID& ws) {
    for (auto& n : m_masterWorkspacesData) {
        if (n.workspaceID == ws)
            return &n;
    }

    //create on the fly if it doesn't exist yet
    const auto PWORKSPACEDATA   = &m_masterWorkspacesData.emplace_back();
    PWORKSPACEDATA->workspaceID = ws;
    static auto PORIENTATION    = CConfigValue<std::string>("master:orientation");

    if (*PORIENTATION == "top")
        PWORKSPACEDATA->orientation = ORIENTATION_TOP;
    else if (*PORIENTATION == "right")
        PWORKSPACEDATA->orientation = ORIENTATION_RIGHT;
    else if (*PORIENTATION == "bottom")
        PWORKSPACEDATA->orientation = ORIENTATION_BOTTOM;
    else if (*PORIENTATION == "center")
        PWORKSPACEDATA->orientation = ORIENTATION_CENTER;
    else
        PWORKSPACEDATA->orientation = ORIENTATION_LEFT;

    return PWORKSPACEDATA;
}

std::string CHyprMasterLayout::getLayoutName() {
    return "Master";
}

SMasterNodeData* CHyprMasterLayout::getMasterNodeOnWorkspace(const WORKSPACEID& ws) {
    for (auto& n : m_masterNodesData) {
        if (n.isMaster && n.workspaceID == ws)
            return &n;
    }

    return nullptr;
}

void CHyprMasterLayout::onWindowCreatedTiling(PHLWINDOW pWindow, eDirection direction) {
    if (pWindow->m_isFloating)
        return;

    static auto PNEWONACTIVE = CConfigValue<std::string>("master:new_on_active");
    static auto PNEWONTOP    = CConfigValue<Hyprlang::INT>("master:new_on_top");
    static auto PNEWSTATUS   = CConfigValue<std::string>("master:new_status");

    const auto  PMONITOR = pWindow->m_monitor.lock();

    const bool  BNEWBEFOREACTIVE = *PNEWONACTIVE == "before";
    const bool  BNEWISMASTER     = *PNEWSTATUS == "master";

    const auto  PNODE = [&]() {
        if (*PNEWONACTIVE != "none" && !BNEWISMASTER) {
            const auto pLastNode = getNodeFromWindow(Desktop::focusState()->window());
            if (pLastNode && !(pLastNode->isMaster && (getMastersOnWorkspace(pWindow->workspaceID()) == 1 || *PNEWSTATUS == "slave"))) {
                auto it = std::ranges::find(m_masterNodesData, *pLastNode);
                if (!BNEWBEFOREACTIVE)
                    ++it;
                return &(*m_masterNodesData.emplace(it));
            }
        }
        return *PNEWONTOP ? &m_masterNodesData.emplace_front() : &m_masterNodesData.emplace_back();
    }();

    PNODE->workspaceID = pWindow->workspaceID();
    PNODE->pWindow     = pWindow;

    const auto   WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    static auto  PMFACT             = CConfigValue<Hyprlang::FLOAT>("master:mfact");
    float        lastSplitPercent   = *PMFACT;

    auto         OPENINGON = isWindowTiled(Desktop::focusState()->window()) && Desktop::focusState()->window()->m_workspace == pWindow->m_workspace ?
                getNodeFromWindow(Desktop::focusState()->window()) :
                getMasterNodeOnWorkspace(pWindow->workspaceID());

    const auto   MOUSECOORDS   = g_pInputManager->getMouseCoordsInternal();
    static auto  PDROPATCURSOR = CConfigValue<Hyprlang::INT>("master:drop_at_cursor");
    eOrientation orientation   = getDynamicOrientation(pWindow->m_workspace);
    const auto   NODEIT        = std::ranges::find(m_masterNodesData, *PNODE);

    bool         forceDropAsMaster = false;
    // if dragging window to move, drop it at the cursor position instead of bottom/top of stack
    if (*PDROPATCURSOR && g_pInputManager->m_dragMode == MBIND_MOVE) {
        if (WINDOWSONWORKSPACE > 2) {
            for (auto it = m_masterNodesData.begin(); it != m_masterNodesData.end(); ++it) {
                if (it->workspaceID != pWindow->workspaceID())
                    continue;
                const CBox box = it->pWindow->getWindowIdealBoundingBoxIgnoreReserved();
                if (box.containsPoint(MOUSECOORDS)) {
                    switch (orientation) {
                        case ORIENTATION_LEFT:
                        case ORIENTATION_RIGHT:
                            if (MOUSECOORDS.y > it->pWindow->middle().y)
                                ++it;
                            break;
                        case ORIENTATION_TOP:
                        case ORIENTATION_BOTTOM:
                            if (MOUSECOORDS.x > it->pWindow->middle().x)
                                ++it;
                            break;
                        case ORIENTATION_CENTER: break;
                        default: UNREACHABLE();
                    }
                    m_masterNodesData.splice(it, m_masterNodesData, NODEIT);
                    break;
                }
            }
        } else if (WINDOWSONWORKSPACE == 2) {
            // when dropping as the second tiled window in the workspace,
            // make it the master only if the cursor is on the master side of the screen
            for (auto const& nd : m_masterNodesData) {
                if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                    switch (orientation) {
                        case ORIENTATION_LEFT:
                        case ORIENTATION_CENTER:
                            if (MOUSECOORDS.x < nd.pWindow->middle().x)
                                forceDropAsMaster = true;
                            break;
                        case ORIENTATION_RIGHT:
                            if (MOUSECOORDS.x > nd.pWindow->middle().x)
                                forceDropAsMaster = true;
                            break;
                        case ORIENTATION_TOP:
                            if (MOUSECOORDS.y < nd.pWindow->middle().y)
                                forceDropAsMaster = true;
                            break;
                        case ORIENTATION_BOTTOM:
                            if (MOUSECOORDS.y > nd.pWindow->middle().y)
                                forceDropAsMaster = true;
                            break;
                        default: UNREACHABLE();
                    }
                    break;
                }
            }
        }
    }

    if ((BNEWISMASTER && g_pInputManager->m_dragMode != MBIND_MOVE)                             //
        || WINDOWSONWORKSPACE == 1                                                              //
        || (WINDOWSONWORKSPACE > 2 && !pWindow->m_firstMap && OPENINGON && OPENINGON->isMaster) //
        || forceDropAsMaster                                                                    //
        || (*PNEWSTATUS == "inherit" && OPENINGON && OPENINGON->isMaster && g_pInputManager->m_dragMode != MBIND_MOVE)) {

        if (BNEWBEFOREACTIVE) {
            for (auto& nd : m_masterNodesData | std::views::reverse) {
                if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                    nd.isMaster      = false;
                    lastSplitPercent = nd.percMaster;
                    break;
                }
            }
        } else {
            for (auto& nd : m_masterNodesData) {
                if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                    nd.isMaster      = false;
                    lastSplitPercent = nd.percMaster;
                    break;
                }
            }
        }

        PNODE->isMaster   = true;
        PNODE->percMaster = lastSplitPercent;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize(); MAXSIZE.x < PMONITOR->m_size.x * lastSplitPercent || MAXSIZE.y < PMONITOR->m_size.y) {
            // we can't continue. make it floating.
            pWindow->m_isFloating = true;
            m_masterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        PNODE->isMaster   = false;
        PNODE->percMaster = lastSplitPercent;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = pWindow->requestedMaxSize();
            MAXSIZE.x < PMONITOR->m_size.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->m_size.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_isFloating = true;
            m_masterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    // recalc
    recalculateMonitor(pWindow->monitorID());
    pWindow->m_workspace->updateWindows();
}

void CHyprMasterLayout::onWindowRemovedTiling(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto  WORKSPACEID = PNODE->workspaceID;
    const auto  MASTERSLEFT = getMastersOnWorkspace(WORKSPACEID);
    static auto SMALLSPLIT  = CConfigValue<Hyprlang::INT>("master:allow_small_split");

    pWindow->m_ruleApplicator->resetProps(Desktop::Rule::RULE_PROP_ALL, Desktop::Types::PRIORITY_LAYOUT);
    pWindow->updateWindowData();

    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

    if (PNODE->isMaster && (MASTERSLEFT <= 1 || *SMALLSPLIT == 1)) {
        // find a new master from top of the list
        for (auto& nd : m_masterNodesData) {
            if (!nd.isMaster && nd.workspaceID == WORKSPACEID) {
                nd.isMaster   = true;
                nd.percMaster = PNODE->percMaster;
                break;
            }
        }
    }

    m_masterNodesData.remove(*PNODE);

    if (getMastersOnWorkspace(WORKSPACEID) == getNodesOnWorkspace(WORKSPACEID) && MASTERSLEFT > 1) {
        for (auto& nd : m_masterNodesData | std::views::reverse) {
            if (nd.workspaceID == WORKSPACEID) {
                nd.isMaster = false;
                break;
            }
        }
    }
    // BUGFIX: correct bug where closing one master in a stack of 2 would leave
    // the screen half bare, and make it difficult to select remaining window
    if (getNodesOnWorkspace(WORKSPACEID) == 1) {
        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID == WORKSPACEID && !nd.isMaster) {
                nd.isMaster = true;
                break;
            }
        }
    }
    recalculateMonitor(pWindow->monitorID());
    pWindow->m_workspace->updateWindows();
}

void CHyprMasterLayout::recalculateMonitor(const MONITORID& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

    if (!PMONITOR || !PMONITOR->m_activeWorkspace)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->m_activeSpecialWorkspace)
        calculateWorkspace(PMONITOR->m_activeSpecialWorkspace);

    calculateWorkspace(PMONITOR->m_activeWorkspace);

#ifndef NO_XWAYLAND
    CBox box = g_pCompositor->calculateX11WorkArea();
    if (!g_pXWayland || !g_pXWayland->m_wm)
        return;
    g_pXWayland->m_wm->updateWorkArea(box.x, box.y, box.w, box.h);
#endif
}

void CHyprMasterLayout::calculateWorkspace(PHLWORKSPACE pWorkspace) {
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
            SMasterNodeData fakeNode;
            fakeNode.pWindow                = PFULLWINDOW;
            const auto WORKAREA             = PMONITOR->logicalBoxMinusReserved();
            fakeNode.position               = WORKAREA.pos();
            fakeNode.size                   = WORKAREA.size();
            fakeNode.workspaceID            = pWorkspace->m_id;
            PFULLWINDOW->m_position         = fakeNode.position;
            PFULLWINDOW->m_size             = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }

        // if has fullscreen, don't calculate the rest
        return;
    }

    const auto PMASTERNODE = getMasterNodeOnWorkspace(pWorkspace->m_id);

    if (!PMASTERNODE)
        return;

    eOrientation orientation         = getDynamicOrientation(pWorkspace);
    bool         centerMasterWindow  = false;
    static auto  SLAVECOUNTFORCENTER = CConfigValue<Hyprlang::INT>("master:slave_count_for_center_master");
    static auto  CMFALLBACK          = CConfigValue<std::string>("master:center_master_fallback");
    static auto  PIGNORERESERVED     = CConfigValue<Hyprlang::INT>("master:center_ignores_reserved");
    static auto  PSMARTRESIZING      = CConfigValue<Hyprlang::INT>("master:smart_resizing");

    const auto   MASTERS      = getMastersOnWorkspace(pWorkspace->m_id);
    const auto   WINDOWS      = getNodesOnWorkspace(pWorkspace->m_id);
    const auto   STACKWINDOWS = WINDOWS - MASTERS;
    const auto   WORKAREA     = workAreaOnWorkspace(pWorkspace);

    if (orientation == ORIENTATION_CENTER) {
        if (STACKWINDOWS >= *SLAVECOUNTFORCENTER)
            centerMasterWindow = true;
        else {
            if (*CMFALLBACK == "left")
                orientation = ORIENTATION_LEFT;
            else if (*CMFALLBACK == "right")
                orientation = ORIENTATION_RIGHT;
            else if (*CMFALLBACK == "top")
                orientation = ORIENTATION_TOP;
            else if (*CMFALLBACK == "bottom")
                orientation = ORIENTATION_BOTTOM;
            else
                orientation = ORIENTATION_LEFT;
        }
    }

    const float totalSize             = (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) ? WORKAREA.w : WORKAREA.h;
    const float masterAverageSize     = totalSize / MASTERS;
    const float slaveAverageSize      = totalSize / STACKWINDOWS;
    float       masterAccumulatedSize = 0;
    float       slaveAccumulatedSize  = 0;

    if (*PSMARTRESIZING) {
        // check the total width and height so that later
        // if larger/smaller than screen size them down/up
        for (auto const& nd : m_masterNodesData) {
            if (nd.workspaceID == pWorkspace->m_id) {
                if (nd.isMaster)
                    masterAccumulatedSize += totalSize / MASTERS * nd.percSize;
                else
                    slaveAccumulatedSize += totalSize / STACKWINDOWS * nd.percSize;
            }
        }
    }

    // compute placement of master window(s)
    if (WINDOWS == 1 && !centerMasterWindow) {
        static auto PALWAYSKEEPPOSITION = CConfigValue<Hyprlang::INT>("master:always_keep_position");
        if (*PALWAYSKEEPPOSITION) {
            const float WIDTH = WORKAREA.w * PMASTERNODE->percMaster;
            float       nextX = 0;

            if (orientation == ORIENTATION_RIGHT)
                nextX = WORKAREA.w - WIDTH;
            else if (orientation == ORIENTATION_CENTER)
                nextX = (WORKAREA.w - WIDTH) / 2;

            PMASTERNODE->size     = Vector2D(WIDTH, WORKAREA.h);
            PMASTERNODE->position = WORKAREA.pos() + Vector2D(nextX, 0.0);
        } else {
            PMASTERNODE->size     = WORKAREA.size();
            PMASTERNODE->position = WORKAREA.pos();
        }

        applyNodeDataToWindow(PMASTERNODE);
        return;
    } else if (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) {
        const float HEIGHT      = STACKWINDOWS != 0 ? WORKAREA.h * PMASTERNODE->percMaster : WORKAREA.h;
        float       widthLeft   = WORKAREA.w;
        int         mastersLeft = MASTERS;
        float       nextX       = 0;
        float       nextY       = 0;

        if (orientation == ORIENTATION_BOTTOM)
            nextY = WORKAREA.h - HEIGHT;

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || !nd.isMaster)
                continue;

            float WIDTH = mastersLeft > 1 ? widthLeft / mastersLeft * nd.percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && mastersLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WORKAREA.w / masterAccumulatedSize;
                WIDTH = masterAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WORKAREA.pos() + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            mastersLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else { // orientation left, right or center
        float WIDTH       = *PIGNORERESERVED && centerMasterWindow ? PMONITOR->m_size.x : WORKAREA.w;
        float heightLeft  = WORKAREA.h;
        int   mastersLeft = MASTERS;
        float nextX       = 0;
        float nextY       = 0;

        if (STACKWINDOWS > 0 || centerMasterWindow)
            WIDTH *= PMASTERNODE->percMaster;

        if (orientation == ORIENTATION_RIGHT)
            nextX = WORKAREA.w - WIDTH;
        else if (centerMasterWindow)
            nextX = ((*PIGNORERESERVED && centerMasterWindow ? PMONITOR->m_size.x : WORKAREA.w) - WIDTH) / 2;

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || !nd.isMaster)
                continue;

            float HEIGHT = mastersLeft > 1 ? heightLeft / mastersLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && mastersLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WORKAREA.h / masterAccumulatedSize;
                HEIGHT = masterAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = (*PIGNORERESERVED && centerMasterWindow ? PMONITOR->m_position : WORKAREA.pos()) + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            mastersLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;
        }
    }

    if (STACKWINDOWS == 0)
        return;

    // compute placement of slave window(s)
    int slavesLeft = STACKWINDOWS;
    if (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) {
        const float HEIGHT    = WORKAREA.h - PMASTERNODE->size.y;
        float       widthLeft = WORKAREA.w;
        float       nextX     = 0;
        float       nextY     = 0;

        if (orientation == ORIENTATION_TOP)
            nextY = PMASTERNODE->size.y;

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                continue;

            float WIDTH = slavesLeft > 1 ? widthLeft / slavesLeft * nd.percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && slavesLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WORKAREA.w / slaveAccumulatedSize;
                WIDTH = slaveAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WORKAREA.pos() + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            slavesLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else if (orientation == ORIENTATION_LEFT || orientation == ORIENTATION_RIGHT) {
        const float WIDTH      = WORKAREA.w - PMASTERNODE->size.x;
        float       heightLeft = WORKAREA.h;
        float       nextY      = 0;
        float       nextX      = 0;

        if (orientation == ORIENTATION_LEFT)
            nextX = PMASTERNODE->size.x;

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                continue;

            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WORKAREA.h / slaveAccumulatedSize;
                HEIGHT = slaveAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WORKAREA.pos() + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            slavesLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;
        }
    } else { // slaves for centered master window(s)
        const float WIDTH       = ((*PIGNORERESERVED ? PMONITOR->m_size.x : WORKAREA.w) - PMASTERNODE->size.x) / 2.0;
        float       heightLeft  = 0;
        float       heightLeftL = WORKAREA.h;
        float       heightLeftR = WORKAREA.h;
        float       nextX       = 0;
        float       nextY       = 0;
        float       nextYL      = 0;
        float       nextYR      = 0;
        bool        onRight     = *CMFALLBACK == "right";
        int         slavesLeftL = 1 + (slavesLeft - 1) / 2;
        int         slavesLeftR = slavesLeft - slavesLeftL;

        if (onRight) {
            slavesLeftR = 1 + (slavesLeft - 1) / 2;
            slavesLeftL = slavesLeft - slavesLeftR;
        }

        const float slaveAverageHeightL     = WORKAREA.h / slavesLeftL;
        const float slaveAverageHeightR     = WORKAREA.h / slavesLeftR;
        float       slaveAccumulatedHeightL = 0;
        float       slaveAccumulatedHeightR = 0;

        if (*PSMARTRESIZING) {
            for (auto const& nd : m_masterNodesData) {
                if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                    continue;

                if (onRight) {
                    slaveAccumulatedHeightR += slaveAverageHeightR * nd.percSize;
                } else {
                    slaveAccumulatedHeightL += slaveAverageHeightL * nd.percSize;
                }
                onRight = !onRight;
            }

            onRight = *CMFALLBACK == "right";
        }

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID != pWorkspace->m_id || nd.isMaster)
                continue;

            if (onRight) {
                nextX      = WIDTH + PMASTERNODE->size.x - (*PIGNORERESERVED ? PMONITOR->m_reservedArea.left() : 0);
                nextY      = nextYR;
                heightLeft = heightLeftR;
                slavesLeft = slavesLeftR;
            } else {
                nextX      = 0;
                nextY      = nextYL;
                heightLeft = heightLeftL;
                slavesLeft = slavesLeftL;
            }

            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                if (onRight) {
                    nd.percSize *= WORKAREA.h / slaveAccumulatedHeightR;
                    HEIGHT = slaveAverageHeightR * nd.percSize;
                } else {
                    nd.percSize *= WORKAREA.h / slaveAccumulatedHeightL;
                    HEIGHT = slaveAverageHeightL * nd.percSize;
                }
            }

            nd.size     = Vector2D(*PIGNORERESERVED ? (WIDTH - (onRight ? PMONITOR->m_reservedArea.right() : PMONITOR->m_reservedArea.left())) : WIDTH, HEIGHT);
            nd.position = WORKAREA.pos() + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            if (onRight) {
                heightLeftR -= HEIGHT;
                nextYR += HEIGHT;
                slavesLeftR--;
            } else {
                heightLeftL -= HEIGHT;
                nextYL += HEIGHT;
                slavesLeftL--;
            }

            onRight = !onRight;
        }
    }
}

void CHyprMasterLayout::applyNodeDataToWindow(SMasterNodeData* pNode) {
    PHLMONITOR PMONITOR = nullptr;

    const auto WS = g_pCompositor->getWorkspaceByID(pNode->workspaceID);

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto const& m : g_pCompositor->m_monitors) {
            if (m->activeSpecialWorkspaceID() == pNode->workspaceID) {
                PMONITOR = m;
                break;
            }
        }
    } else if (WS)
        PMONITOR = WS->m_monitor.lock();

    if (!PMONITOR || !WS) {
        Debug::log(ERR, "Orphaned Node {}!!", pNode);
        return;
    }

    // for gaps outer
    const auto WORKAREA      = workAreaOnWorkspace(WS);
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, WORKAREA.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, WORKAREA.x + WORKAREA.w);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, WORKAREA.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, WORKAREA.y + WORKAREA.h);

    const auto PWINDOW = pNode->pWindow.lock();
    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(PWINDOW->m_workspace);

    if (PWINDOW->isFullscreen() && !pNode->ignoreFullscreenChecks)
        return;

    PWINDOW->m_ruleApplicator->resetProps(Desktop::Rule::RULE_PROP_ALL, Desktop::Types::PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();

    static auto PANIMATE    = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");
    static auto PGAPSINDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    auto*       PGAPSIN     = sc<CCssGapData*>((PGAPSINDATA.ptr())->getData());

    auto        gapsIn = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);

    if (!validMapped(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid {}!!", pNode, PWINDOW);
        return;
    }

    PWINDOW->m_size     = pNode->size;
    PWINDOW->m_position = pNode->position;

    PWINDOW->updateWindowDecos();

    auto       calcPos  = PWINDOW->m_position;
    auto       calcSize = PWINDOW->m_size;

    const auto OFFSETTOPLEFT = Vector2D(sc<double>(DISPLAYLEFT ? 0 : gapsIn.m_left), sc<double>(DISPLAYTOP ? 0 : gapsIn.m_top));

    const auto OFFSETBOTTOMRIGHT = Vector2D(sc<double>(DISPLAYRIGHT ? 0 : gapsIn.m_right), sc<double>(DISPLAYBOTTOM ? 0 : gapsIn.m_bottom));

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    Vector2D    availableSpace = calcSize;

    static auto PCLAMP_TILED = CConfigValue<Hyprlang::INT>("misc:size_limits_tiled");

    if (*PCLAMP_TILED) {
        const auto borderSize       = PWINDOW->getRealBorderSize();
        Vector2D   monitorAvailable = WORKAREA.size() - Vector2D{2.0 * borderSize, 2.0 * borderSize};

        Vector2D   minSize = PWINDOW->m_ruleApplicator->minSize().valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}).clamp(Vector2D{0, 0}, monitorAvailable);
        Vector2D   maxSize = PWINDOW->isFullscreen() ? Vector2D{INFINITY, INFINITY} :
                                                       PWINDOW->m_ruleApplicator->maxSize().valueOr(Vector2D{INFINITY, INFINITY}).clamp(Vector2D{0, 0}, monitorAvailable);
        calcSize           = calcSize.clamp(minSize, maxSize);

        calcPos += (availableSpace - calcSize) / 2.0;

        calcPos.x = std::clamp(calcPos.x, WORKAREA.x + borderSize, WORKAREA.x + WORKAREA.w - calcSize.x - borderSize);
        calcPos.y = std::clamp(calcPos.y, WORKAREA.y + borderSize, WORKAREA.y + WORKAREA.h - calcSize.y - borderSize);
    }

    if (PWINDOW->onSpecialWorkspace() && !PWINDOW->isFullscreen()) {
        static auto PSCALEFACTOR = CConfigValue<Hyprlang::FLOAT>("master:special_scale_factor");

        CBox        wb = {calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f, calcSize * *PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realPosition = wb.pos();
        *PWINDOW->m_realSize     = wb.size();
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round(); // avoid rounding mess

        *PWINDOW->m_realPosition = wb.pos();
        *PWINDOW->m_realSize     = wb.size();
    }

    if (m_forceWarps && !*PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_realPosition->warp();
        PWINDOW->m_realSize->warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CHyprMasterLayout::isWindowTiled(PHLWINDOW pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprMasterLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, PHLWINDOW pWindow) {
    const auto PWINDOW = pWindow ? pWindow : Desktop::focusState()->window();

    if (!validMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        *PWINDOW->m_realSize = (PWINDOW->m_realSize->goal() + pixResize)
                                   .clamp(PWINDOW->m_ruleApplicator->minSize().valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}),
                                          PWINDOW->m_ruleApplicator->maxSize().valueOr(Vector2D{INFINITY, INFINITY}));
        PWINDOW->updateWindowDecos();
        return;
    }

    const auto   PMONITOR            = PWINDOW->m_monitor.lock();
    static auto  SLAVECOUNTFORCENTER = CConfigValue<Hyprlang::INT>("master:slave_count_for_center_master");
    static auto  PSMARTRESIZING      = CConfigValue<Hyprlang::INT>("master:smart_resizing");

    const auto   WORKAREA      = PMONITOR->logicalBoxMinusReserved();
    const bool   DISPLAYBOTTOM = STICKS(PWINDOW->m_position.y + PWINDOW->m_size.y, WORKAREA.y + WORKAREA.h);
    const bool   DISPLAYRIGHT  = STICKS(PWINDOW->m_position.x + PWINDOW->m_size.x, WORKAREA.x + WORKAREA.w);
    const bool   DISPLAYTOP    = STICKS(PWINDOW->m_position.y, WORKAREA.y);
    const bool   DISPLAYLEFT   = STICKS(PWINDOW->m_position.x, WORKAREA.x);

    const bool   LEFT = corner == CORNER_TOPLEFT || corner == CORNER_BOTTOMLEFT;
    const bool   TOP  = corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT;
    const bool   NONE = corner == CORNER_NONE;

    const auto   MASTERS      = getMastersOnWorkspace(PNODE->workspaceID);
    const auto   WINDOWS      = getNodesOnWorkspace(PNODE->workspaceID);
    const auto   STACKWINDOWS = WINDOWS - MASTERS;

    eOrientation orientation = getDynamicOrientation(PWINDOW->m_workspace);
    bool         centered    = orientation == ORIENTATION_CENTER && (STACKWINDOWS >= *SLAVECOUNTFORCENTER);
    double       delta       = 0;

    if (getNodesOnWorkspace(PWINDOW->workspaceID()) == 1 && !centered)
        return;

    m_forceWarps = true;

    switch (orientation) {
        case ORIENTATION_LEFT: delta = pixResize.x / PMONITOR->m_size.x; break;
        case ORIENTATION_RIGHT: delta = -pixResize.x / PMONITOR->m_size.x; break;
        case ORIENTATION_BOTTOM: delta = -pixResize.y / PMONITOR->m_size.y; break;
        case ORIENTATION_TOP: delta = pixResize.y / PMONITOR->m_size.y; break;
        case ORIENTATION_CENTER:
            delta = pixResize.x / PMONITOR->m_size.x;
            if (STACKWINDOWS >= *SLAVECOUNTFORCENTER) {
                if (!NONE || !PNODE->isMaster)
                    delta *= 2;
                if ((!PNODE->isMaster && DISPLAYLEFT) || (PNODE->isMaster && LEFT && *PSMARTRESIZING))
                    delta = -delta;
            }
            break;
        default: UNREACHABLE();
    }

    const auto workspaceIdForResizing = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();
    for (auto& n : m_masterNodesData) {
        if (n.isMaster && n.workspaceID == workspaceIdForResizing)
            n.percMaster = std::clamp(n.percMaster + delta, 0.05, 0.95);
    }

    // check the up/down resize
    const bool isStackVertical = orientation == ORIENTATION_LEFT || orientation == ORIENTATION_RIGHT || orientation == ORIENTATION_CENTER;

    const auto RESIZEDELTA = isStackVertical ? pixResize.y : pixResize.x;

    auto       nodesInSameColumn = PNODE->isMaster ? MASTERS : STACKWINDOWS;
    if (orientation == ORIENTATION_CENTER && !PNODE->isMaster)
        nodesInSameColumn = DISPLAYRIGHT ? (nodesInSameColumn + 1) / 2 : nodesInSameColumn / 2;

    const auto SIZE = isStackVertical ? WORKAREA.h / nodesInSameColumn : WORKAREA.w / nodesInSameColumn;

    if (RESIZEDELTA != 0 && nodesInSameColumn > 1) {
        if (!*PSMARTRESIZING) {
            PNODE->percSize = std::clamp(PNODE->percSize + RESIZEDELTA / SIZE, 0.05, 1.95);
        } else {
            const auto  NODEIT    = std::ranges::find(m_masterNodesData, *PNODE);
            const auto  REVNODEIT = std::ranges::find(m_masterNodesData | std::views::reverse, *PNODE);

            const float totalSize       = isStackVertical ? WORKAREA.h : WORKAREA.w;
            const float minSize         = totalSize / nodesInSameColumn * 0.2;
            const bool  resizePrevNodes = isStackVertical ? (TOP || DISPLAYBOTTOM) && !DISPLAYTOP : (LEFT || DISPLAYRIGHT) && !DISPLAYLEFT;

            int         nodesLeft = 0;
            float       sizeLeft  = 0;
            int         nodeCount = 0;
            // check the sizes of all the nodes to be resized for later calculation
            auto checkNodesLeft = [&sizeLeft, &nodesLeft, orientation, isStackVertical, &nodeCount, PNODE](auto it) {
                if (it.isMaster != PNODE->isMaster || it.workspaceID != PNODE->workspaceID)
                    return;
                nodeCount++;
                if (!it.isMaster && orientation == ORIENTATION_CENTER && nodeCount % 2 == 1)
                    return;
                sizeLeft += isStackVertical ? it.size.y : it.size.x;
                nodesLeft++;
            };
            float resizeDiff;
            if (resizePrevNodes) {
                std::for_each(std::next(REVNODEIT), m_masterNodesData.rend(), checkNodesLeft);
                resizeDiff = -RESIZEDELTA;
            } else {
                std::for_each(std::next(NODEIT), m_masterNodesData.end(), checkNodesLeft);
                resizeDiff = RESIZEDELTA;
            }

            const float nodeSize        = isStackVertical ? PNODE->size.y : PNODE->size.x;
            const float maxSizeIncrease = sizeLeft - nodesLeft * minSize;
            const float maxSizeDecrease = minSize - nodeSize;

            // leaves enough room for the other nodes
            resizeDiff = std::clamp(resizeDiff, maxSizeDecrease, maxSizeIncrease);
            PNODE->percSize += resizeDiff / SIZE;

            // resize the other nodes
            nodeCount            = 0;
            auto resizeNodesLeft = [maxSizeIncrease, resizeDiff, minSize, orientation, isStackVertical, SIZE, &nodeCount, nodesLeft, PNODE](auto& it) {
                if (it.isMaster != PNODE->isMaster || it.workspaceID != PNODE->workspaceID)
                    return;
                nodeCount++;
                // if center orientation, only resize when on the same side
                if (!it.isMaster && orientation == ORIENTATION_CENTER && nodeCount % 2 == 1)
                    return;
                const float size               = isStackVertical ? it.size.y : it.size.x;
                const float resizeDeltaForEach = maxSizeIncrease != 0 ? resizeDiff * (size - minSize) / maxSizeIncrease : resizeDiff / nodesLeft;
                it.percSize -= resizeDeltaForEach / SIZE;
            };
            if (resizePrevNodes) {
                std::for_each(std::next(REVNODEIT), m_masterNodesData.rend(), resizeNodesLeft);
            } else {
                std::for_each(std::next(NODEIT), m_masterNodesData.end(), resizeNodesLeft);
            }
        }
    }

    recalculateMonitor(PMONITOR->m_id);

    m_forceWarps = false;
}

void CHyprMasterLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) {
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

            pWindow->m_ruleApplicator->resetProps(Desktop::Rule::RULE_PROP_ALL, Desktop::Types::PRIORITY_LAYOUT);
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

            SMasterNodeData fakeNode;
            const auto      WORKAREA        = PMONITOR->logicalBoxMinusReserved();
            fakeNode.pWindow                = pWindow;
            fakeNode.position               = WORKAREA.pos();
            fakeNode.size                   = WORKAREA.size();
            fakeNode.workspaceID            = pWindow->workspaceID();
            pWindow->m_position             = fakeNode.position;
            pWindow->m_size                 = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->changeWindowZOrder(pWindow, true);
}

void CHyprMasterLayout::recalculateWindow(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    recalculateMonitor(pWindow->monitorID());
}

SWindowRenderLayoutHints CHyprMasterLayout::requestRenderHints(PHLWINDOW pWindow) {
    // window should be valid, insallah

    SWindowRenderLayoutHints hints;

    return hints; // master doesn't have any hints
}

void CHyprMasterLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
    if (!isDirection(dir))
        return;

    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(pWindow, dir[0]);

    if (!PWINDOW2)
        return;

    pWindow->setAnimationsToMove();

    if (pWindow->m_workspace != PWINDOW2->m_workspace) {
        // if different monitors, send to monitor
        onWindowRemovedTiling(pWindow);
        pWindow->moveToWorkspace(PWINDOW2->m_workspace);
        pWindow->m_monitor = PWINDOW2->m_monitor;
        if (!silent) {
            const auto pMonitor = pWindow->m_monitor.lock();
            Desktop::focusState()->rawMonitorFocus(pMonitor);
        }
        onWindowCreatedTiling(pWindow);
    } else {
        // if same monitor, switch windows
        switchWindows(pWindow, PWINDOW2);
        if (silent)
            Desktop::focusState()->fullWindowFocus(PWINDOW2);
    }

    pWindow->updateGroupOutputs();
    if (!pWindow->m_groupData.pNextWindow.expired()) {
        PHLWINDOW next = pWindow->m_groupData.pNextWindow.lock();
        while (next != pWindow) {
            next->updateToplevel();
            next = next->m_groupData.pNextWindow.lock();
        }
    }
}

void CHyprMasterLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_monitor, pWindow->m_monitor);
        std::swap(pWindow2->m_workspace, pWindow->m_workspace);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    pWindow->setAnimationsToMove();
    pWindow2->setAnimationsToMove();

    recalculateMonitor(pWindow->monitorID());
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->monitorID());

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);
}

void CHyprMasterLayout::alterSplitRatio(PHLWINDOW pWindow, float ratio, bool exact) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PMASTER = getMasterNodeOnWorkspace(pWindow->workspaceID());

    float      newRatio = exact ? ratio : PMASTER->percMaster + ratio;
    PMASTER->percMaster = std::clamp(newRatio, 0.05f, 0.95f);

    recalculateMonitor(pWindow->monitorID());
}

PHLWINDOW CHyprMasterLayout::getNextWindow(PHLWINDOW pWindow, bool next, bool loop) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    const auto PNODE = getNodeFromWindow(pWindow);

    auto       nodes = m_masterNodesData;
    if (!next)
        std::ranges::reverse(nodes);

    const auto NODEIT = std::ranges::find(nodes, *PNODE);

    const bool ISMASTER = PNODE->isMaster;

    auto CANDIDATE = std::find_if(NODEIT, nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER == other.isMaster && other.workspaceID == PNODE->workspaceID; });
    if (CANDIDATE == nodes.end())
        CANDIDATE = std::ranges::find_if(nodes, [&](const auto& other) { return other != *PNODE && ISMASTER != other.isMaster && other.workspaceID == PNODE->workspaceID; });

    if (CANDIDATE != nodes.end() && !loop) {
        if (CANDIDATE->isMaster && next)
            return nullptr;
        if (!CANDIDATE->isMaster && ISMASTER && !next)
            return nullptr;
    }

    return CANDIDATE == nodes.end() ? nullptr : CANDIDATE->pWindow.lock();
}

std::any CHyprMasterLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    auto switchToWindow = [&](PHLWINDOW PWINDOWTOCHANGETO) {
        if (!validMapped(PWINDOWTOCHANGETO))
            return;

        Desktop::focusState()->fullWindowFocus(PWINDOWTOCHANGETO);
        g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->middle());

        g_pInputManager->m_forcedFocus = PWINDOWTOCHANGETO;
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_forcedFocus.reset();
    };

    CVarList vars(message, 0, ' ');

    if (vars.size() < 1 || vars[0].empty()) {
        Debug::log(ERR, "layoutmsg called without params");
        return 0;
    }

    auto command = vars[0];

    // swapwithmaster <master | child | auto> <ignoremaster>
    // first message argument can have the following values:
    // * master - keep the focus at the new master
    // * child - keep the focus at the new child
    // * auto (default) - swap the focus (keep the focus of the previously selected window)
    // * ignoremaster - ignore if master is focused
    if (command == "swapwithmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        if (!isWindowTiled(PWINDOW))
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        const auto NEWCHILD = PMASTER->pWindow.lock();

        const bool IGNORE_IF_MASTER = vars.size() >= 2 && std::ranges::any_of(vars, [](const auto& e) { return e == "ignoremaster"; });

        if (PMASTER->pWindow.lock() != PWINDOW) {
            const auto& NEWMASTER       = PWINDOW;
            const bool  newFocusToChild = vars.size() >= 2 && vars[1] == "child";
            switchWindows(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
        } else if (!IGNORE_IF_MASTER) {
            for (auto const& n : m_masterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    const auto NEWMASTER = n.pWindow.lock();
                    switchWindows(NEWMASTER, NEWCHILD);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    const auto NEWFOCUS         = newFocusToMaster ? NEWMASTER : NEWCHILD;
                    switchToWindow(NEWFOCUS);
                    break;
                }
            }
        }

        return 0;
    }
    // focusmaster <master | previous | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master, even if it was focused before
    // * previous - focus window which was previously switched from using `focusmaster previous` command, otherwise fallback to `auto`
    // * auto (default) - swap the focus with the first child, if the current focus was master, otherwise focus master
    else if (command == "focusmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        const auto& ARG = vars[1]; // returns empty string if out of bounds

        if (PMASTER->pWindow.lock() != PWINDOW) {
            switchToWindow(PMASTER->pWindow.lock());
            // save previously focused window (only for `previous` mode)
            if (ARG == "previous")
                getMasterWorkspaceData(PWINDOW->workspaceID())->focusMasterPrev = PWINDOW;
            return 0;
        }

        const auto focusAuto = [&]() {
            // focus first non-master window
            for (auto const& n : m_masterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchToWindow(n.pWindow.lock());
                    break;
                }
            }
        };

        if (ARG == "master")
            return 0;
        // switch to previously saved window
        else if (ARG == "previous") {
            const auto PREVWINDOW = getMasterWorkspaceData(PWINDOW->workspaceID())->focusMasterPrev.lock();
            const bool VALID      = validMapped(PREVWINDOW) && PWINDOW->workspaceID() == PREVWINDOW->workspaceID() && PWINDOW != PREVWINDOW;
            VALID ? switchToWindow(PREVWINDOW) : focusAuto();
        } else
            focusAuto();
    } else if (command == "cyclenext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool NOLOOP      = vars.size() >= 2 && vars[1] == "noloop";
        const auto PNEXTWINDOW = getNextWindow(PWINDOW, true, !NOLOOP);
        switchToWindow(PNEXTWINDOW);
    } else if (command == "cycleprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool NOLOOP      = vars.size() >= 2 && vars[1] == "noloop";
        const auto PPREVWINDOW = getNextWindow(PWINDOW, false, !NOLOOP);
        switchToWindow(PPREVWINDOW);
    } else if (command == "swapnext") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating) {
            g_pKeybindManager->m_dispatchers["swapnext"]("");
            return 0;
        }

        const bool NOLOOP            = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, true, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            switchToWindow(header.pWindow);
        }
    } else if (command == "swapprev") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating) {
            g_pKeybindManager->m_dispatchers["swapnext"]("prev");
            return 0;
        }

        const bool NOLOOP            = vars.size() >= 2 && vars[1] == "noloop";
        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, false, !NOLOOP);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenClient(header.pWindow, FSMODE_NONE);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            switchToWindow(header.pWindow);
        }
    } else if (command == "addmaster") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating)
            return 0;

        const auto  PNODE = getNodeFromWindow(header.pWindow);

        const auto  WINDOWS    = getNodesOnWorkspace(header.pWindow->workspaceID());
        const auto  MASTERS    = getMastersOnWorkspace(header.pWindow->workspaceID());
        static auto SMALLSPLIT = CConfigValue<Hyprlang::INT>("master:allow_small_split");

        if (MASTERS + 2 > WINDOWS && *SMALLSPLIT == 0)
            return 0;

        g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_masterNodesData) {
                if (n.workspaceID == header.pWindow->workspaceID() && !n.isMaster) {
                    n.isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        recalculateMonitor(header.pWindow->monitorID());

    } else if (command == "removemaster") {

        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_isFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->workspaceID());
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->workspaceID());

        if (WINDOWS < 2 || MASTERS < 2)
            return 0;

        g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto& nd : m_masterNodesData | std::views::reverse) {
                if (nd.workspaceID == header.pWindow->workspaceID() && nd.isMaster) {
                    nd.isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }

        recalculateMonitor(header.pWindow->monitorID());
    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());

        if (command == "orientationleft")
            PWORKSPACEDATA->orientation = ORIENTATION_LEFT;
        else if (command == "orientationright")
            PWORKSPACEDATA->orientation = ORIENTATION_RIGHT;
        else if (command == "orientationtop")
            PWORKSPACEDATA->orientation = ORIENTATION_TOP;
        else if (command == "orientationbottom")
            PWORKSPACEDATA->orientation = ORIENTATION_BOTTOM;
        else if (command == "orientationcenter")
            PWORKSPACEDATA->orientation = ORIENTATION_CENTER;

        recalculateMonitor(header.pWindow->monitorID());

    } else if (command == "orientationnext") {
        runOrientationCycle(header, nullptr, 1);
    } else if (command == "orientationprev") {
        runOrientationCycle(header, nullptr, -1);
    } else if (command == "orientationcycle") {
        runOrientationCycle(header, &vars, 1);
    } else if (command == "mfact") {
        g_pKeybindManager->m_dispatchers["splitratio"](vars[1] + " " + vars[2]);
    } else if (command == "rollnext") {
        const auto PWINDOW = header.pWindow;
        const auto PNODE   = getNodeFromWindow(PWINDOW);

        if (!PNODE)
            return 0;

        const auto OLDMASTER = PNODE->isMaster ? PNODE : getMasterNodeOnWorkspace(PNODE->workspaceID);
        if (!OLDMASTER)
            return 0;

        const auto OLDMASTERIT = std::ranges::find(m_masterNodesData, *OLDMASTER);

        for (auto& nd : m_masterNodesData) {
            if (nd.workspaceID == PNODE->workspaceID && !nd.isMaster) {
                nd.isMaster            = true;
                const auto NEWMASTERIT = std::ranges::find(m_masterNodesData, nd);
                m_masterNodesData.splice(OLDMASTERIT, m_masterNodesData, NEWMASTERIT);
                switchToWindow(nd.pWindow.lock());
                OLDMASTER->isMaster = false;
                m_masterNodesData.splice(m_masterNodesData.end(), m_masterNodesData, OLDMASTERIT);
                break;
            }
        }

        recalculateMonitor(PWINDOW->monitorID());
    } else if (command == "rollprev") {
        const auto PWINDOW = header.pWindow;
        const auto PNODE   = getNodeFromWindow(PWINDOW);

        if (!PNODE)
            return 0;

        const auto OLDMASTER = PNODE->isMaster ? PNODE : getMasterNodeOnWorkspace(PNODE->workspaceID);
        if (!OLDMASTER)
            return 0;

        const auto OLDMASTERIT = std::ranges::find(m_masterNodesData, *OLDMASTER);

        for (auto& nd : m_masterNodesData | std::views::reverse) {
            if (nd.workspaceID == PNODE->workspaceID && !nd.isMaster) {
                nd.isMaster            = true;
                const auto NEWMASTERIT = std::ranges::find(m_masterNodesData, nd);
                m_masterNodesData.splice(OLDMASTERIT, m_masterNodesData, NEWMASTERIT);
                switchToWindow(nd.pWindow.lock());
                OLDMASTER->isMaster = false;
                m_masterNodesData.splice(m_masterNodesData.begin(), m_masterNodesData, OLDMASTERIT);
                break;
            }
        }

        recalculateMonitor(PWINDOW->monitorID());
    }

    return 0;
}

// If vars is null, we use the default list
void CHyprMasterLayout::runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int direction) {
    std::vector<eOrientation> cycle;
    if (vars != nullptr)
        buildOrientationCycleVectorFromVars(cycle, *vars);

    if (cycle.empty())
        buildOrientationCycleVectorFromEOperation(cycle);

    const auto PWINDOW = header.pWindow;

    if (!PWINDOW)
        return;

    g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());

    int        nextOrPrev = 0;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (PWORKSPACEDATA->orientation == cycle[i]) {
            nextOrPrev = i + direction;
            break;
        }
    }

    if (nextOrPrev >= sc<int>(cycle.size()))
        nextOrPrev = nextOrPrev % sc<int>(cycle.size());
    else if (nextOrPrev < 0)
        nextOrPrev = cycle.size() + (nextOrPrev % sc<int>(cycle.size()));

    PWORKSPACEDATA->orientation = cycle.at(nextOrPrev);
    recalculateMonitor(header.pWindow->monitorID());
}

void CHyprMasterLayout::buildOrientationCycleVectorFromEOperation(std::vector<eOrientation>& cycle) {
    for (int i = 0; i <= ORIENTATION_CENTER; ++i) {
        cycle.push_back(sc<eOrientation>(i));
    }
}

void CHyprMasterLayout::buildOrientationCycleVectorFromVars(std::vector<eOrientation>& cycle, CVarList& vars) {
    for (size_t i = 1; i < vars.size(); ++i) {
        if (vars[i] == "top") {
            cycle.push_back(ORIENTATION_TOP);
        } else if (vars[i] == "right") {
            cycle.push_back(ORIENTATION_RIGHT);
        } else if (vars[i] == "bottom") {
            cycle.push_back(ORIENTATION_BOTTOM);
        } else if (vars[i] == "left") {
            cycle.push_back(ORIENTATION_LEFT);
        } else if (vars[i] == "center") {
            cycle.push_back(ORIENTATION_CENTER);
        }
    }
}

eOrientation CHyprMasterLayout::getDynamicOrientation(PHLWORKSPACE pWorkspace) {
    const auto  WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(pWorkspace);
    std::string orientationString;
    if (WORKSPACERULE.layoutopts.contains("orientation"))
        orientationString = WORKSPACERULE.layoutopts.at("orientation");

    eOrientation orientation = getMasterWorkspaceData(pWorkspace->m_id)->orientation;
    // override if workspace rule is set
    if (!orientationString.empty()) {
        if (orientationString == "top")
            orientation = ORIENTATION_TOP;
        else if (orientationString == "right")
            orientation = ORIENTATION_RIGHT;
        else if (orientationString == "bottom")
            orientation = ORIENTATION_BOTTOM;
        else if (orientationString == "center")
            orientation = ORIENTATION_CENTER;
        else
            orientation = ORIENTATION_LEFT;
    }

    return orientation;
}

void CHyprMasterLayout::replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE);
}

Vector2D CHyprMasterLayout::predictSizeForNewWindowTiled() {
    static auto PNEWSTATUS = CConfigValue<std::string>("master:new_status");

    if (!Desktop::focusState()->monitor())
        return {};

    const int NODES = getNodesOnWorkspace(Desktop::focusState()->monitor()->m_activeWorkspace->m_id);

    if (NODES <= 0)
        return Desktop::focusState()->monitor()->m_size;

    const auto MASTER = getMasterNodeOnWorkspace(Desktop::focusState()->monitor()->m_activeWorkspace->m_id);
    if (!MASTER) // wtf
        return {};

    if (*PNEWSTATUS == "master") {
        return MASTER->size;
    } else {
        const auto SLAVES = NODES - getMastersOnWorkspace(Desktop::focusState()->monitor()->m_activeWorkspace->m_id);

        // TODO: make this better
        return {Desktop::focusState()->monitor()->m_size.x - MASTER->size.x, Desktop::focusState()->monitor()->m_size.y / (SLAVES + 1)};
    }

    return {};
}

void CHyprMasterLayout::onEnable() {
    for (auto const& w : g_pCompositor->m_windows) {
        if (w->m_isFloating || !w->m_isMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w);
    }
}

void CHyprMasterLayout::onDisable() {
    m_masterNodesData.clear();
}
