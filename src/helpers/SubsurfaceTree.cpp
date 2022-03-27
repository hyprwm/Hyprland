#include "SubsurfaceTree.hpp"
#include "../events/Events.hpp"

void addSurfaceGlobalOffset(SSurfaceTreeNode* node, int* lx, int* ly) {
    *lx += node->pSurface->sx;
    *ly += node->pSurface->sy;

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

SSurfaceTreeNode* createTree(wlr_surface* pSurface) {
    SubsurfaceTree::surfaceTreeNodes.push_back(SSurfaceTreeNode());

    const auto PNODE = &SubsurfaceTree::surfaceTreeNodes.back();

    PNODE->pSurface = pSurface;

    PNODE->listen_newSubsurface.notify = Events::listener_newSubsurfaceNode;
    PNODE->listen_commit.notify = Events::listener_commitSubsurface;
    PNODE->listen_destroy.notify = Events::listener_destroySubsurfaceNode;

    wl_signal_add(&pSurface->events.commit, &PNODE->listen_commit);
    wl_signal_add(&pSurface->events.destroy, &PNODE->listen_destroy);
    wl_signal_add(&pSurface->events.new_subsurface, &PNODE->listen_newSubsurface);

    return PNODE;
}

SSurfaceTreeNode* createSubsurfaceNode(SSurfaceTreeNode* pParent, SSubsurface* pSubsurface, wlr_surface* surface) {
    const auto PNODE = createTree(surface);
    PNODE->pParent = pParent;
    PNODE->pSubsurface = pSubsurface;

    return PNODE;
}

SSurfaceTreeNode* SubsurfaceTree::createTreeRoot(wlr_surface* pSurface, applyGlobalOffsetFn fn, void* data) {
    const auto PNODE = createTree(pSurface);
    PNODE->offsetfn = fn;
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

    RASSERT(exists, "Tried to delete a surfaceTreeNode that doesn't exist!");

    for (auto& c : pNode->childSubsurfaces)
        destroySubsurface(&c);

    pNode->childSubsurfaces.clear();

    wl_list_remove(&pNode->listen_newSubsurface.link);
    wl_list_remove(&pNode->listen_commit.link);
    wl_list_remove(&pNode->listen_destroy.link);

    Debug::log(LOG, "SurfaceTree Node removed");
}

void destroySubsurface(SSubsurface* pSubsurface) {
    if (pSubsurface->pChild) {
        SubsurfaceTree::destroySurfaceTree(pSubsurface->pChild);
        pSubsurface->pChild = nullptr;
    }

    wl_list_remove(&pSubsurface->listen_map.link);
    wl_list_remove(&pSubsurface->listen_unmap.link);
    wl_list_remove(&pSubsurface->listen_destroy.link);
}

//
// Subsurface listeners
//

void Events::listener_newSubsurfaceNode(wl_listener* listener, void* data) {
    SSurfaceTreeNode* pNode = wl_container_of(listener, pNode, listen_newSubsurface);

    const auto PSUBSURFACE = (wlr_subsurface*)data;

    pNode->childSubsurfaces.push_back(SSubsurface());
    const auto PNEWSUBSURFACE = &pNode->childSubsurfaces.back();

    Debug::log(LOG, "Added a new subsurface %x", PSUBSURFACE);

    PNEWSUBSURFACE->pSubsurface = PSUBSURFACE;
    PNEWSUBSURFACE->pParent = pNode;

    PNEWSUBSURFACE->listen_map.notify = Events::listener_mapSubsurface;
    PNEWSUBSURFACE->listen_unmap.notify = Events::listener_unmapSubsurface;
    PNEWSUBSURFACE->listen_destroy.notify = Events::listener_destroySubsurface;

    wl_signal_add(&PSUBSURFACE->events.map, &PNEWSUBSURFACE->listen_map);
    wl_signal_add(&PSUBSURFACE->events.unmap, &PNEWSUBSURFACE->listen_unmap);
    wl_signal_add(&PSUBSURFACE->events.destroy, &PNEWSUBSURFACE->listen_destroy);
}

void Events::listener_mapSubsurface(wl_listener* listener, void* data) {
    SSubsurface* subsurface = wl_container_of(listener, subsurface, listen_map);

    Debug::log(LOG, "Subsurface %x mapped", subsurface->pSubsurface);

    subsurface->pChild = createSubsurfaceNode(subsurface->pParent, subsurface, subsurface->pSubsurface->surface);
}

void Events::listener_unmapSubsurface(wl_listener* listener, void* data) {
    SSubsurface* subsurface = wl_container_of(listener, subsurface, listen_unmap);

    Debug::log(LOG, "Subsurface %x unmapped", subsurface);

    if (subsurface->pChild) {
        const auto PNODE = subsurface->pChild;

        int lx = 0, ly = 0;
        addSurfaceGlobalOffset(PNODE, &lx, &ly);

        wlr_box extents = {0};
        wlr_surface_get_extends(PNODE->pSurface, &extents);

        extents.x += lx;
        extents.y += ly;

        SubsurfaceTree::destroySurfaceTree(subsurface->pChild);
        subsurface->pChild = nullptr;
    }
}

void Events::listener_commitSubsurface(wl_listener* listener, void* data) {
    SSurfaceTreeNode* pNode = wl_container_of(listener, pNode, listen_commit);
}

void Events::listener_destroySubsurface(wl_listener* listener, void* data) {
    SSubsurface* subsurface = wl_container_of(listener, subsurface, listen_destroy);

    Debug::log(LOG, "Subsurface %x destroyed", subsurface);

    subsurface->pParent->childSubsurfaces.remove(*subsurface);
}

void Events::listener_destroySubsurfaceNode(wl_listener* listener, void* data) {
    SSurfaceTreeNode* pNode = wl_container_of(listener, pNode, listen_destroy);

    Debug::log(LOG, "Subsurface Node %x destroyed", pNode);

    for (auto& c : pNode->childSubsurfaces)
        destroySubsurface(&c);

    wl_list_remove(&pNode->listen_newSubsurface.link);
    wl_list_remove(&pNode->listen_commit.link);
    wl_list_remove(&pNode->listen_destroy.link);

    SubsurfaceTree::surfaceTreeNodes.remove(*pNode);
}
