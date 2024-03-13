#pragma once

#include "IHyprLayout.hpp"
#include "../config/ConfigManager.hpp"
#include <vector>
#include <list>
#include <deque>
#include <any>

enum eFullscreenMode : int8_t;

//orientation determines which side of the screen the master area resides
enum eOrientation : uint8_t {
    ORIENTATION_LEFT = 0,
    ORIENTATION_TOP,
    ORIENTATION_RIGHT,
    ORIENTATION_BOTTOM,
    ORIENTATION_CENTER
};

struct SMasterNodeData {
    bool     isMaster   = false;
    float    percMaster = 0.5f;

    CWindow* pWindow = nullptr;

    Vector2D position;
    Vector2D size;

    float    percSize = 1.f; // size multiplier for resizing children

    int      workspaceID = -1;

    bool     ignoreFullscreenChecks = false;

    //
    bool operator==(const SMasterNodeData& rhs) const {
        return pWindow == rhs.pWindow;
    }
};

struct SMasterWorkspaceData {
    int          workspaceID = -1;
    eOrientation orientation = ORIENTATION_LEFT;

    //
    bool operator==(const SMasterWorkspaceData& rhs) const {
        return workspaceID == rhs.workspaceID;
    }
};

class CHyprMasterLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(CWindow*, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(CWindow*);
    virtual bool                     isWindowTiled(CWindow*);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(CWindow*);
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, CWindow* pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(CWindow*, eFullscreenMode, bool);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(CWindow*);
    virtual void                     switchWindows(CWindow*, CWindow*);
    virtual void                     moveWindowTo(CWindow*, const std::string& dir);
    virtual void                     alterSplitRatio(CWindow*, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(CWindow* from, CWindow* to);
    virtual Vector2D                 predictSizeForNewWindow();

    virtual void                     onEnable();
    virtual void                     onDisable();

  private:
    std::list<SMasterNodeData>        m_lMasterNodesData;
    std::vector<SMasterWorkspaceData> m_lMasterWorkspacesData;

    bool                              m_bForceWarps = false;

    void                              buildOrientationCycleVectorFromVars(std::vector<eOrientation>& cycle, CVarList& vars);
    void                              buildOrientationCycleVectorFromEOperation(std::vector<eOrientation>& cycle);
    void                              runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int next);
    int                               getNodesOnWorkspace(const int&);
    void                              applyNodeDataToWindow(SMasterNodeData*);
    SMasterNodeData*                  getNodeFromWindow(CWindow*);
    SMasterNodeData*                  getMasterNodeOnWorkspace(const int&);
    SMasterWorkspaceData*             getMasterWorkspaceData(const int&);
    void                              calculateWorkspace(const int&);
    CWindow*                          getNextWindow(CWindow*, bool);
    int                               getMastersOnWorkspace(const int&);

    friend struct SMasterNodeData;
    friend struct SMasterWorkspaceData;
};

template <typename CharT>
struct std::formatter<SMasterNodeData*, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const SMasterNodeData* const& node, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!node)
            return std::format_to(out, "[Node nullptr]");
        std::format_to(out, "[Node {:x}: workspace: {}, pos: {:j2}, size: {:j2}", (uintptr_t)node, node->workspaceID, node->position, node->size);
        if (node->isMaster)
            std::format_to(out, ", master");
        if (node->pWindow)
            std::format_to(out, ", window: {:x}", node->pWindow);
        return std::format_to(out, "]");
    }
};
