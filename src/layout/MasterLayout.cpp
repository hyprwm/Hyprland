#include "MasterLayout.hpp"
#include "../Compositor.hpp"
#include "../render/decorations/CHyprGroupBarDecoration.hpp"
#include "config/ConfigDataValues.hpp"
#include <ranges>
#include "../config/ConfigValue.hpp"

SMasterNodeData* CHyprMasterLayout::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& nd : m_lMasterNodesData) {
        if (nd.pWindow.lock() == pWindow)
            return &nd;
    }

    return nullptr;
}

int CHyprMasterLayout::getNodesOnWorkspace(const WORKSPACEID& ws) {
    int no = 0;
    for (auto const& n : m_lMasterNodesData) {
        if (n.workspaceID == ws)
            no++;
    }

    return no;
}

int CHyprMasterLayout::getMastersOnWorkspace(const WORKSPACEID& ws) {
    int no = 0;
    for (auto const& n : m_lMasterNodesData) {
        if (n.workspaceID == ws && n.isMaster)
            no++;
    }

    return no;
}

SMasterWorkspaceData* CHyprMasterLayout::getMasterWorkspaceData(const WORKSPACEID& ws) {
    for (auto& n : m_lMasterWorkspacesData) {
        if (n.workspaceID == ws)
            return &n;
    }

    //create on the fly if it doesn't exist yet
    const auto PWORKSPACEDATA   = &m_lMasterWorkspacesData.emplace_back();
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
    for (auto& n : m_lMasterNodesData) {
        if (n.isMaster && n.workspaceID == ws)
            return &n;
    }

    return nullptr;
}

