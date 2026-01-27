#pragma once

#include "IHyprLayout.hpp"
#include "../desktop/DesktopTypes.hpp"

#include <list>
#include <vector>
#include <array>
#include <optional>
#include <format>

class CHyprDwindleLayout;
enum eFullscreenMode : int8_t;

struct SDwindleNodeData {
    WP<SDwindleNodeData>                pParent;
    bool                                isNode = false;

    PHLWINDOWREF                        pWindow;

    std::array<WP<SDwindleNodeData>, 2> children = {};
    WP<SDwindleNodeData>                self;

    bool                                splitTop = false; // for preserve_split

    CBox                                box = {0};

    WORKSPACEID                         workspaceID = WORKSPACE_INVALID;

    float                               splitRatio = 1.f;

    bool                                valid = true;

    bool                                ignoreFullscreenChecks = false;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) const {
        return pWindow.lock() == rhs.pWindow.lock() && workspaceID == rhs.workspaceID && box == rhs.box && pParent == rhs.pParent && children[0] == rhs.children[0] &&
            children[1] == rhs.children[1];
    }

    void                recalcSizePosRecursive(bool force = false, bool horizontalOverride = false, bool verticalOverride = false);
    void                applyRootBox();
    CHyprDwindleLayout* layout = nullptr;
};

class CHyprDwindleLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(PHLWINDOW);
    virtual bool                     isWindowTiled(PHLWINDOW);
    virtual void                     recalculateMonitor(const MONITORID&);
    virtual void                     recalculateWindow(PHLWINDOW);
    virtual void                     onBeginDragWindow();
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
    std::vector<SP<SDwindleNodeData>> m_dwindleNodesData;

    struct {
        bool started = false;
        bool pseudo  = false;
        bool xExtent = false;
        bool yExtent = false;
    } m_pseudoDragFlags;

    std::optional<Vector2D> m_overrideFocalPoint; // for onWindowCreatedTiling.

    int                     getNodesOnWorkspace(const WORKSPACEID&);
    void                    applyNodeDataToWindow(SP<SDwindleNodeData>, bool force = false);
    void                    calculateWorkspace(const PHLWORKSPACE& pWorkspace);
    SP<SDwindleNodeData>    getNodeFromWindow(PHLWINDOW);
    SP<SDwindleNodeData>    getFirstNodeOnWorkspace(const WORKSPACEID&);
    SP<SDwindleNodeData>    getClosestNodeOnWorkspace(const WORKSPACEID&, const Vector2D&);
    SP<SDwindleNodeData>    getMasterNodeOnWorkspace(const WORKSPACEID&);

    void                    toggleSplit(PHLWINDOW);
    void                    swapSplit(PHLWINDOW);
    void                    moveToRoot(PHLWINDOW, bool stable = true);

    eDirection              m_overrideDirection = DIRECTION_DEFAULT;

    friend struct SDwindleNodeData;
};

template <typename CharT>
struct std::formatter<SP<SDwindleNodeData>, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const SP<SDwindleNodeData>& node, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!node)
            return std::format_to(out, "[Node nullptr]");
        std::format_to(out, "[Node {:x}: workspace: {}, pos: {:j2}, size: {:j2}", rc<uintptr_t>(node.get()), node->workspaceID, node->box.pos(), node->box.size());
        if (!node->isNode && !node->pWindow.expired())
            std::format_to(out, ", window: {:x}", node->pWindow.lock());
        return std::format_to(out, "]");
    }
};
