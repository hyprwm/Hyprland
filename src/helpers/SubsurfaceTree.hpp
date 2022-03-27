#pragma once

#include "../defines.hpp"
#include <list>

struct SSubsurface;

typedef void (*applyGlobalOffsetFn)(void *, int *, int *);

struct SSurfaceTreeNode {
    wlr_surface*        pSurface = nullptr;

    DYNMULTILISTENER(newSubsurface);
    DYNMULTILISTENER(commit);
    DYNMULTILISTENER(destroy);

    SSurfaceTreeNode*   pParent = nullptr;
    SSubsurface*        pSubsurface = nullptr;

    std::list<SSubsurface> childSubsurfaces;

    applyGlobalOffsetFn offsetfn;
    void *globalOffsetData;

    bool operator==(const SSurfaceTreeNode& rhs) {
        return pSurface == rhs.pSurface;
    }
};

struct SSubsurface {
    wlr_subsurface*     pSubsurface = nullptr;

    SSurfaceTreeNode*   pParent = nullptr;
    SSurfaceTreeNode*   pChild = nullptr;

    DYNMULTILISTENER(map);
    DYNMULTILISTENER(unmap);
    DYNMULTILISTENER(destroy);

    bool operator==(const SSubsurface& rhs) {
        return pSubsurface == rhs.pSubsurface;
    }
};

namespace SubsurfaceTree {
    SSurfaceTreeNode* createTreeRoot(wlr_surface*, applyGlobalOffsetFn, void*);
    void destroySurfaceTree(SSurfaceTreeNode*);

    inline std::list<SSurfaceTreeNode> surfaceTreeNodes;
};
