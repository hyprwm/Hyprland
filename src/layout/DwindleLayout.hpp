#pragma once

#include "IHyprLayout.hpp"
#include <list>
#include <deque>

class CHyprDwindleLayout;

struct SDwindleNodeData {
    SDwindleNodeData* pParent = nullptr;
    bool            isNode = false;

    CWindow*        pWindow = nullptr;

    std::array<SDwindleNodeData*, 2> children = { nullptr, nullptr };

    bool            isGroup = false;
    int             groupMemberActive = 0;
    std::deque<SDwindleNodeData*> groupMembers;
    SDwindleNodeData* pGroupParent = nullptr;

    Vector2D        position;
    Vector2D        size;

    int             workspaceID = -1;

    float           splitRatio = 1.f;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) {
        return pWindow == rhs.pWindow && workspaceID == rhs.workspaceID && position == rhs.position && size == rhs.size && pParent == rhs.pParent && children[0] == rhs.children[0] && children[1] == rhs.children[1];
    }

    void            recalcSizePosRecursive();
    void            getAllChildrenRecursive(std::deque<SDwindleNodeData*>*);
    CHyprDwindleLayout* layout = nullptr;
};

class CHyprDwindleLayout : public IHyprLayout {
public:
    virtual void        onWindowCreated(CWindow*);
    virtual void        onWindowRemoved(CWindow*);
    virtual void        recalculateMonitor(const int&);
    virtual void        recalculateWindow(CWindow*);
    virtual void        changeWindowFloatingMode(CWindow*);
    virtual void        onBeginDragWindow();
    virtual void        onEndDragWindow();
    virtual void        onMouseMove(const Vector2D&);
    virtual void        onWindowCreatedFloating(CWindow*);
    virtual void        fullscreenRequestForWindow(CWindow*);
    virtual void        toggleWindowGroup(CWindow*);
    virtual void        switchGroupWindow(CWindow*);
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*);

   private:

    std::list<SDwindleNodeData>     m_lDwindleNodesData;

    Vector2D                        m_vBeginDragXY;
    Vector2D                        m_vLastDragXY;
    Vector2D                        m_vBeginDragPositionXY;
    Vector2D                        m_vBeginDragSizeXY;

    int                 getNodesOnWorkspace(const int&);
    void                applyNodeDataToWindow(SDwindleNodeData*);
    SDwindleNodeData*   getNodeFromWindow(CWindow*);
    SDwindleNodeData*   getFirstNodeOnWorkspace(const int&);
    SDwindleNodeData*   getMasterNodeOnWorkspace(const int&);

    friend struct SDwindleNodeData;
};