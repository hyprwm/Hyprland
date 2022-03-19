#pragma once

#include "IHyprLayout.hpp"
#include <list>

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
        return pWindow == rhs.pWindow && monitorID == rhs.monitorID && position == rhs.position && size == rhs.size && pParent == rhs.pParent;
    }

    // TODO: recalcsizepos for dynamic
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
};