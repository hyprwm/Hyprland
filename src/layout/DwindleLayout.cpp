#include "DwindleLayout.hpp"
#include "../Compositor.hpp"

void SDwindleNodeData::recalcSizePosRecursive() {
    if (children[0]) {

        if (size.x > size.y) {
            // split sidey
            children[0]->position = position;
            children[0]->size = Vector2D(size.x / 2.f, size.y);
            children[1]->position = Vector2D(position.x + size.x / 2.f, position.y);
            children[1]->size = Vector2D(size.x / 2.f, size.y);
        } else {
            // split toppy bottomy
            children[0]->position = position;
            children[0]->size = Vector2D(size.x, size.y / 2.f);
            children[1]->position = Vector2D(position.x, position.y + size.y / 2.f);
            children[1]->size = Vector2D(size.x, size.y / 2.f);
        }

        if (children[0]->isNode)
            children[0]->recalcSizePosRecursive();
        else
            layout->applyNodeDataToWindow(children[0]);
        if (children[1]->isNode)
            children[1]->recalcSizePosRecursive();
        else
            layout->applyNodeDataToWindow(children[1]);
    } else {
        layout->applyNodeDataToWindow(this);
    }
}

int CHyprDwindleLayout::getNodesOnWorkspace(const int& id) {
    int no = 0;
    for (auto& n : m_lDwindleNodesData) {
        if (n.workspaceID == id)
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

void CHyprDwindleLayout::applyNodeDataToWindow(SDwindleNodeData* pNode) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(g_pCompositor->getWorkspaceByID(pNode->workspaceID)->monitorID);

    if (!PMONITOR){
        Debug::log(ERR, "Orphaned Node %x (workspace ID: %i)!!", pNode, pNode->workspaceID);
        return;
    }

    // Don't set nodes, only windows.
    if (pNode->isNode) 
        return;

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

    PWINDOW->m_vEffectivePosition = PWINDOW->m_vPosition + Vector2D(BORDERSIZE, BORDERSIZE);
    PWINDOW->m_vEffectiveSize = PWINDOW->m_vSize - Vector2D(2 * BORDERSIZE, 2 * BORDERSIZE);

    const auto OFFSETTOPLEFT = Vector2D(DISPLAYLEFT ? GAPSOUT : GAPSIN,
                                        DISPLAYTOP ? GAPSOUT : GAPSIN);

    const auto OFFSETBOTTOMRIGHT = Vector2D(DISPLAYRIGHT ? GAPSOUT : GAPSIN,
                                            DISPLAYBOTTOM ? GAPSOUT : GAPSIN);

    PWINDOW->m_vEffectivePosition = PWINDOW->m_vEffectivePosition + OFFSETTOPLEFT;
    PWINDOW->m_vEffectiveSize = PWINDOW->m_vEffectiveSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vEffectiveSize);
}

