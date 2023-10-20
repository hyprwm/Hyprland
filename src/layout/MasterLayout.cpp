#include "MasterLayout.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include <ranges>

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

int CHyprMasterLayout::getMastersOnWorkspace(const int& ws) {
    int no = 0;
    for (auto& n : m_lMasterNodesData) {
        if (n.workspaceID == ws && n.isMaster)
            no++;
    }

    return no;
}

SMasterWorkspaceData* CHyprMasterLayout::getMasterWorkspaceData(const int& ws) {
    for (auto& n : m_lMasterWorkspacesData) {
        if (n.workspaceID == ws)
            return &n;
    }

    //create on the fly if it doesn't exist yet
    const auto PWORKSPACEDATA   = &m_lMasterWorkspacesData.emplace_back();
    PWORKSPACEDATA->workspaceID = ws;
    const auto orientation      = &g_pConfigManager->getConfigValuePtr("master:orientation")->strValue;
    if (*orientation == "top") {
        PWORKSPACEDATA->orientation = ORIENTATION_TOP;
    } else if (*orientation == "right") {
        PWORKSPACEDATA->orientation = ORIENTATION_RIGHT;
    } else if (*orientation == "bottom") {
        PWORKSPACEDATA->orientation = ORIENTATION_BOTTOM;
    } else if (*orientation == "left") {
        PWORKSPACEDATA->orientation = ORIENTATION_LEFT;
    } else {
        PWORKSPACEDATA->orientation = ORIENTATION_CENTER;
    }
    return PWORKSPACEDATA;
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

void CHyprMasterLayout::onWindowCreatedTiling(CWindow* pWindow, eDirection direction) {
    if (pWindow->m_bIsFloating)
        return;

    static auto* const PNEWTOP = &g_pConfigManager->getConfigValuePtr("master:new_on_top")->intValue;

    const auto         PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    const auto         PNODE = *PNEWTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();

    PNODE->workspaceID = pWindow->m_iWorkspaceID;
    PNODE->pWindow     = pWindow;

    static auto* const PNEWISMASTER = &g_pConfigManager->getConfigValuePtr("master:new_is_master")->intValue;

    const auto         WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    static auto* const PMFACT             = &g_pConfigManager->getConfigValuePtr("master:mfact")->floatValue;
    float              lastSplitPercent   = *PMFACT;

    auto               OPENINGON = isWindowTiled(g_pCompositor->m_pLastWindow) && g_pCompositor->m_pLastWindow->m_iWorkspaceID == pWindow->m_iWorkspaceID ?
                      getNodeFromWindow(g_pCompositor->m_pLastWindow) :
                      getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);

    const auto         MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();

    // if it's a group, add the window
    if (OPENINGON && OPENINGON != PNODE && OPENINGON->pWindow->m_sGroupData.pNextWindow                   // target is group
        && !g_pKeybindManager->m_bGroupsLocked                                                            // global group lock disengaged
        && ((pWindow->m_eGroupRules & GROUP_INVADE && pWindow->m_bFirstMap)                               // window ignore local group locks, or
            || (!OPENINGON->pWindow->getGroupHead()->m_sGroupData.locked                                  //    target unlocked
                && !(pWindow->m_sGroupData.pNextWindow && pWindow->getGroupHead()->m_sGroupData.locked))) //    source unlocked or isn't group
        && !pWindow->m_sGroupData.deny                                                                    // source is not denied entry
        && !(pWindow->m_eGroupRules & GROUP_BARRED)                                                       // group rule doesn't prevent adding window
    ) {

        if (!pWindow->m_sGroupData.pNextWindow)
            pWindow->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(pWindow));

        m_lMasterNodesData.remove(*PNODE);

        const wlr_box box = OPENINGON->pWindow->getDecorationByType(DECORATION_GROUPBAR)->getWindowDecorationRegion().getExtents();
        if (wlr_box_contains_point(&box, MOUSECOORDS.x, MOUSECOORDS.y)) { // TODO: Deny when not using mouse
            const int SIZE               = OPENINGON->pWindow->getGroupSize();
            const int INDEX              = (int)((MOUSECOORDS.x - box.x) * 2 * SIZE / box.width + 1) / 2 - 1;
            CWindow*  pWindowInsertAfter = OPENINGON->pWindow->getGroupWindowByIndex(INDEX);
            pWindowInsertAfter->insertWindowToGroup(pWindow);
            if (INDEX == -1)
                std::swap(pWindow->m_sGroupData.pNextWindow->m_sGroupData.head, pWindow->m_sGroupData.head);
        } else {
            static const auto* USECURRPOS = &g_pConfigManager->getConfigValuePtr("group:insert_after_current")->intValue;
            (*USECURRPOS ? OPENINGON->pWindow : OPENINGON->pWindow->getGroupTail())->insertWindowToGroup(pWindow);
        }

        OPENINGON->pWindow->setGroupCurrent(pWindow);
        pWindow->applyGroupRules();
        pWindow->updateWindowDecos();
        recalculateWindow(pWindow);

        return;
    }

    pWindow->applyGroupRules();

    static auto* const PDROPATCURSOR  = &g_pConfigManager->getConfigValuePtr("master:drop_at_cursor")->intValue;
    const auto         PWORKSPACEDATA = getMasterWorkspaceData(pWindow->m_iWorkspaceID);
    eOrientation       orientation    = PWORKSPACEDATA->orientation;
    const auto         NODEIT         = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), *PNODE);
    if (*PDROPATCURSOR && g_pInputManager->dragMode == MBIND_MOVE) {
        // if dragging window to move, drop it at the cursor position instead of bottom/top of stack
        for (auto it = m_lMasterNodesData.begin(); it != m_lMasterNodesData.end(); ++it) {
            if (it->workspaceID != pWindow->m_iWorkspaceID)
                continue;
            const wlr_box box = it->pWindow->getWindowIdealBoundingBoxIgnoreReserved();
            if (wlr_box_contains_point(&box, MOUSECOORDS.x, MOUSECOORDS.y)) { // TODO: Deny when not using mouse
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
                m_lMasterNodesData.splice(it, m_lMasterNodesData, NODEIT);
                break;
            }
        }
    }

    if (*PNEWISMASTER || WINDOWSONWORKSPACE == 1 || (!pWindow->m_bFirstMap && OPENINGON->isMaster)) {
        for (auto& nd : m_lMasterNodesData) {
            if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                nd.isMaster      = false;
                lastSplitPercent = nd.percMaster;
                break;
            }
        }

        PNODE->isMaster   = true;
        PNODE->percMaster = lastSplitPercent;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow); MAXSIZE.x < PMONITOR->vecSize.x * lastSplitPercent || MAXSIZE.y < PMONITOR->vecSize.y) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    } else {
        PNODE->isMaster   = false;
        PNODE->percMaster = lastSplitPercent;

        // first, check if it isn't too big.
        if (const auto MAXSIZE = g_pXWaylandManager->getMaxSizeForWindow(pWindow);
            MAXSIZE.x < PMONITOR->vecSize.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->vecSize.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    // recalc
    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprMasterLayout::onWindowRemovedTiling(CWindow* pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto         WORKSPACEID = PNODE->workspaceID;
    const auto         MASTERSLEFT = getMastersOnWorkspace(WORKSPACEID);
    static const auto* SMALLSPLIT  = &g_pConfigManager->getConfigValuePtr("master:allow_small_split")->intValue;

    pWindow->updateSpecialRenderData();

    if (pWindow->m_bIsFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);

    if (PNODE->isMaster && (MASTERSLEFT <= 1 || *SMALLSPLIT == 1)) {
        // find a new master from top of the list
        for (auto& nd : m_lMasterNodesData) {
            if (!nd.isMaster && nd.workspaceID == WORKSPACEID) {
                nd.isMaster   = true;
                nd.percMaster = PNODE->percMaster;
                break;
            }
        }
    }

    m_lMasterNodesData.remove(*PNODE);

    if (getMastersOnWorkspace(WORKSPACEID) == getNodesOnWorkspace(WORKSPACEID) && MASTERSLEFT > 1) {
        for (auto& nd : m_lMasterNodesData | std::views::reverse) {
            if (nd.workspaceID == WORKSPACEID) {
                nd.isMaster = false;
                break;
            }
        }
    }
    // BUGFIX: correct bug where closing one master in a stack of 2 would leave
    // the screen half bare, and make it difficult to select remaining window
    if (getNodesOnWorkspace(WORKSPACEID) == 1) {
        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID == WORKSPACEID && nd.isMaster == false) {
                nd.isMaster = true;
                break;
            }
        }
    }
    recalculateMonitor(pWindow->m_iMonitorID);
}

