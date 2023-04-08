#include "SubsurfaceTree.hpp"
#include "../events/Events.hpp"
#include "../Compositor.hpp"

void addSurfaceGlobalOffset(SSurfaceTreeNode* node, int* lx, int* ly) {
    if (!node->pSurface || !node->pSurface->exists())
        return;

    *lx += node->pSurface->wlr()->current.dx;
    *ly += node->pSurface->wlr()->current.dy;

    if (node->offsetfn) {
        // This is the root node
        RASSERT(!node->pSubsurface, "Node had no subsurface!");
        node->offsetfn(node->globalOffsetData, lx, ly);
    } else {
        RASSERT(node->pSubsurface, "Node had no subsurface!");

        *lx += node->pSubsurface->pSubsurface->current.x;
        *ly += node->pSubsurface->pSubsurface->current.y;

        addSurfaceGlobalOffset(node->pParent, lx, ly);
    }
}

SSurfaceTreeNode* createTree(wlr_surface* pSurface, CWindow* pWindow) {
    const auto PNODE = &SubsurfaceTree::surfaceTreeNodes.emplace_back();

    if (pSurface->data)
        PNODE->pSurface = (CWLSurface*)pSurface->data;
    else {
        PNODE->pInternalSurface = pSurface;
        PNODE->pSurface         = &PNODE->pInternalSurface;
    }

    PNODE->pWindowOwner = pWindow;

    PNODE->hyprListener_newSubsurface.initCallback(&pSurface->events.new_subsurface, &Events::listener_newSubsurfaceNode, PNODE, "SurfaceTreeNode");
    PNODE->hyprListener_commit.initCallback(&pSurface->events.commit, &Events::listener_commitSubsurface, PNODE, "SurfaceTreeNode");
    PNODE->hyprListener_destroy.initCallback(&pSurface->events.destroy, &Events::listener_destroySubsurfaceNode, PNODE, "SurfaceTreeNode");

    wlr_subsurface* wlrSubsurface;
    wl_list_for_each(wlrSubsurface, &pSurface->current.subsurfaces_below, current.link) {
        Events::listener_newSubsurfaceNode(PNODE, wlrSubsurface);
    }
    wl_list_for_each(wlrSubsurface, &pSurface->current.subsurfaces_above, current.link) {
        Events::listener_newSubsurfaceNode(PNODE, wlrSubsurface);
    }

    return PNODE;
}

SSurfaceTreeNode* createSubsurfaceNode(SSurfaceTreeNode* pParent, SSubsurface* pSubsurface, wlr_surface* surface, CWindow* pWindow) {
    const auto PNODE   = createTree(surface, pWindow);
    PNODE->pParent     = pParent;
    PNODE->pSubsurface = pSubsurface;

    Debug::log(LOG, "Creating a subsurface Node! (pWindow: %x)", pWindow);

    return PNODE;
}

SSurfaceTreeNode* SubsurfaceTree::createTreeRoot(wlr_surface* pSurface, applyGlobalOffsetFn fn, void* data, CWindow* pWindow) {
    const auto PNODE = createTree(pSurface, pWindow);

    Debug::log(LOG, "Creating a surfaceTree Root! (pWindow: %x)", pWindow);

    PNODE->offsetfn         = fn;
    PNODE->globalOffsetData = data;

    return PNODE;
}

void destroySubsurface(SSubsurface* pSubsurface);