void CHyprDwindleLayout::onWindowCreated(CWindow* pWindow) {
    if (pWindow->m_bIsFloating)
        return;

    m_lDwindleNodesData.push_back(SDwindleNodeData());
    const auto PNODE = &m_lDwindleNodesData.back();

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    // Populate the node with our window's data
    PNODE->workspaceID = PMONITOR->activeWorkspace;
    PNODE->pWindow = pWindow;
    PNODE->isNode = false;
    PNODE->layout = this;

    SDwindleNodeData* OPENINGON;
    const auto MONFROMCURSOR = g_pCompositor->getMonitorFromCursor();

    if (PMONITOR->ID == MONFROMCURSOR->ID)
        OPENINGON = getNodeFromWindow(g_pCompositor->vectorToWindowTiled(g_pInputManager->getMouseCoordsInternal()));
    else
        OPENINGON = getFirstNodeOnWorkspace(PMONITOR->activeWorkspace);

    Debug::log(LOG, "OPENINGON: %x, Workspace: %i, Monitor: %i", OPENINGON, PNODE->workspaceID, PMONITOR->ID);

    // if it's the first, it's easy. Make it fullscreen.
    if (!OPENINGON || OPENINGON->pWindow == pWindow) {
        PNODE->position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        PNODE->size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;

        applyNodeDataToWindow(PNODE);

        pWindow->m_vRealPosition = PNODE->position + PNODE->size / 2.f;
        pWindow->m_vRealSize = Vector2D(5, 5);

        return;
    }
    
    // If it's not, get the node under our cursor

    m_lDwindleNodesData.push_back(SDwindleNodeData());
    const auto NEWPARENT = &m_lDwindleNodesData.back();

    // make the parent have the OPENINGON's stats
    NEWPARENT->children[0] = OPENINGON;
    NEWPARENT->children[1] = PNODE;
    NEWPARENT->position = OPENINGON->position;
    NEWPARENT->size = OPENINGON->size;
    NEWPARENT->workspaceID = OPENINGON->workspaceID;
    NEWPARENT->pParent = OPENINGON->pParent;
    NEWPARENT->isNode = true; // it is a node

    // and update the previous parent if it exists
    if (OPENINGON->pParent) {
        if (OPENINGON->pParent->children[0] == OPENINGON) {
            OPENINGON->pParent->children[0] = NEWPARENT;
        } else {
            OPENINGON->pParent->children[1] = NEWPARENT;
        }
    }

    // Update the children
    if (NEWPARENT->size.x > NEWPARENT->size.y) {
        // split sidey
        OPENINGON->position = NEWPARENT->position;
        OPENINGON->size = Vector2D(NEWPARENT->size.x / 2.f, NEWPARENT->size.y);
        PNODE->position = Vector2D(NEWPARENT->position.x + NEWPARENT->size.x / 2.f, NEWPARENT->position.y);
        PNODE->size = Vector2D(NEWPARENT->size.x / 2.f, NEWPARENT->size.y);
    } else {
        // split toppy bottomy
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

    pWindow->m_vRealPosition = PNODE->position + PNODE->size / 2.f;
    pWindow->m_vRealSize = Vector2D(5,5);
}

void CHyprDwindleLayout::onWindowRemoved(CWindow* pWindow) {

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

    // Ignore any recalc events if we have a fullscreen window.
    if (PWORKSPACE->hasFullscreenWindow)
        return;

    const auto TOPNODE = getMasterNodeOnWorkspace(PMONITOR->activeWorkspace);

    if (TOPNODE && PMONITOR) {
        TOPNODE->position = PMONITOR->vecPosition + PMONITOR->vecReservedTopLeft;
        TOPNODE->size = PMONITOR->vecSize - PMONITOR->vecReservedTopLeft - PMONITOR->vecReservedBottomRight;
        TOPNODE->recalcSizePosRecursive();
    }
}

void CHyprDwindleLayout::changeWindowFloatingMode(CWindow* pWindow) {

    if (pWindow->m_bIsFullscreen) {
        Debug::log(LOG, "Rejecting a change float order because window is fullscreen.");

        // restore its' floating mode
        pWindow->m_bIsFloating = !pWindow->m_bIsFloating;
        return;
    }

    const auto PNODE = getNodeFromWindow(pWindow);

    if (!PNODE) {
        // save real pos cuz the func applies the default 5,5 mid
        const auto PSAVEDPOS = pWindow->m_vRealPosition;
        const auto PSAVEDSIZE = pWindow->m_vRealSize;

        onWindowCreated(pWindow);

        pWindow->m_vRealPosition = PSAVEDPOS;
        pWindow->m_vRealSize = PSAVEDSIZE;
    } else {
        onWindowRemoved(pWindow);
    }
}

void CHyprDwindleLayout::onBeginDragWindow() {

    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow;

    m_vBeginDragSizeXY = Vector2D();

    // Window will be floating. Let's check if it's valid. It should be, but I don't like crashing.
    if (!g_pCompositor->windowValidMapped(DRAGGINGWINDOW)) {
        Debug::log(ERR, "Dragging attempted on an invalid window!");
        return;
    }

    if (DRAGGINGWINDOW->m_bIsFullscreen) {
	    Debug::log(LOG, "Rejecting drag on a fullscreen window.");
	    return;
    }


    m_vBeginDragXY = g_pInputManager->getMouseCoordsInternal();
    m_vBeginDragPositionXY = DRAGGINGWINDOW->m_vRealPosition;
    m_vBeginDragSizeXY = DRAGGINGWINDOW->m_vRealSize;
}

void CHyprDwindleLayout::onMouseMove(const Vector2D& mousePos) {
    const auto DRAGGINGWINDOW = g_pInputManager->currentlyDraggedWindow;

    // Window invalid or drag begin size 0,0 meaning we rejected it.
    if (!g_pCompositor->windowValidMapped(DRAGGINGWINDOW) || m_vBeginDragSizeXY == Vector2D())
        return;
        
    const auto DELTA = Vector2D(mousePos.x - m_vBeginDragXY.x, mousePos.y - m_vBeginDragXY.y);

    if (g_pInputManager->dragButton == BTN_LEFT) {
        DRAGGINGWINDOW->m_vRealPosition = m_vBeginDragPositionXY + DELTA;
        DRAGGINGWINDOW->m_vEffectivePosition = DRAGGINGWINDOW->m_vRealPosition;
    } else {
        DRAGGINGWINDOW->m_vRealSize = m_vBeginDragSizeXY + DELTA;
        DRAGGINGWINDOW->m_vRealSize = Vector2D(std::clamp(DRAGGINGWINDOW->m_vRealSize.x, (double)20, (double)999999), std::clamp(DRAGGINGWINDOW->m_vRealSize.y, (double)20, (double)999999));

        DRAGGINGWINDOW->m_vEffectiveSize = DRAGGINGWINDOW->m_vRealSize;

        g_pXWaylandManager->setWindowSize(DRAGGINGWINDOW, DRAGGINGWINDOW->m_vRealSize);
    }

    // get middle point
    Vector2D middle = DRAGGINGWINDOW->m_vRealPosition + DRAGGINGWINDOW->m_vRealSize / 2.f;

    // and check its monitor
    const auto PMONITOR = g_pCompositor->getMonitorFromVector(middle);

    if (PMONITOR) {
        DRAGGINGWINDOW->m_iMonitorID = PMONITOR->ID;
        DRAGGINGWINDOW->m_iWorkspaceID = PMONITOR->activeWorkspace;
    }
}

void CHyprDwindleLayout::onWindowCreatedFloating(CWindow* pWindow) {
    wlr_box desiredGeometry = {0};
    g_pXWaylandManager->getGeometryForWindow(pWindow, &desiredGeometry);
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (desiredGeometry.width <= 0 || desiredGeometry.height <= 0) {
        const auto PWINDOWSURFACE = g_pXWaylandManager->getWindowSurface(pWindow);
        pWindow->m_vEffectiveSize = Vector2D(PWINDOWSURFACE->current.width, PWINDOWSURFACE->current.height);
        pWindow->m_vEffectivePosition = Vector2D(PMONITOR->vecPosition.x + (PMONITOR->vecSize.x - pWindow->m_vRealSize.x) / 2.f, PMONITOR->vecPosition.y + (PMONITOR->vecSize.y - pWindow->m_vRealSize.y) / 2.f);

    } else {
        // we respect the size.
        pWindow->m_vEffectiveSize = Vector2D(desiredGeometry.width, desiredGeometry.height);

        // check if it's on the correct monitor!
        Vector2D middlePoint = Vector2D(desiredGeometry.x, desiredGeometry.y) + Vector2D(desiredGeometry.width, desiredGeometry.height) / 2.f;

                                                                                            // TODO: detect a popup in a more consistent way.
        if (g_pCompositor->getMonitorFromVector(middlePoint)->ID != pWindow->m_iMonitorID || (desiredGeometry.x == 0 && desiredGeometry.y == 0)) {
            // if it's not, fall back to the center placement
            pWindow->m_vEffectivePosition = PMONITOR->vecPosition + Vector2D((PMONITOR->vecSize.x - desiredGeometry.width) / 2.f, (PMONITOR->vecSize.y - desiredGeometry.height) / 2.f);
        } else {
            // if it is, we respect where it wants to put itself.
            // most of these are popups
            pWindow->m_vEffectivePosition = Vector2D(desiredGeometry.x, desiredGeometry.y);
        }
    }

    if (!pWindow->m_bX11DoesntWantBorders) {
        pWindow->m_vRealPosition = pWindow->m_vEffectivePosition + pWindow->m_vEffectiveSize / 2.f;
        pWindow->m_vRealSize = Vector2D(5, 5);
    } else {
        pWindow->m_vRealPosition = pWindow->m_vEffectivePosition;
        pWindow->m_vRealSize = pWindow->m_vEffectiveSize;
    }

    g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize);
    g_pCompositor->fixXWaylandWindowsOnWorkspace(PMONITOR->activeWorkspace);
}