void CHyprMasterLayout::recalculateMonitor(const int& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

    if (!PMONITOR)
        return;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PMONITOR->activeWorkspace);

    if (!PWORKSPACE)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->specialWorkspaceID) {
        calculateWorkspace(PMONITOR->specialWorkspaceID);
    }

    if (PWORKSPACE->m_bHasFullscreenWindow) {
        // massive hack from the fullscreen func
        const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(PWORKSPACE->m_iID);

        if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) {
            PFULLWINDOW->m_vRealPosition = PMONITOR->vecPosition;
            PFULLWINDOW->m_vRealSize     = PMONITOR->vecSize;
        } else if (PWORKSPACE->m_efFullscreenMode == FULLSCREEN_MAXIMIZED) {
            SMasterNodeData fakeNode;
            fakeNode.pWindow         = PFULLWINDOW;
            fakeNode.position        = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size            = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID     = PWORKSPACE->m_iID;
            PFULLWINDOW->m_vPosition = fakeNode.position;
            PFULLWINDOW->m_vSize     = fakeNode.size;

            applyNodeDataToWindow(&fakeNode);
        }

        return;
    }

    // calc the WS
    calculateWorkspace(PWORKSPACE->m_iID);
}

void CHyprMasterLayout::calculateWorkspace(const int& ws) {
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(ws);

    if (!PWORKSPACE)
        return;

    const auto PWORKSPACEDATA = getMasterWorkspaceData(ws);
    const auto PMONITOR       = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);
    const auto PMASTERNODE    = getMasterNodeOnWorkspace(PWORKSPACE->m_iID);

    if (!PMASTERNODE)
        return;

    eOrientation       orientation        = PWORKSPACEDATA->orientation;
    bool               centerMasterWindow = false;
    static auto* const ALWAYSCENTER       = &g_pConfigManager->getConfigValuePtr("master:always_center_master")->intValue;
    static auto* const PSMARTRESIZING     = &g_pConfigManager->getConfigValuePtr("master:smart_resizing")->intValue;

    const auto         MASTERS      = getMastersOnWorkspace(PWORKSPACE->m_iID);
    const auto         WINDOWS      = getNodesOnWorkspace(PWORKSPACE->m_iID);
    const auto         STACKWINDOWS = WINDOWS - MASTERS;
    const auto         WSSIZE       = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
    const auto         WSPOS        = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;

    if (orientation == ORIENTATION_CENTER) {
        if (STACKWINDOWS >= 2 || (*ALWAYSCENTER == 1)) {
            centerMasterWindow = true;
        } else {
            orientation = ORIENTATION_LEFT;
        }
    }

    const float totalSize             = (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) ? WSSIZE.x : WSSIZE.y;
    const float masterAverageSize     = totalSize / MASTERS;
    const float slaveAverageSize      = totalSize / STACKWINDOWS;
    float       masterAccumulatedSize = 0;
    float       slaveAccumulatedSize  = 0;

    if (*PSMARTRESIZING) {
        // check the total width and height so that later
        // if larger/smaller than screen size them down/up
        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID == PWORKSPACE->m_iID) {
                if (nd.isMaster)
                    masterAccumulatedSize += totalSize / MASTERS * nd.percSize;
                else
                    slaveAccumulatedSize += totalSize / STACKWINDOWS * nd.percSize;
            }
        }
    }

    // compute placement of master window(s)
    if (WINDOWS == 1 && !centerMasterWindow) {
        PMASTERNODE->size     = WSSIZE;
        PMASTERNODE->position = WSPOS;

        applyNodeDataToWindow(PMASTERNODE);
        return;
    } else if (orientation == ORIENTATION_TOP || orientation == ORIENTATION_BOTTOM) {
        const float HEIGHT      = WSSIZE.y * PMASTERNODE->percMaster;
        float       widthLeft   = WSSIZE.x;
        int         mastersLeft = MASTERS;
        float       nextX       = 0;
        float       nextY       = 0;

        if (orientation == ORIENTATION_BOTTOM)
            nextY = WSSIZE.y - HEIGHT;

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != PWORKSPACE->m_iID || !nd.isMaster)
                continue;

            float WIDTH = mastersLeft > 1 ? widthLeft / mastersLeft * nd.percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && mastersLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WSSIZE.x / masterAccumulatedSize;
                WIDTH = masterAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            mastersLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else { // orientation left, right or center
        float WIDTH       = WSSIZE.x;
        float heightLeft  = WSSIZE.y;
        int   mastersLeft = MASTERS;
        float nextX       = 0;
        float nextY       = 0;

        if (STACKWINDOWS > 0 || centerMasterWindow)
            WIDTH *= PMASTERNODE->percMaster;

        if (orientation == ORIENTATION_RIGHT) {
            nextX = WSSIZE.x - WIDTH;
        } else if (centerMasterWindow) {
            nextX = (WSSIZE.x - WIDTH) / 2;
        }

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != PWORKSPACE->m_iID || !nd.isMaster)
                continue;

            float HEIGHT = mastersLeft > 1 ? heightLeft / mastersLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && mastersLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WSSIZE.y / masterAccumulatedSize;
                HEIGHT = masterAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
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
        const float HEIGHT    = WSSIZE.y - PMASTERNODE->size.y;
        float       widthLeft = WSSIZE.x;
        float       nextX     = 0;
        float       nextY     = 0;

        if (orientation == ORIENTATION_TOP)
            nextY = PMASTERNODE->size.y;

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != PWORKSPACE->m_iID || nd.isMaster)
                continue;

            float WIDTH = slavesLeft > 1 ? widthLeft / slavesLeft * nd.percSize : widthLeft;
            if (WIDTH > widthLeft * 0.9f && slavesLeft > 1)
                WIDTH = widthLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WSSIZE.x / slaveAccumulatedSize;
                WIDTH = slaveAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            slavesLeft--;
            widthLeft -= WIDTH;
            nextX += WIDTH;
        }
    } else if (orientation == ORIENTATION_LEFT || orientation == ORIENTATION_RIGHT) {
        const float WIDTH      = WSSIZE.x - PMASTERNODE->size.x;
        float       heightLeft = WSSIZE.y;
        float       nextY      = 0;
        float       nextX      = 0;

        if (orientation == ORIENTATION_LEFT)
            nextX = PMASTERNODE->size.x;

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != PWORKSPACE->m_iID || nd.isMaster)
                continue;

            float HEIGHT = slavesLeft > 1 ? heightLeft / slavesLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && slavesLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WSSIZE.y / slaveAccumulatedSize;
                HEIGHT = slaveAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
            applyNodeDataToWindow(&nd);

            slavesLeft--;
            heightLeft -= HEIGHT;
            nextY += HEIGHT;
        }
    } else { // slaves for centered master window(s)
        const float WIDTH       = (WSSIZE.x - PMASTERNODE->size.x) / 2.0;
        float       heightLeft  = 0;
        float       heightLeftL = WSSIZE.y;
        float       heightLeftR = WSSIZE.y;
        float       nextX       = 0;
        float       nextY       = 0;
        float       nextYL      = 0;
        float       nextYR      = 0;
        bool        onRight     = true;

        int         slavesLeftR = 1 + (slavesLeft - 1) / 2;
        int         slavesLeftL = slavesLeft - slavesLeftR;

        const float slaveAverageHeightL     = WSSIZE.y / slavesLeftL;
        const float slaveAverageHeightR     = WSSIZE.y / slavesLeftR;
        float       slaveAccumulatedHeightL = 0;
        float       slaveAccumulatedHeightR = 0;
        if (*PSMARTRESIZING) {
            for (auto& nd : m_lMasterNodesData) {
                if (nd.workspaceID != PWORKSPACE->m_iID || nd.isMaster)
                    continue;

                if (onRight) {
                    slaveAccumulatedHeightR += slaveAverageHeightR * nd.percSize;
                } else {
                    slaveAccumulatedHeightL += slaveAverageHeightL * nd.percSize;
                }
                onRight = !onRight;
            }
            onRight = true;
        }

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != PWORKSPACE->m_iID || nd.isMaster)
                continue;

            if (onRight) {
                nextX      = WIDTH + PMASTERNODE->size.x;
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
                    nd.percSize *= WSSIZE.y / slaveAccumulatedHeightR;
                    HEIGHT = slaveAverageHeightR * nd.percSize;
                } else {
                    nd.percSize *= WSSIZE.y / slaveAccumulatedHeightL;
                    HEIGHT = slaveAverageHeightL * nd.percSize;
                }
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = WSPOS + Vector2D(nextX, nextY);
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
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PWINDOW = pNode->pWindow;
    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID));

    PWINDOW->updateSpecialRenderData();

    static auto* const PGAPSIN         = &g_pConfigManager->getConfigValuePtr("general:gaps_in")->intValue;
    static auto* const PGAPSOUT        = &g_pConfigManager->getConfigValuePtr("general:gaps_out")->intValue;
    static auto* const PNOGAPSWHENONLY = &g_pConfigManager->getConfigValuePtr("master:no_gaps_when_only")->intValue;
    static auto* const PANIMATE        = &g_pConfigManager->getConfigValuePtr("misc:animate_manual_resizes")->intValue;

    auto               gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto               gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);

    if (!g_pCompositor->windowValidMapped(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid {}!!", pNode, PWINDOW);
        return;
    }

    PWINDOW->m_vSize     = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    if (*PNOGAPSWHENONLY && !g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID) &&
        (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) == 1 ||
         (PWINDOW->m_bIsFullscreen && g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID)->m_efFullscreenMode == FULLSCREEN_MAXIMIZED))) {

        PWINDOW->m_sSpecialRenderData.border   = WORKSPACERULE.border.value_or(*PNOGAPSWHENONLY == 2);
        PWINDOW->m_sSpecialRenderData.decorate = WORKSPACERULE.decorate.value_or(true);
        PWINDOW->m_sSpecialRenderData.rounding = false;
        PWINDOW->m_sSpecialRenderData.shadow   = false;

        const auto RESERVED = PWINDOW->getFullWindowReservedArea();

        const int  BORDERSIZE = PWINDOW->getRealBorderSize();

        PWINDOW->m_vRealPosition = PWINDOW->m_vPosition + Vector2D(BORDERSIZE, BORDERSIZE) + RESERVED.topLeft;
        PWINDOW->m_vRealSize     = PWINDOW->m_vSize - Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE) - (RESERVED.topLeft + RESERVED.bottomRight);

        PWINDOW->updateWindowDecos();

        return;
    }

    const int  BORDERSIZE = PWINDOW->getRealBorderSize();

    auto       calcPos  = PWINDOW->m_vPosition + Vector2D(BORDERSIZE, BORDERSIZE);
    auto       calcSize = PWINDOW->m_vSize - Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE);

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? gapsOut : gapsIn, DISPLAYTOP ? gapsOut : gapsIn);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? gapsOut : gapsIn, DISPLAYBOTTOM ? gapsOut : gapsIn);

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(PWINDOW->m_iWorkspaceID)) {
        static auto* const PSCALEFACTOR = &g_pConfigManager->getConfigValuePtr("master:special_scale_factor")->floatValue;

        PWINDOW->m_vRealPosition = calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f;
        PWINDOW->m_vRealSize     = calcSize * *PSCALEFACTOR;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize * *PSCALEFACTOR);
    } else {
        PWINDOW->m_vRealSize     = calcSize;
        PWINDOW->m_vRealPosition = calcPos;

        g_pXWaylandManager->setWindowSize(PWINDOW, calcSize);
    }

    if (m_bForceWarps && !*PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CHyprMasterLayout::isWindowTiled(CWindow* pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprMasterLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, CWindow* pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow;

    if (!g_pCompositor->windowValidMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        PWINDOW->m_vRealSize = Vector2D(std::max((PWINDOW->m_vRealSize.goalv() + pixResize).x, 20.0), std::max((PWINDOW->m_vRealSize.goalv() + pixResize).y, 20.0));
        PWINDOW->updateWindowDecos();
        return;
    }

    const auto         PMONITOR       = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
    const auto         PWORKSPACEDATA = getMasterWorkspaceData(PMONITOR->activeWorkspace);
    static auto* const ALWAYSCENTER   = &g_pConfigManager->getConfigValuePtr("master:always_center_master")->intValue;
    static auto* const PSMARTRESIZING = &g_pConfigManager->getConfigValuePtr("master:smart_resizing")->intValue;

    eOrientation       orientation = PWORKSPACEDATA->orientation;
    bool               centered    = orientation == ORIENTATION_CENTER && (*ALWAYSCENTER == 1);
    double             delta       = 0;

    const bool         DISPLAYBOTTOM = STICKS(PWINDOW->m_vPosition.y + PWINDOW->m_vSize.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);
    const bool         DISPLAYRIGHT  = STICKS(PWINDOW->m_vPosition.x + PWINDOW->m_vSize.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool         DISPLAYTOP    = STICKS(PWINDOW->m_vPosition.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool         DISPLAYLEFT   = STICKS(PWINDOW->m_vPosition.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);

    const bool         LEFT = corner == CORNER_TOPLEFT || corner == CORNER_BOTTOMLEFT;
    const bool         TOP  = corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT;
    const bool         NONE = corner == CORNER_NONE;

    const auto         MASTERS      = getMastersOnWorkspace(PNODE->workspaceID);
    const auto         WINDOWS      = getNodesOnWorkspace(PNODE->workspaceID);
    const auto         STACKWINDOWS = WINDOWS - MASTERS;

    if (getNodesOnWorkspace(PWINDOW->m_iWorkspaceID) == 1 && !centered)
        return;

    m_bForceWarps = true;

    switch (orientation) {
        case ORIENTATION_LEFT: delta = pixResize.x / PMONITOR->vecSize.x; break;
        case ORIENTATION_RIGHT: delta = -pixResize.x / PMONITOR->vecSize.x; break;
        case ORIENTATION_BOTTOM: delta = -pixResize.y / PMONITOR->vecSize.y; break;
        case ORIENTATION_TOP: delta = pixResize.y / PMONITOR->vecSize.y; break;
        case ORIENTATION_CENTER:
            delta = pixResize.x / PMONITOR->vecSize.x;
            if (WINDOWS > 2) {
                if (!NONE || !PNODE->isMaster)
                    delta *= 2;
                if ((!PNODE->isMaster && DISPLAYLEFT) || (PNODE->isMaster && LEFT && *PSMARTRESIZING))
                    delta = -delta;
            }
            break;
        default: UNREACHABLE();
    }

    const auto workspaceIdForResizing = PMONITOR->specialWorkspaceID == 0 ? PMONITOR->activeWorkspace : PMONITOR->specialWorkspaceID;
    for (auto& n : m_lMasterNodesData) {
        if (n.isMaster && n.workspaceID == workspaceIdForResizing)
            n.percMaster = std::clamp(n.percMaster + delta, 0.05, 0.95);
    }

    // check the up/down resize
    const bool isStackVertical = orientation == ORIENTATION_LEFT || orientation == ORIENTATION_RIGHT || orientation == ORIENTATION_CENTER;

    const auto RESIZEDELTA = isStackVertical ? pixResize.y : pixResize.x;
    const auto WSSIZE      = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;

    auto       nodesInSameColumn = PNODE->isMaster ? MASTERS : STACKWINDOWS;
    if (orientation == ORIENTATION_CENTER && !PNODE->isMaster)
        nodesInSameColumn = DISPLAYRIGHT ? (nodesInSameColumn + 1) / 2 : nodesInSameColumn / 2;

    const auto SIZE = isStackVertical ? WSSIZE.y / nodesInSameColumn : WSSIZE.x / nodesInSameColumn;

    if (RESIZEDELTA != 0 && nodesInSameColumn > 1) {
        if (!*PSMARTRESIZING) {
            PNODE->percSize = std::clamp(PNODE->percSize + RESIZEDELTA / SIZE, 0.05, 1.95);
        } else {
            const auto  NODEIT    = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), *PNODE);
            const auto  REVNODEIT = std::find(m_lMasterNodesData.rbegin(), m_lMasterNodesData.rend(), *PNODE);

            const float totalSize       = isStackVertical ? WSSIZE.y : WSSIZE.x;
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
                std::for_each(std::next(REVNODEIT), m_lMasterNodesData.rend(), checkNodesLeft);
                resizeDiff = -RESIZEDELTA;
            } else {
                std::for_each(std::next(NODEIT), m_lMasterNodesData.end(), checkNodesLeft);
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
                std::for_each(std::next(REVNODEIT), m_lMasterNodesData.rend(), resizeNodesLeft);
            } else {
                std::for_each(std::next(NODEIT), m_lMasterNodesData.end(), resizeNodesLeft);
            }
        }
    }

    recalculateMonitor(PMONITOR->ID);

    m_bForceWarps = false;
}