void SubsurfaceTree::destroySurfaceTree(SSurfaceTreeNode* pNode) {
    bool exists = false;
    for (auto& n : surfaceTreeNodes) {
        if (&n == pNode) {
            exists = true;
            break;
        }
    }

    if (!exists) {
        Debug::log(ERR, "Tried to remove a SurfaceTreeNode that doesn't exist?? (Node %x)", pNode);
        return;
    }

    for (auto& c : pNode->childSubsurfaces)
        destroySubsurface(&c);

    pNode->childSubsurfaces.clear();

    pNode->hyprListener_commit.removeCallback();
    pNode->hyprListener_destroy.removeCallback();
    pNode->hyprListener_newSubsurface.removeCallback();

    // damage
    if (pNode->pSurface && pNode->pSurface->exists()) {
        wlr_box extents = {};
        wlr_surface_get_extends(pNode->pSurface->wlr(), &extents);

        int lx = 0, ly = 0;
        addSurfaceGlobalOffset(pNode, &lx, &ly);

        extents.x += lx;
        extents.y += ly;

        g_pHyprRenderer->damageBox(&extents);
    }

    // remove references to this node
    for (auto& tn : surfaceTreeNodes) {
        for (auto& cs : tn.childSubsurfaces) {
            if (cs.pChild == pNode)
                cs.pChild = nullptr;
        }
    }

    surfaceTreeNodes.remove(*pNode);

    Debug::log(LOG, "SurfaceTree Node removed");
}

void destroySubsurface(SSubsurface* pSubsurface) {
    if (pSubsurface->pChild) {
        SubsurfaceTree::destroySurfaceTree(pSubsurface->pChild);
        pSubsurface->pChild = nullptr;
    }

    pSubsurface->hyprListener_destroy.removeCallback();
    pSubsurface->hyprListener_map.removeCallback();
    pSubsurface->hyprListener_unmap.removeCallback();
}

//
// Subsurface listeners
//

void Events::listener_newSubsurfaceNode(void* owner, void* data) {
    SSurfaceTreeNode* pNode = (SSurfaceTreeNode*)owner;

    const auto        PSUBSURFACE = (wlr_subsurface*)data;

    const auto        PNEWSUBSURFACE = &pNode->childSubsurfaces.emplace_back();

    Debug::log(LOG, "Added a new subsurface %x", PSUBSURFACE);

    PNEWSUBSURFACE->pSubsurface = PSUBSURFACE;
    PNEWSUBSURFACE->pParent     = pNode;

    PNEWSUBSURFACE->hyprListener_map.initCallback(&PSUBSURFACE->events.map, &Events::listener_mapSubsurface, PNEWSUBSURFACE, "Subsurface");
    PNEWSUBSURFACE->hyprListener_unmap.initCallback(&PSUBSURFACE->events.unmap, &Events::listener_unmapSubsurface, PNEWSUBSURFACE, "Subsurface");
    PNEWSUBSURFACE->hyprListener_destroy.initCallback(&PSUBSURFACE->events.destroy, &Events::listener_destroySubsurface, PNEWSUBSURFACE, "Subsurface");

    PNEWSUBSURFACE->pWindowOwner = pNode->pWindowOwner;

    if (PSUBSURFACE->mapped)
        listener_mapSubsurface(PNEWSUBSURFACE, nullptr);

    wlr_subsurface* existingWlrSubsurface;
    wl_list_for_each(existingWlrSubsurface, &PSUBSURFACE->surface->current.subsurfaces_below, current.link) {
        listener_newSubsurfaceNode(pNode, existingWlrSubsurface);
    }
    wl_list_for_each(existingWlrSubsurface, &PSUBSURFACE->surface->current.subsurfaces_above, current.link) {
        listener_newSubsurfaceNode(pNode, existingWlrSubsurface);
    }
}

void Events::listener_mapSubsurface(void* owner, void* data) {
    SSubsurface* subsurface = (SSubsurface*)owner;

    if (subsurface->pChild)
        return;

    Debug::log(LOG, "Subsurface %x mapped", subsurface->pSubsurface);

    subsurface->pChild = createSubsurfaceNode(subsurface->pParent, subsurface, subsurface->pSubsurface->surface, subsurface->pWindowOwner);
}