void CHyprDwindleLayout::fullscreenRequestForWindow(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    if (PWORKSPACE->hasFullscreenWindow && !pWindow->m_bIsFullscreen) {
        // if the window wants to be fullscreen but there already is one,
        // ignore the request.
        return;
    }

    // otherwise, accept it.
    pWindow->m_bIsFullscreen = !pWindow->m_bIsFullscreen;
    PWORKSPACE->hasFullscreenWindow = !PWORKSPACE->hasFullscreenWindow;

    if (!pWindow->m_bIsFullscreen) {
        // if it got its fullscreen disabled, set back its node if it had one
        const auto PNODE = getNodeFromWindow(pWindow);
        if (PNODE)
            applyNodeDataToWindow(PNODE);
        else {
            // get back its' dimensions from position and size
            pWindow->m_vEffectivePosition = pWindow->m_vPosition;
            pWindow->m_vEffectiveSize = pWindow->m_vSize;

            g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize);
        }
    } else {
        // if it now got fullscreen, make it fullscreen

        // save position and size if floating
        if (pWindow->m_bIsFloating) {
            pWindow->m_vPosition = pWindow->m_vRealPosition;
            pWindow->m_vSize = pWindow->m_vRealSize;
        }

        // apply new pos and size being monitors' box
        pWindow->m_vEffectivePosition = PMONITOR->vecPosition;
        pWindow->m_vEffectiveSize = PMONITOR->vecSize;

        g_pXWaylandManager->setWindowSize(pWindow, pWindow->m_vRealSize);
    }

    // we need to fix XWayland windows by sending them to NARNIA
    // because otherwise they'd still be recieving mouse events
    g_pCompositor->fixXWaylandWindowsOnWorkspace(PMONITOR->activeWorkspace);
}