void CHyprMasterLayout::fullscreenRequestForWindow(CWindow* pWindow, eFullscreenMode fullscreenMode, bool on) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    if (on == pWindow->m_bIsFullscreen || g_pCompositor->isWorkspaceSpecial(pWindow->m_iWorkspaceID))
        return; // ignore

    const auto PMONITOR   = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->m_bHasFullscreenWindow && on) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen           = on;
    PWORKSPACE->m_bHasFullscreenWindow = !PWORKSPACE->m_bHasFullscreenWindow;

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

        // save position and size if floating
        if (pWindow->m_bIsFloating) {
            pWindow->m_vLastFloatingSize     = pWindow->m_vRealSize.goalv();
            pWindow->m_vLastFloatingPosition = pWindow->m_vRealPosition.goalv();
            pWindow->m_vPosition             = pWindow->m_vRealPosition.goalv();
            pWindow->m_vSize                 = pWindow->m_vRealSize.goalv();
        }

        // apply new pos and size being monitors' box
        if (fullscreenMode == FULLSCREEN_FULL) {
            pWindow->m_vRealPosition = PMONITOR->vecPosition;
            pWindow->m_vRealSize     = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SMasterNodeData fakeNode;
            fakeNode.pWindow     = pWindow;
            fakeNode.position    = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size        = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID = pWindow->m_iWorkspaceID;
            pWindow->m_vPosition = fakeNode.position;
            pWindow->m_vSize     = fakeNode.size;

            applyNodeDataToWindow(&fakeNode);
        }
    }

    g_pCompositor->updateWindowAnimatedDecorationValues(pWindow);

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize.goalv());

    g_pCompositor->changeWindowZOrder(pWindow, true);

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