void CHyprMasterLayout::onWindowCreatedTiling(PHLWINDOW pWindow, eDirection direction) {
    if (pWindow->m_bIsFloating)
        return;

    static auto PNEWONACTIVE = CConfigValue<std::string>("master:new_on_active");
    static auto PNEWONTOP    = CConfigValue<Hyprlang::INT>("master:new_on_top");
    static auto PNEWSTATUS   = CConfigValue<std::string>("master:new_status");

    const auto  PMONITOR = pWindow->m_pMonitor.lock();

    const bool  BNEWBEFOREACTIVE = *PNEWONACTIVE == "before";
    const bool  BNEWISMASTER     = *PNEWSTATUS == "master";

    const auto  PNODE = [&]() {
        if (*PNEWONACTIVE != "none" && !BNEWISMASTER) {
            const auto pLastNode = getNodeFromWindow(g_pCompositor->m_pLastWindow.lock());
            if (pLastNode && !(pLastNode->isMaster && (getMastersOnWorkspace(pWindow->workspaceID()) == 1 || *PNEWSTATUS == "slave"))) {
                auto it = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), *pLastNode);
                if (!BNEWBEFOREACTIVE)
                    ++it;
                return &(*m_lMasterNodesData.emplace(it));
            }
        }
        return *PNEWONTOP ? &m_lMasterNodesData.emplace_front() : &m_lMasterNodesData.emplace_back();
    }();

    PNODE->workspaceID = pWindow->workspaceID();
    PNODE->pWindow     = pWindow;

    const auto   WINDOWSONWORKSPACE = getNodesOnWorkspace(PNODE->workspaceID);
    static auto  PMFACT             = CConfigValue<Hyprlang::FLOAT>("master:mfact");
    float        lastSplitPercent   = *PMFACT;

    auto         OPENINGON = isWindowTiled(g_pCompositor->m_pLastWindow.lock()) && g_pCompositor->m_pLastWindow->m_pWorkspace == pWindow->m_pWorkspace ?
                getNodeFromWindow(g_pCompositor->m_pLastWindow.lock()) :
                getMasterNodeOnWorkspace(pWindow->workspaceID());

    const auto   MOUSECOORDS   = g_pInputManager->getMouseCoordsInternal();
    static auto  PDROPATCURSOR = CConfigValue<Hyprlang::INT>("master:drop_at_cursor");
    eOrientation orientation   = getDynamicOrientation(pWindow->m_pWorkspace);
    const auto   NODEIT        = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), *PNODE);

    bool         forceDropAsMaster = false;
    // if dragging window to move, drop it at the cursor position instead of bottom/top of stack
    if (*PDROPATCURSOR && g_pInputManager->dragMode == MBIND_MOVE) {
        if (WINDOWSONWORKSPACE > 2) {
            for (auto it = m_lMasterNodesData.begin(); it != m_lMasterNodesData.end(); ++it) {
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
                    m_lMasterNodesData.splice(it, m_lMasterNodesData, NODEIT);
                    break;
                }
            }
        } else if (WINDOWSONWORKSPACE == 2) {
            // when dropping as the second tiled window in the workspace,
            // make it the master only if the cursor is on the master side of the screen
            for (auto const& nd : m_lMasterNodesData) {
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

    if ((BNEWISMASTER && g_pInputManager->dragMode != MBIND_MOVE)                   //
        || WINDOWSONWORKSPACE == 1                                                  //
        || (WINDOWSONWORKSPACE > 2 && !pWindow->m_bFirstMap && OPENINGON->isMaster) //
        || forceDropAsMaster                                                        //
        || (*PNEWSTATUS == "inherit" && OPENINGON && OPENINGON->isMaster && g_pInputManager->dragMode != MBIND_MOVE)) {

        if (BNEWBEFOREACTIVE) {
            for (auto& nd : m_lMasterNodesData | std::views::reverse) {
                if (nd.isMaster && nd.workspaceID == PNODE->workspaceID) {
                    nd.isMaster      = false;
                    lastSplitPercent = nd.percMaster;
                    break;
                }
            }
        } else {
            for (auto& nd : m_lMasterNodesData) {
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
        if (const auto MAXSIZE = pWindow->requestedMaxSize(); MAXSIZE.x < PMONITOR->vecSize.x * lastSplitPercent || MAXSIZE.y < PMONITOR->vecSize.y) {
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
        if (const auto MAXSIZE = pWindow->requestedMaxSize();
            MAXSIZE.x < PMONITOR->vecSize.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->vecSize.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            // we can't continue. make it floating.
            pWindow->m_bIsFloating = true;
            m_lMasterNodesData.remove(*PNODE);
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedFloating(pWindow);
            return;
        }
    }

    // recalc
    recalculateMonitor(pWindow->monitorID());
}

void CHyprMasterLayout::onWindowRemovedTiling(PHLWINDOW pWindow) {
    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE)
        return;

    const auto  WORKSPACEID = PNODE->workspaceID;
    const auto  MASTERSLEFT = getMastersOnWorkspace(WORKSPACEID);
    static auto SMALLSPLIT  = CConfigValue<Hyprlang::INT>("master:allow_small_split");

    pWindow->unsetWindowData(PRIORITY_LAYOUT);
    pWindow->updateWindowData();

    if (pWindow->isFullscreen())
        g_pCompositor->setWindowFullscreenInternal(pWindow, FSMODE_NONE);

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
    recalculateMonitor(pWindow->monitorID());
}

void CHyprMasterLayout::recalculateMonitor(const MONITORID& monid) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(monid);

    if (!PMONITOR || !PMONITOR->activeWorkspace)
        return;

    g_pHyprRenderer->damageMonitor(PMONITOR);

    if (PMONITOR->activeSpecialWorkspace)
        calculateWorkspace(PMONITOR->activeSpecialWorkspace);

    calculateWorkspace(PMONITOR->activeWorkspace);
}

void CHyprMasterLayout::calculateWorkspace(PHLWORKSPACE pWorkspace) {
    const auto PMONITOR = pWorkspace->m_pMonitor.lock();

    if (!PMONITOR)
        return;

    if (pWorkspace->m_bHasFullscreenWindow) {
        // massive hack from the fullscreen func
        const auto PFULLWINDOW = pWorkspace->getFullscreenWindow();

        if (pWorkspace->m_efFullscreenMode == FSMODE_FULLSCREEN) {
            PFULLWINDOW->m_vRealPosition = PMONITOR->vecPosition;
            PFULLWINDOW->m_vRealSize     = PMONITOR->vecSize;
        } else if (pWorkspace->m_efFullscreenMode == FSMODE_MAXIMIZED) {
            SMasterNodeData fakeNode;
            fakeNode.pWindow                = PFULLWINDOW;
            fakeNode.position               = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size                   = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID            = pWorkspace->m_iID;
            PFULLWINDOW->m_vPosition        = fakeNode.position;
            PFULLWINDOW->m_vSize            = fakeNode.size;
            fakeNode.ignoreFullscreenChecks = true;

            applyNodeDataToWindow(&fakeNode);
        }

        // if has fullscreen, don't calculate the rest
        return;
    }

    const auto PMASTERNODE = getMasterNodeOnWorkspace(pWorkspace->m_iID);

    if (!PMASTERNODE)
        return;

    eOrientation orientation        = getDynamicOrientation(pWorkspace);
    bool         centerMasterWindow = false;
    static auto  ALWAYSCENTER       = CConfigValue<Hyprlang::INT>("master:always_center_master");
    static auto  PIGNORERESERVED    = CConfigValue<Hyprlang::INT>("master:center_ignores_reserved");
    static auto  PSMARTRESIZING     = CConfigValue<Hyprlang::INT>("master:smart_resizing");

    const auto   MASTERS      = getMastersOnWorkspace(pWorkspace->m_iID);
    const auto   WINDOWS      = getNodesOnWorkspace(pWorkspace->m_iID);
    const auto   STACKWINDOWS = WINDOWS - MASTERS;
    const auto   WSSIZE       = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
    const auto   WSPOS        = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;

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
        for (auto const& nd : m_lMasterNodesData) {
            if (nd.workspaceID == pWorkspace->m_iID) {
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
        const float HEIGHT      = STACKWINDOWS != 0 ? WSSIZE.y * PMASTERNODE->percMaster : WSSIZE.y;
        float       widthLeft   = WSSIZE.x;
        int         mastersLeft = MASTERS;
        float       nextX       = 0;
        float       nextY       = 0;

        if (orientation == ORIENTATION_BOTTOM)
            nextY = WSSIZE.y - HEIGHT;

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != pWorkspace->m_iID || !nd.isMaster)
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
        float WIDTH       = *PIGNORERESERVED && centerMasterWindow ? PMONITOR->vecSize.x : WSSIZE.x;
        float heightLeft  = WSSIZE.y;
        int   mastersLeft = MASTERS;
        float nextX       = 0;
        float nextY       = 0;

        if (STACKWINDOWS > 0 || centerMasterWindow)
            WIDTH *= PMASTERNODE->percMaster;

        if (orientation == ORIENTATION_RIGHT) {
            nextX = WSSIZE.x - WIDTH;
        } else if (centerMasterWindow) {
            nextX = ((*PIGNORERESERVED && centerMasterWindow ? PMONITOR->vecSize.x : WSSIZE.x) - WIDTH) / 2;
        }

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID != pWorkspace->m_iID || !nd.isMaster)
                continue;

            float HEIGHT = mastersLeft > 1 ? heightLeft / mastersLeft * nd.percSize : heightLeft;
            if (HEIGHT > heightLeft * 0.9f && mastersLeft > 1)
                HEIGHT = heightLeft * 0.9f;

            if (*PSMARTRESIZING) {
                nd.percSize *= WSSIZE.y / masterAccumulatedSize;
                HEIGHT = masterAverageSize * nd.percSize;
            }

            nd.size     = Vector2D(WIDTH, HEIGHT);
            nd.position = (*PIGNORERESERVED && centerMasterWindow ? PMONITOR->vecPosition : WSPOS) + Vector2D(nextX, nextY);
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
            if (nd.workspaceID != pWorkspace->m_iID || nd.isMaster)
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
            if (nd.workspaceID != pWorkspace->m_iID || nd.isMaster)
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
        const float WIDTH       = ((*PIGNORERESERVED ? PMONITOR->vecSize.x : WSSIZE.x) - PMASTERNODE->size.x) / 2.0;
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
            for (auto const& nd : m_lMasterNodesData) {
                if (nd.workspaceID != pWorkspace->m_iID || nd.isMaster)
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
            if (nd.workspaceID != pWorkspace->m_iID || nd.isMaster)
                continue;

            if (onRight) {
                nextX      = WIDTH + PMASTERNODE->size.x - (*PIGNORERESERVED ? PMONITOR->vecReservedTopLeft.x : 0);
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

            nd.size     = Vector2D(*PIGNORERESERVED ? (WIDTH - (onRight ? PMONITOR->vecReservedBottomRight.x : PMONITOR->vecReservedTopLeft.x)) : WIDTH, HEIGHT);
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
    PHLMONITOR PMONITOR = nullptr;

    if (g_pCompositor->isWorkspaceSpecial(pNode->workspaceID)) {
        for (auto const& m : g_pCompositor->m_vMonitors) {
            if (m->activeSpecialWorkspaceID() == pNode->workspaceID) {
                PMONITOR = m;
                break;
            }
        }
    } else
        PMONITOR = g_pCompositor->getWorkspaceByID(pNode->workspaceID)->m_pMonitor.lock();

    if (!PMONITOR) {
        Debug::log(ERR, "Orphaned Node {}!!", pNode);
        return;
    }

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    const auto PWINDOW = pNode->pWindow.lock();
    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(PWINDOW->m_pWorkspace);

    if (PWINDOW->isFullscreen() && !pNode->ignoreFullscreenChecks)
        return;

    PWINDOW->unsetWindowData(PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();

    static auto PANIMATE     = CConfigValue<Hyprlang::INT>("misc:animate_manual_resizes");
    static auto PGAPSINDATA  = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    static auto PGAPSOUTDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_out");
    auto*       PGAPSIN      = (CCssGapData*)(PGAPSINDATA.ptr())->getData();
    auto*       PGAPSOUT     = (CCssGapData*)(PGAPSOUTDATA.ptr())->getData();

    auto        gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    auto        gapsOut = WORKSPACERULE.gapsOut.value_or(*PGAPSOUT);

    if (!validMapped(PWINDOW)) {
        Debug::log(ERR, "Node {} holding invalid {}!!", pNode, PWINDOW);
        return;
    }

    PWINDOW->m_vSize     = pNode->size;
    PWINDOW->m_vPosition = pNode->position;

    PWINDOW->updateWindowDecos();

    auto       calcPos  = PWINDOW->m_vPosition;
    auto       calcSize = PWINDOW->m_vSize;

    const auto OFFSETTOPLEFT = Vector2D((double)(DISPLAYLEFT ? gapsOut.left : gapsIn.left), (double)(DISPLAYTOP ? gapsOut.top : gapsIn.top));

    const auto OFFSETBOTTOMRIGHT = Vector2D((double)(DISPLAYRIGHT ? gapsOut.right : gapsIn.right), (double)(DISPLAYBOTTOM ? gapsOut.bottom : gapsIn.bottom));

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (PWINDOW->onSpecialWorkspace() && !PWINDOW->isFullscreen()) {
        static auto PSCALEFACTOR = CConfigValue<Hyprlang::FLOAT>("master:special_scale_factor");

        CBox        wb = {calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f, calcSize * *PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        PWINDOW->m_vRealPosition = wb.pos();
        PWINDOW->m_vRealSize     = wb.size();

        g_pXWaylandManager->setWindowSize(PWINDOW, wb.size());
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round(); // avoid rounding mess

        PWINDOW->m_vRealPosition = wb.pos();
        PWINDOW->m_vRealSize     = wb.size();

        g_pXWaylandManager->setWindowSize(PWINDOW, wb.size());
    }

    if (m_bForceWarps && !*PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);

        PWINDOW->m_vRealPosition.warp();
        PWINDOW->m_vRealSize.warp();

        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

bool CHyprMasterLayout::isWindowTiled(PHLWINDOW pWindow) {
    return getNodeFromWindow(pWindow) != nullptr;
}

void CHyprMasterLayout::resizeActiveWindow(const Vector2D& pixResize, eRectCorner corner, PHLWINDOW pWindow) {
    const auto PWINDOW = pWindow ? pWindow : g_pCompositor->m_pLastWindow.lock();

    if (!validMapped(PWINDOW))
        return;

    const auto PNODE = getNodeFromWindow(PWINDOW);

    if (!PNODE) {
        PWINDOW->m_vRealSize =
            (PWINDOW->m_vRealSize.goal() + pixResize)
                .clamp(PWINDOW->m_sWindowData.minSize.valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), PWINDOW->m_sWindowData.maxSize.valueOr(Vector2D{INFINITY, INFINITY}));
        PWINDOW->updateWindowDecos();
        return;
    }

    const auto   PMONITOR       = PWINDOW->m_pMonitor.lock();
    static auto  ALWAYSCENTER   = CConfigValue<Hyprlang::INT>("master:always_center_master");
    static auto  PSMARTRESIZING = CConfigValue<Hyprlang::INT>("master:smart_resizing");

    eOrientation orientation = getDynamicOrientation(PWINDOW->m_pWorkspace);
    bool         centered    = orientation == ORIENTATION_CENTER && (*ALWAYSCENTER == 1);
    double       delta       = 0;

    const bool   DISPLAYBOTTOM = STICKS(PWINDOW->m_vPosition.y + PWINDOW->m_vSize.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);
    const bool   DISPLAYRIGHT  = STICKS(PWINDOW->m_vPosition.x + PWINDOW->m_vSize.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool   DISPLAYTOP    = STICKS(PWINDOW->m_vPosition.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool   DISPLAYLEFT   = STICKS(PWINDOW->m_vPosition.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);

    const bool   LEFT = corner == CORNER_TOPLEFT || corner == CORNER_BOTTOMLEFT;
    const bool   TOP  = corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT;
    const bool   NONE = corner == CORNER_NONE;

    const auto   MASTERS      = getMastersOnWorkspace(PNODE->workspaceID);
    const auto   WINDOWS      = getNodesOnWorkspace(PNODE->workspaceID);
    const auto   STACKWINDOWS = WINDOWS - MASTERS;

    if (getNodesOnWorkspace(PWINDOW->workspaceID()) == 1 && !centered)
        return;

    m_bForceWarps = true;

    switch (orientation) {
        case ORIENTATION_LEFT: delta = pixResize.x / PMONITOR->vecSize.x; break;
        case ORIENTATION_RIGHT: delta = -pixResize.x / PMONITOR->vecSize.x; break;
        case ORIENTATION_BOTTOM: delta = -pixResize.y / PMONITOR->vecSize.y; break;
        case ORIENTATION_TOP: delta = pixResize.y / PMONITOR->vecSize.y; break;
        case ORIENTATION_CENTER:
            delta = pixResize.x / PMONITOR->vecSize.x;
            if (WINDOWS > 2 || *ALWAYSCENTER) {
                if (!NONE || !PNODE->isMaster)
                    delta *= 2;
                if ((!PNODE->isMaster && DISPLAYLEFT) || (PNODE->isMaster && LEFT && *PSMARTRESIZING))
                    delta = -delta;
            }
            break;
        default: UNREACHABLE();
    }

    const auto workspaceIdForResizing = PMONITOR->activeSpecialWorkspace ? PMONITOR->activeSpecialWorkspaceID() : PMONITOR->activeWorkspaceID();
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

void CHyprMasterLayout::fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE) {
    const auto PMONITOR   = pWindow->m_pMonitor.lock();
    const auto PWORKSPACE = pWindow->m_pWorkspace;

    // save position and size if floating
    if (pWindow->m_bIsFloating && CURRENT_EFFECTIVE_MODE == FSMODE_NONE) {
        pWindow->m_vLastFloatingSize     = pWindow->m_vRealSize.goal();
        pWindow->m_vLastFloatingPosition = pWindow->m_vRealPosition.goal();
        pWindow->m_vPosition             = pWindow->m_vRealPosition.goal();
        pWindow->m_vSize                 = pWindow->m_vRealSize.goal();
    }

    if (EFFECTIVE_MODE == FSMODE_NONE) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            pWindow->m_vRealPosition = pWindow->m_vLastFloatingPosition;
            pWindow->m_vRealSize     = pWindow->m_vLastFloatingSize;

            pWindow->unsetWindowData(PRIORITY_LAYOUT);
            pWindow->updateWindowData();
        }
    } else {
        // apply new pos and size being monitors' box
        if (EFFECTIVE_MODE == FSMODE_FULLSCREEN) {
            pWindow->m_vRealPosition = PMONITOR->vecPosition;
            pWindow->m_vRealSize     = PMONITOR->vecSize;
        } else {
            // This is a massive hack.
            // We make a fake "only" node and apply
            // To keep consistent with the settings without C+P code

            SMasterNodeData fakeNode;
            fakeNode.pWindow                = pWindow;
            fakeNode.position               = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
            fakeNode.size                   = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
            fakeNode.workspaceID            = pWindow->workspaceID();
            pWindow->m_vPosition            = fakeNode.position;
            pWindow->m_vSize                = fakeNode.size;
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

    return hints; // master doesnt have any hints
}

void CHyprMasterLayout::moveWindowTo(PHLWINDOW pWindow, const std::string& dir, bool silent) {
    if (!isDirection(dir))
        return;

    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(pWindow, dir[0]);

    if (!PWINDOW2)
        return;

    pWindow->setAnimationsToMove();

    if (pWindow->m_pWorkspace != PWINDOW2->m_pWorkspace) {
        // if different monitors, send to monitor
        onWindowRemovedTiling(pWindow);
        pWindow->moveToWorkspace(PWINDOW2->m_pWorkspace);
        pWindow->m_pMonitor = PWINDOW2->m_pMonitor;
        if (!silent) {
            const auto pMonitor = pWindow->m_pMonitor.lock();
            g_pCompositor->setActiveMonitor(pMonitor);
        }
        onWindowCreatedTiling(pWindow);
    } else {
        // if same monitor, switch windows
        switchWindows(pWindow, PWINDOW2);
        if (silent)
            g_pCompositor->focusWindow(PWINDOW2);
    }
}

void CHyprMasterLayout::switchWindows(PHLWINDOW pWindow, PHLWINDOW pWindow2) {
    // windows should be valid, insallah

    const auto PNODE  = getNodeFromWindow(pWindow);
    const auto PNODE2 = getNodeFromWindow(pWindow2);

    if (!PNODE2 || !PNODE)
        return;

    if (PNODE->workspaceID != PNODE2->workspaceID) {
        std::swap(pWindow2->m_pMonitor, pWindow->m_pMonitor);
        std::swap(pWindow2->m_pWorkspace, pWindow->m_pWorkspace);
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

PHLWINDOW CHyprMasterLayout::getNextWindow(PHLWINDOW pWindow, bool next) {
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

    return CANDIDATE == nodes.end() ? nullptr : CANDIDATE->pWindow.lock();
}

std::any CHyprMasterLayout::layoutMessage(SLayoutMessageHeader header, std::string message) {
    auto switchToWindow = [&](PHLWINDOW PWINDOWTOCHANGETO) {
        if (!validMapped(PWINDOWTOCHANGETO))
            return;

        if (header.pWindow->isFullscreen()) {
            const auto  PWORKSPACE        = header.pWindow->m_pWorkspace;
            const auto  FSMODE            = header.pWindow->m_sFullscreenState.internal;
            static auto INHERITFULLSCREEN = CConfigValue<Hyprlang::INT>("master:inherit_fullscreen");
            g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            if (*INHERITFULLSCREEN)
                g_pCompositor->setWindowFullscreenInternal(PWINDOWTOCHANGETO, FSMODE);
        } else {
            g_pCompositor->focusWindow(PWINDOWTOCHANGETO);
            g_pCompositor->warpCursorTo(PWINDOWTOCHANGETO->middle());
        }

        g_pInputManager->m_pForcedFocus = PWINDOWTOCHANGETO;
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_pForcedFocus.reset();
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

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        const auto NEWCHILD = PMASTER->pWindow.lock();

        if (PMASTER->pWindow.lock() != PWINDOW) {
            const auto NEWMASTER       = PWINDOW;
            const bool newFocusToChild = vars.size() >= 2 && vars[1] == "child";
            switchWindows(NEWMASTER, NEWCHILD);
            const auto NEWFOCUS = newFocusToChild ? NEWCHILD : NEWMASTER;
            switchToWindow(NEWFOCUS);
        } else {
            for (auto const& n : m_lMasterNodesData) {
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
    // focusmaster <master | auto>
    // first message argument can have the following values:
    // * master - keep the focus at the new master, even if it was focused before
    // * auto (default) - swap the focus with the first child, if the current focus was master, otherwise focus master
    else if (command == "focusmaster") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PMASTER = getMasterNodeOnWorkspace(PWINDOW->workspaceID());

        if (!PMASTER)
            return 0;

        if (PMASTER->pWindow.lock() != PWINDOW) {
            switchToWindow(PMASTER->pWindow.lock());
        } else if (vars.size() >= 2 && vars[1] == "master") {
            return 0;
        } else {
            // if master is focused keep master focused (don't do anything)
            for (auto const& n : m_lMasterNodesData) {
                if (n.workspaceID == PMASTER->workspaceID && !n.isMaster) {
                    switchToWindow(n.pWindow.lock());
                    break;
                }
            }
        }

        return 0;
    } else if (command == "cyclenext") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PNEXTWINDOW = getNextWindow(PWINDOW, true);
        switchToWindow(PNEXTWINDOW);
    } else if (command == "cycleprev") {
        const auto PWINDOW = header.pWindow;

        if (!PWINDOW)
            return 0;

        const auto PPREVWINDOW = getNextWindow(PWINDOW, false);
        switchToWindow(PPREVWINDOW);
    } else if (command == "swapnext") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, true);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            switchToWindow(header.pWindow);
        }
    } else if (command == "swapprev") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating) {
            g_pKeybindManager->m_mDispatchers["swapnext"]("prev");
            return 0;
        }

        const auto PWINDOWTOSWAPWITH = getNextWindow(header.pWindow, false);

        if (PWINDOWTOSWAPWITH) {
            g_pCompositor->setWindowFullscreenClient(header.pWindow, FSMODE_NONE);
            switchWindows(header.pWindow, PWINDOWTOSWAPWITH);
            switchToWindow(header.pWindow);
        }
    } else if (command == "addmaster") {
        if (!validMapped(header.pWindow))
            return 0;

        if (header.pWindow->m_bIsFloating)
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
            for (auto& n : m_lMasterNodesData) {
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

        if (header.pWindow->m_bIsFloating)
            return 0;

        const auto PNODE = getNodeFromWindow(header.pWindow);

        const auto WINDOWS = getNodesOnWorkspace(header.pWindow->workspaceID());
        const auto MASTERS = getMastersOnWorkspace(header.pWindow->workspaceID());

        if (WINDOWS < 2 || MASTERS < 2)
            return 0;

        g_pCompositor->setWindowFullscreenInternal(header.pWindow, FSMODE_NONE);

        if (!PNODE || !PNODE->isMaster) {
            // first non-master node
            for (auto& nd : m_lMasterNodesData | std::views::reverse) {
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
        g_pKeybindManager->m_mDispatchers["splitratio"](vars[1] + " " + vars[2]);
    } else if (command == "rollnext") {
        const auto PWINDOW = header.pWindow;
        const auto PNODE   = getNodeFromWindow(PWINDOW);

        if (!PNODE)
            return 0;

        const auto OLDMASTER = PNODE->isMaster ? PNODE : getMasterNodeOnWorkspace(PNODE->workspaceID);
        if (!OLDMASTER)
            return 0;

        const auto OLDMASTERIT = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), *OLDMASTER);

        for (auto& nd : m_lMasterNodesData) {
            if (nd.workspaceID == PNODE->workspaceID && !nd.isMaster) {
                nd.isMaster            = true;
                const auto NEWMASTERIT = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), nd);
                m_lMasterNodesData.splice(OLDMASTERIT, m_lMasterNodesData, NEWMASTERIT);
                switchToWindow(nd.pWindow.lock());
                OLDMASTER->isMaster = false;
                m_lMasterNodesData.splice(m_lMasterNodesData.end(), m_lMasterNodesData, OLDMASTERIT);
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

        const auto OLDMASTERIT = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), *OLDMASTER);

        for (auto& nd : m_lMasterNodesData | std::views::reverse) {
            if (nd.workspaceID == PNODE->workspaceID && !nd.isMaster) {
                nd.isMaster            = true;
                const auto NEWMASTERIT = std::find(m_lMasterNodesData.begin(), m_lMasterNodesData.end(), nd);
                m_lMasterNodesData.splice(OLDMASTERIT, m_lMasterNodesData, NEWMASTERIT);
                switchToWindow(nd.pWindow.lock());
                OLDMASTER->isMaster = false;
                m_lMasterNodesData.splice(m_lMasterNodesData.begin(), m_lMasterNodesData, OLDMASTERIT);
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

    if (cycle.size() == 0)
        buildOrientationCycleVectorFromEOperation(cycle);

    const auto PWINDOW = header.pWindow;

    if (!PWINDOW)
        return;

    g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);

    const auto PWORKSPACEDATA = getMasterWorkspaceData(PWINDOW->workspaceID());

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
    recalculateMonitor(header.pWindow->monitorID());
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

eOrientation CHyprMasterLayout::getDynamicOrientation(PHLWORKSPACE pWorkspace) {
    const auto  WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(pWorkspace);
    std::string orientationString;
    if (WORKSPACERULE.layoutopts.contains("orientation"))
        orientationString = WORKSPACERULE.layoutopts.at("orientation");

    eOrientation orientation = getMasterWorkspaceData(pWorkspace->m_iID)->orientation;
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

    if (!g_pCompositor->m_pLastMonitor)
        return {};

    const int NODES = getNodesOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspace->m_iID);

    if (NODES <= 0)
        return g_pCompositor->m_pLastMonitor->vecSize;

    const auto MASTER = getMasterNodeOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspace->m_iID);
    if (!MASTER) // wtf
        return {};

    if (*PNEWSTATUS == "master") {
        return MASTER->size;
    } else {
        const auto SLAVES = NODES - getMastersOnWorkspace(g_pCompositor->m_pLastMonitor->activeWorkspace->m_iID);

        // TODO: make this better
        return {g_pCompositor->m_pLastMonitor->vecSize.x - MASTER->size.x, g_pCompositor->m_pLastMonitor->vecSize.y / (SLAVES + 1)};
    }

    return {};
}

void CHyprMasterLayout::onEnable() {
    for (auto const& w : g_pCompositor->m_vWindows) {
        if (w->m_bIsFloating || !w->m_bIsMapped || w->isHidden())
            continue;

        onWindowCreatedTiling(w);
    }
}

void CHyprMasterLayout::onDisable() {
    m_lMasterNodesData.clear();
}
