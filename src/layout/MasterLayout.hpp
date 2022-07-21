#pragma once

#include "IHyprLayout.hpp"
#include <list>
#include <deque>

enum eFullscreenMode : uint8_t;

struct SMasterNodeData {
    bool isMaster = false;
    float percMaster = 0.5f;

    CWindow* pWindow = nullptr;

    Vector2D position;
    Vector2D size;

    int workspaceID = -1;

    bool operator==(const SMasterNodeData& rhs) {
        return pWindow == rhs.pWindow;
    }
};

class CHyprMasterLayout : public IHyprLayout {
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

    std::list<SMasterNodeData>     m_lMasterNodesData;

    int                 getNodesOnWorkspace(const int&);
    void                applyNodeDataToWindow(SMasterNodeData*);
    SMasterNodeData*    getNodeFromWindow(CWindow*);
    SMasterNodeData*    getMasterNodeOnWorkspace(const int&);
    void                calculateWorkspace(const int&);

    friend struct SMasterNodeData;
};