void CHyprMasterLayout::moveWindowTo(CWindow* pWindow, const std::string& dir) {
    if (!isDirection(dir))
        return;

    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(pWindow, dir[0]);

    if (pWindow->m_iWorkspaceID != PWINDOW2->m_iWorkspaceID) {
        // if different monitors, send to monitor
        onWindowRemovedTiling(pWindow);
        pWindow->moveToWorkspace(PWINDOW2->m_iWorkspaceID);
        pWindow->m_iMonitorID = PWINDOW2->m_iMonitorID;
        onWindowCreatedTiling(pWindow);
    } else {
        // if same monitor, switch windows
        switchWindows(pWindow, PWINDOW2);
    }
}

void CHyprMasterLayout::switchWindows(CWindow* pWindow, CWindow* pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    const auto inheritFullscreen = prepareLoseFocus(pWindow);

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_iMonitorID, pWindow->m_iMonitorID);
        std::swap(pWindow2->m_iWorkspaceID, pWindow->m_iWorkspaceID);
    }

    // massive hack: just swap window pointers, lol
    PNODE->pWindow  = pWindow2;
    PNODE2->pWindow = pWindow;

    recalculateMonitor(pWindow->m_iMonitorID);
    if (PNODE2->workspaceID != PNODE->workspaceID)
        recalculateMonitor(pWindow2->m_iMonitorID);

    g_pHyprRenderer->damageWindow(pWindow);
    g_pHyprRenderer->damageWindow(pWindow2);

    prepareNewFocus(pWindow2, inheritFullscreen);
}

