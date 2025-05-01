#pragma once

#include "IHyprLayout.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../helpers/varlist/VarList.hpp"
#include <vector>
#include <list>
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
    bool         isMaster   = false;
    float        percMaster = 0.5f;

    PHLWINDOWREF pWindow;

    Vector2D     position;
    Vector2D     size;

    float        percSize = 1.f; // size multiplier for resizing children

    WORKSPACEID  workspaceID = WORKSPACE_INVALID;

    bool         ignoreFullscreenChecks = false;

    //
    bool operator==(const SMasterNodeData& rhs) const {
        return pWindow.lock() == rhs.pWindow.lock();
    }
};

struct SMasterWorkspaceData {
    WORKSPACEID  workspaceID = WORKSPACE_INVALID;
    eOrientation orientation = ORIENTATION_LEFT;

    //
    bool operator==(const SMasterWorkspaceData& rhs) const {
        return workspaceID == rhs.workspaceID;
    }
};

class CHyprMasterLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(PHLWINDOW);
    virtual bool                     isWindowTiled(PHLWINDOW);
    virtual void                     recalculateMonitor(const MONITORID&);
    virtual void                     recalculateWindow(PHLWINDOW);
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(PHLWINDOW pWindow, const eFullscreenMode CURRENT_EFFECTIVE_MODE, const eFullscreenMode EFFECTIVE_MODE);
    virtual std::any                 layoutMessage(SLayoutMessageHeader, std::string);
    virtual SWindowRenderLayoutHints requestRenderHints(PHLWINDOW);
    virtual void                     switchWindows(PHLWINDOW, PHLWINDOW);
    virtual void                     moveWindowTo(PHLWINDOW, const std::string& dir, bool silent);
    virtual void                     alterSplitRatio(PHLWINDOW, float, bool);
    virtual std::string              getLayoutName();
    virtual void                     replaceWindowDataWith(PHLWINDOW from, PHLWINDOW to);
    virtual Vector2D                 predictSizeForNewWindowTiled();

    virtual void                     onEnable();
    virtual void                     onDisable();

  private:
    std::list<SMasterNodeData>        m_masterNodesData;
    std::vector<SMasterWorkspaceData> m_masterWorkspacesData;

    bool                              m_forceWarps = false;

    void                              buildOrientationCycleVectorFromVars(std::vector<eOrientation>& cycle, CVarList& vars);
    void                              buildOrientationCycleVectorFromEOperation(std::vector<eOrientation>& cycle);
    void                              runOrientationCycle(SLayoutMessageHeader& header, CVarList* vars, int next);
    eOrientation                      getDynamicOrientation(PHLWORKSPACE);
    int                               getNodesOnWorkspace(const WORKSPACEID&);
    void                              applyNodeDataToWindow(SMasterNodeData*);
    SMasterNodeData*                  getNodeFromWindow(PHLWINDOW);
    SMasterNodeData*                  getMasterNodeOnWorkspace(const WORKSPACEID&);
    SMasterWorkspaceData*             getMasterWorkspaceData(const WORKSPACEID&);
    void                              calculateWorkspace(PHLWORKSPACE);
    PHLWINDOW                         getNextWindow(PHLWINDOW, bool, bool);
    int                               getMastersOnWorkspace(const WORKSPACEID&);

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
        if (!node->pWindow.expired())
            std::format_to(out, ", window: {:x}", node->pWindow.lock());
        return std::format_to(out, "]");
    }
};