void Events::listener_unmapSubsurface(void* owner, void* data) {
    SSubsurface* subsurface = (SSubsurface*)owner;

    Debug::log(LOG, "Subsurface %x unmapped", subsurface);

    if (subsurface->pSubsurface->surface == g_pCompositor->m_pLastFocus)
        g_pInputManager->releaseAllMouseButtons();

    if (subsurface->pChild) {
        const auto PNODE = subsurface->pChild;

        const auto IT =
            std::find_if(SubsurfaceTree::surfaceTreeNodes.begin(), SubsurfaceTree::surfaceTreeNodes.end(), [&](const SSurfaceTreeNode& other) { return &other == PNODE; });

        if (IT != SubsurfaceTree::surfaceTreeNodes.end()) {
            if (PNODE->pSurface && PNODE->pSurface->exists()) {
                int lx = 0, ly = 0;
                addSurfaceGlobalOffset(PNODE, &lx, &ly);

                wlr_box extents = {lx, ly, 0, 0};

                extents.width  = PNODE->pSurface->wlr()->current.width;
                extents.height = PNODE->pSurface->wlr()->current.height;

                g_pHyprRenderer->damageBox(&extents);
            }

            // SubsurfaceTree::destroySurfaceTree(subsurface->pChild);
            // subsurface->pChild = nullptr;
        }
    }

    g_pInputManager->simulateMouseMovement(); // to focus and return back to an appropriate surface
}

void Events::listener_commitSubsurface(void* owner, void* data) {
    SSurfaceTreeNode* pNode = (SSurfaceTreeNode*)owner;

    // no damaging if it's not visible
    if (!g_pHyprRenderer->shouldRenderWindow(pNode->pWindowOwner)) {
        static auto* const PLOGDAMAGE = &g_pConfigManager->getConfigValuePtr("debug:log_damage")->intValue;
        if (*PLOGDAMAGE)
            Debug::log(LOG, "Refusing to commit damage from %x because it's invisible.", pNode->pWindowOwner);
        return;
    }

    int lx = 0, ly = 0;

    addSurfaceGlobalOffset(pNode, &lx, &ly);

    // I do not think this is correct, but it solves a lot of issues with some apps (e.g. firefox)
    // What this does is that basically, if the pNode is a child of some other node, on commit,
    // it will also damage (check & damage if needed) all its siblings.
    if (pNode->pParent)
        for (auto& cs : pNode->pParent->childSubsurfaces) {
            const auto NODECOORDS = pNode->pSubsurface ? Vector2D(pNode->pSubsurface->pSubsurface->current.x, pNode->pSubsurface->pSubsurface->current.y) : Vector2D();

            if (&cs != pNode->pSubsurface && cs.pSubsurface) {
                g_pHyprRenderer->damageSurface(cs.pSubsurface->surface, lx - NODECOORDS.x + cs.pSubsurface->current.x, ly - NODECOORDS.y + cs.pSubsurface->current.y);
            }
        }

    if (pNode->pSurface && pNode->pSurface->exists())
        g_pHyprRenderer->damageSurface(pNode->pSurface->wlr(), lx, ly);
}

void Events::listener_destroySubsurface(void* owner, void* data) {
    SSubsurface* subsurface = (SSubsurface*)owner;

    if (subsurface->pChild) {
        SubsurfaceTree::destroySurfaceTree(subsurface->pChild);
    }

    Debug::log(LOG, "Subsurface %x destroyed", subsurface);

    subsurface->hyprListener_destroy.removeCallback();
    subsurface->hyprListener_map.removeCallback();
    subsurface->hyprListener_unmap.removeCallback();

    subsurface->pParent->childSubsurfaces.remove(*subsurface);
}

void Events::listener_destroySubsurfaceNode(void* owner, void* data) {
    SSurfaceTreeNode* pNode = (SSurfaceTreeNode*)owner;

    Debug::log(LOG, "Subsurface Node %x destroyed", pNode);

    for (auto& c : pNode->childSubsurfaces)
        destroySubsurface(&c);

    pNode->hyprListener_commit.removeCallback();
    pNode->hyprListener_newSubsurface.removeCallback();
    pNode->hyprListener_destroy.removeCallback();

    SubsurfaceTree::surfaceTreeNodes.remove(*pNode);
}