void CHyprMasterLayout::alterSplitRatio(CWindow* pWindow, float ratio, bool exact) {
    // window should be valid, insallah

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto PMASTER = getMasterNodeOnWorkspace(pWindow->m_iWorkspaceID);

    float      newRatio = exact ? ratio : PMASTER->percMaster + ratio;
    PMASTER->percMaster = std::clamp(newRatio, 0.05f, 0.95f);

    recalculateMonitor(pWindow->m_iMonitorID);
}

CWindow* CHyprMasterLayout::getNextWindow(CWindow* pWindow, bool next) {
    if (!isWindowTiled(pWindow))
        return nullptr;

    const auto PNODE = getNodeFromWindow(pWindow);

    auto       nodes = m_lMasterNodesData;
    if (!next)
        std::reverse(nodes.begin(), nodes.end());

    const auto NODEIT = std::find(nodes.begin(), nodes.end(), *PNODE);

    const bool ISMASTER = PNODE->isMaster;

    auto CANDIDATE = std::find_if(NODEIT, nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER == other.isMaster && other.workspaceID == PNODE->workspaceID; });
    if (CANDIDATE == nodes.end())
        CANDIDATE =
            std::find_if(nodes.begin(), nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER != other.isMaster && other.workspaceID == PNODE->workspaceID; });

    return CANDIDATE == nodes.end() ? nullptr : CANDIDATE->pWindow;
}

