#pragma once

#include "IHyprLayout.hpp"
#include <list>

class CHyprDwindleLayout;

struct SDwindleNodeData {
    SDwindleNodeData* pParent = nullptr;
    bool            isNode = false;

    CWindow*        pWindow = nullptr;

    std::array<SDwindleNodeData*, 2> children = { nullptr, nullptr };

    Vector2D        position;
    Vector2D        size;

    int             monitorID = -1;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) {
        return pWindow == rhs.pWindow && monitorID == rhs.monitorID && position == rhs.position && size == rhs.size && pParent == rhs.pParent && children[0] == rhs.children[0] && children[1] == rhs.children[1];
    }

    void            recalcSizePosRecursive();
    CHyprDwindleLayout* layout = nullptr;
};

class CHyprDwindleLayout : public IHyprLayout {
public:
    virtual void        onWindowCreated(CWindow*);
    virtual void        onWindowRemoved(CWindow*);

private:

    std::list<SDwindleNodeData>     m_lDwindleNodesData;

    int                 getNodesOnMonitor(const int&);
    void                applyNodeDataToWindow(SDwindleNodeData*);
    SDwindleNodeData*   getNodeFromWindow(CWindow*);
    SDwindleNodeData*   getFirstNodeOnMonitor(const int&);
    SDwindleNodeData*   getMasterNodeOnMonitor(const int&);

    friend struct SDwindleNodeData;
};