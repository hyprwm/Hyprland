#pragma once

#include "IHyprLayout.hpp"
#include <list>
#include <deque>
#include "../render/decorations/CHyprGroupBarDecoration.hpp"

class CHyprDwindleLayout;
enum eFullscreenMode : uint8_t;

class SDwindleNodeData {

private:

    union {
        CWindow* pWindow;
        SDwindleNodeData* activeGroupNode;
    };

    SDwindleNodeData() = default;

public:

    SDwindleNodeData* pParent = nullptr;
    std::array<SDwindleNodeData*, 2> children;

    inline bool isTerminal() { return children[0] == nullptr; }
    inline bool isGroup() { return !isTerminal() && activeGroupNode != nullptr; }
    inline bool isNode() { return !isTerminal() && activeGroupNode == nullptr; }

    static SDwindleNodeData newNode(SDwindleNodeData* parent, std::array<SDwindleNodeData *, 2>&& children);
    static SDwindleNodeData newTerminalNode(SDwindleNodeData* parent, CWindow* window);


    bool            splitTop = false; // for preserve_split

    Vector2D        position;
    Vector2D        size;

    int             workspaceID = -1;

    float           splitRatio = 1.f;

    bool            valid = true;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) {
        //compare only by means of structure info
       return workspaceID == rhs.workspaceID &&  pParent == rhs.pParent && children[0] == rhs.children[0] && children[1] == rhs.children[1];
    }

    void            recalcSizePosRecursive(bool force = false, bool apply=true);
    void            addChildrenRecursive(std::deque<SDwindleNodeData *> &pDeque, bool groupStop=false);
    std::deque<SDwindleNodeData*> getChildrenRecursive(bool groupStop=false);

    CWindow* getWindow();

    SDwindleNodeData* getNextNode(SDwindleNodeData *topNode, bool forward=true, bool cyclic=true);
    SDwindleNodeData* getNextTerminalNode(SDwindleNodeData *topNode, bool forward, bool cyclic);
    SDwindleNodeData* getNextWindowNode(SDwindleNodeData *topNode, bool forward, bool cyclic);

    void ungroup();
    void turnIntoGroup(SDwindleNodeData* activeNode);
    SDwindleNodeData* getOuterGroup();
    SDwindleNodeData* nextInGroup(bool forward= true);

    CHyprDwindleLayout* layout = nullptr;

};

class CHyprDwindleLayout : public IHyprLayout {
public:
    virtual void        onWindowCreatedTiling(CWindow*);
    virtual void        onWindowRemovedTiling(CWindow*);
    virtual bool        isWindowTiled(CWindow*);
    virtual void        recalculateMonitor(const int&);
    virtual void        recalculateWindow(CWindow*);
    virtual void        resizeActiveWindow(const Vector2D&, CWindow* pWindow = nullptr);
    virtual void        fullscreenRequestForWindow(CWindow*, eFullscreenMode, bool);
    virtual std::any    layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*);
    virtual void        switchWindows(CWindow*, CWindow*);
    virtual void        alterSplitRatioBy(CWindow*, float);
    virtual std::string getLayoutName();

    virtual void        onEnable();
    virtual void        onDisable();

private:

    std::list<SDwindleNodeData>     m_lDwindleNodesData;

    int                 getNodesOnWorkspace(const int&);
    void                applyNodeDataToWindow(SDwindleNodeData*, bool force = false);
    SDwindleNodeData *getNodeFromWindow(CWindow *pWindow, bool reportGroups=true);
    SDwindleNodeData*   getFirstNodeOnWorkspace(const int&);
    SDwindleNodeData*   getMasterNodeOnWorkspace(const int&);

    void                toggleWindowGroup(CWindow*);
    void                switchGroupWindow(CWindow*, bool forward);
    void                toggleSplit(CWindow*);
    std::deque<CWindow*> getGroupMembers(CWindow*);

    friend struct SDwindleNodeData;
};