bool CHyprMasterLayout::prepareLoseFocus(CWindow* pWindow) {
    if (!pWindow)
        return false;

    //if the current window is fullscreen, make it normal again if we are about to lose focus
    if (pWindow->m_bIsFullscreen) {
        g_pCompositor->setWindowFullscreen(pWindow, false, FULLSCREEN_FULL);
        static auto* const INHERIT = &g_pConfigManager->getConfigValuePtr("master:inherit_fullscreen")->intValue;
        return *INHERIT == 1;
    }

    return false;
}

void CHyprMasterLayout::prepareNewFocus(CWindow* pWindow, bool inheritFullscreen) {
    if (!pWindow)
        return;

    if (inheritFullscreen)
        g_pCompositor->setWindowFullscreen(pWindow, true, g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID)->m_efFullscreenMode);
}

std::any CHyprMasterLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    auto switchToWindow = [&](CWindow* PWINDOWTOCHANGETO) {
        if (!g_pCompositor->windowValidMapped(PWINDOWTOCHANGETO))
            return;

        g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
        g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->middle());
    };

    CVarList vars(message, 0, ' ');

    if (vars.size() < 1 || vars[0].empty()) {
        Debug::log(ERR, "layoutmsg called without params");
        return 0;
    }

    auto command = vars[0];

    // swapwithmaster <master | child | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master
    // * child - keep the focus at the new child
    // * auto (default) - swap the focus (keep the focus of the previously selected window)
    if (command == "swapwithmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        if (!isWindowTiled(PWINDOW))
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);

        if (!PMASTER)
            return 0;

        const auto NEWCHILD = PMASTER->pWindow;

        if (PMASTER->pWindow != PWINDOW) {
            const auto NEWMASTER         = PWINDOW;
            const bool newFocusToChild   = vars.size() >= 2 && vars[1] == "child";
            const bool inheritFullscreen = prepareLoseFocus(NEWMASTER);
            switchWindows(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
            prepareNewFocus(NEWFOCUS, inheritFullscreen);
        } else {
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    const auto NEWMASTER         = n.pWindow;
                    const bool inheritFullscreen = prepareLoseFocus(NEWCHILD);
                    switchWindows(NEWMASTER, NEWCHILD);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    const auto NEWFOCUS         = newFocusToMaster ? NEWMASTER : NEWCHILD;
                    switchToWindow(NEWFOCUS);
                    prepareNewFocus(NEWFOCUS, inheritFullscreen);
                    break;
                }
            }
        }

        return 0;
    }
    // focusmaster <master | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master, even if it was focused before
    // * auto (default) - swap the focus with the first child, if the current focus was master, otherwise focus master
    else if (command == "focusmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->m_iWorkspaceID);

        if (!PMASTER)
            return 0;

        if (PMASTER->pWindow != PWINDOW) {
            switchToWindow(PMASTER->pWindow);
            prepareNewFocus(PMASTER->pWindow, inheritFullscreen);
        } else if (vars.size() >= 2 && vars[1] == "master") {
            return 0;
        } else {
            // if master is focused keep master focused (don't do anything)
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchToWindow(n.pWindow);
                    prepareNewFocus(n.pWindow, inheritFullscreen);
                    break;
                }
            }
        }

        return 0;
    } else if (command == "cyclenext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PNEXTWINDOW = getNextWindow(PWINDOW, true);
        switchToWindow(PNEXTWINDOW);
        prepareNewFocus(PNEXTWINDOW, inheritFullscreen);
    } else if (command == "cycleprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const bool inheritFullscreen = prepareLoseFocus(PWINDOW);

        const auto PPREVWINDOW = getNextWindow(PWINDOW, false);
        switchToWindow(PPREVWINDOW);
        prepareNewFocus(PPREVWINDOW, inheritFullscreen);
    } else if (command == "swapnext") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, true);

        if (PWINDOWTOSWAPWITH) {
            prepareLoseFocus(header.pWindow);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (command == "swapprev") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("prev");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, false);

        if (PWINDOWTOSWAPWITH) {
            prepareLoseFocus(header.pWindow);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            g_pCompositor->focusWindow(header.pWindow);
        }
    } else if (command == "addmaster") {
        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto         PNODE = getNodeFromWindow(header.pWindow);

        const auto         WINDOWS    = getNodesOnWorkspace(header.pWindow->m_iWorkspaceID);
        const auto         MASTERS    = getMastersOnWorkspace(header.pWindow->m_iWorkspaceID);
        static const auto* SMALLSPLIT = &g_pConfigManager->getConfigValuePtr("master:allow_small_split")->intValue;

        if (MASTERS + 2 > WINDOWS && *SMALLSPLIT == 0)
            return 0;
        prepareLoseFocus(header.pWindow);

        if (!PNODE || PNODE->isMaster) {
            // first non-master node
            for (auto& n : m_lMasterNodesData) {
                if (n.workspaceID == header.pWindow->m_iWorkspaceID && !n.isMaster) {
                    n.isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);

    } else if (command == "removemaster") {

        if (!g_pCompositor->windowValidMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->m_iWorkspaceID);
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->m_iWorkspaceID);

        if (WINDOWS < 2 || MASTERS < 2)
            return 0;

        prepareLoseFocus(header.pWindow);

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto& nd : m_lMasterNodesData | std::views::reverse) {
                if (nd.workspaceID == header.pWindow->m_iWorkspaceID && nd.isMaster) {
                    nd.isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }

        recalculateMonitor(header.pWindow->m_iMonitorID);
    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        prepareLoseFocus(PWINDOW);

        const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

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

        recalculateMonitor(header.pWindow->m_iMonitorID);

    } else if (command == "orientationnext") {
        runOrientationCycle(header, nullptr, 1);
    } else if (command == "orientationprev") {
        runOrientationCycle(header, nullptr, -1);
    } else if (command == "orientationcycle") {
        runOrientationCycle(header, &vars, 1);
    } else if (command == "mfact") {
        if (vars.size() >= 2) {
            float newMfact = 0;
            try {
                newMfact = std::stof(vars[1]);
            } catch (std::exception& e) {
                Debug::log(ERR, "Argument is invalid: {}", e.what());
                return 0;
            }
            for (auto& nd : m_lMasterNodesData) {
                if (nd.isMaster)
                    nd.percMaster = std::clamp(newMfact, 0.05f, 0.95f);
            }
        }
    }

    return 0;
}

