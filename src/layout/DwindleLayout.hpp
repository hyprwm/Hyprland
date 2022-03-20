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

    int             workspaceID = -1;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) {
        return pWindow == rhs.pWindow && workspaceID == rhs.workspaceID && position == rhs.position && size == rhs.size && pParent == rhs.pParent && children[0] == rhs.children[0] && children[1] == rhs.children[1];
    }

    void            recalcSizePosRecursive();
    CHyprDwindleLayout* layout = nullptr;
};

class CHyprDwindleLayout : public IHyprLayout {
public:
    virtual void        onWindowCreated(CWindow*);
    virtual void        onWindowRemoved(CWindow*);
    virtual void        recalculateMonitor(const int&);
    virtual void        changeWindowFloatingMode(CWindow*);
    virtual void        onBeginDragWindow();
    virtual void        onMouseMove(const Vector2D&);
    virtual void        onWindowCreatedFloating(CWindow*);

   private:

    std::list<SDwindleNodeData>     m_lDwindleNodesData;

    Vector2D                        m_vBeginDragXY;
    Vector2D                        m_vBeginDragPositionXY;
    Vector2D                        m_vBeginDragSizeXY;

    int                 getNodesOnWorkspace(const int&);
    void                applyNodeDataToWindow(SDwindleNodeData*);
    SDwindleNodeData*   getNodeFromWindow(CWindow*);
    SDwindleNodeData*   getFirstNodeOnWorkspace(const int&);
    SDwindleNodeData*   getMasterNodeOnWorkspace(const int&);

    friend struct SDwindleNodeData;
};