// If vars is null, we use the default list
void CHyprMasterLayout::runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int direction) {
    std::vector<eOrientation> cycle;
    if (vars != nullptr)
        buildOrientationCycleVectorFromVars(cycle, *vars);

    if (cycle.size() == 0)
        buildOrientationCycleVectorFromEOperation(cycle);

    const auto PWINDOW = header.pWindow;

    if (!PWINDOW)
        return;

    prepareLoseFocus(PWINDOW);

    const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->m_iWorkspaceID);

    int        nextOrPrev = 0;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (PWORKSPACEDATA->orientation == cycle.at(i)) {
            nextOrPrev = i + direction;
            break;
        }
    }

    if (nextOrPrev >= (int)cycle.size())
        nextOrPrev = nextOrPrev % (int)cycle.size();
    else if (nextOrPrev < 0)
        nextOrPrev = cycle.size() + (nextOrPrev % (int)cycle.size());

    PWORKSPACEDATA->orientation = cycle.at(nextOrPrev);
    recalculateMonitor(header.pWindow->m_iMonitorID);
}

void CHyprMasterLayout::buildOrientationCycleVectorFromEOperation(std::vector<eOrientation>& cycle) {
    for (int i = 0; i <= ORIENTATION_CENTER; ++i) {
        cycle.push_back((eOrientation)i);
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

void CHyprMasterLayout::replaceWindowDataWith(CWindow* from, CWindow* to) {
    const auto PNODE = getNodeFromWindow(from);

    if (!PNODE)
        return;

    PNODE->pWindow = to;

    applyNodeDataToWindow(PNODE);
}

void CHyprMasterLayout::onEnable() {
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bMappedX11 || !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w.get());
    }
}

void CHyprMasterLayout::onDisable() {
    m_lMasterNodesData.clear();
}
