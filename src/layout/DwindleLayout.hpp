#pragma once

#include "IHyprLayout.hpp"
#include "../desktop/DesktopTypes.hpp"

#include <list>
#include <deque>
#include <array>
#include <optional>
#include <format>

class CHyprDwindleLayout;
enum eFullscreenMode : int8_t;

struct SDwindleNodeData {
    SDwindleNodeData*                pParent = nullptr;
    bool                             isNode  = false;

    PHLWINDOWREF                     pWindow;

    std::array<SDwindleNodeData*, 2> children = {nullptr, nullptr};

    bool                             splitTop = false; // for preserve_split

    CBox                             box = {0};

    int                              workspaceID = -1;

    float                            splitRatio = 1.f;

    bool                             valid = true;

    bool                             ignoreFullscreenChecks = false;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) const {
        return pWindow.lock() == rhs.pWindow.lock() && workspaceID == rhs.workspaceID && box == rhs.box && pParent == rhs.pParent && children[0] == rhs.children[0] &&
            children[1] == rhs.children[1];
    }

    void                recalcSizePosRecursive(bool force = false, bool horizontalOverride = false, bool verticalOverride = false);
    void                getAllChildrenRecursive(std::deque<SDwindleNodeData*>*);
    CHyprDwindleLayout* layout = nullptr;
};

class CHyprDwindleLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(PHLWINDOW, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(PHLWINDOW);
    virtual bool                     isWindowTiled(PHLWINDOW);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(PHLWINDOW);
    virtual void                     onBeginDragWindow();
    virtual void                     resizeActiveWindow(const Vector2D&, eRectCorner corner = CORNER_NONE, PHLWINDOW pWindow = nullptr);
    virtual void                     fullscreenRequestForWindow(PHLWINDOW, eFullscreenMode, bool);
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
    std::list<SDwindleNodeData> m_lDwindleNodesData;

    struct {
        bool started = false;
        bool pseudo  = false;
        int  xExtent = 0;
        int  yExtent = 0;
    } m_PseudoDragFlags;

    std::optional<Vector2D> m_vOverrideFocalPoint; // for onWindowCreatedTiling.

    int                     getNodesOnWorkspace(const int&);
    void                    applyNodeDataToWindow(SDwindleNodeData*, bool force = false);
    void                    calculateWorkspace(const PHLWORKSPACE& pWorkspace);
    SDwindleNodeData*       getNodeFromWindow(PHLWINDOW);
    SDwindleNodeData*       getFirstNodeOnWorkspace(const int&);
    SDwindleNodeData*       getClosestNodeOnWorkspace(const int&, const Vector2D&);
    SDwindleNodeData*       getMasterNodeOnWorkspace(const int&);

    void                    toggleSplit(PHLWINDOW);
    void                    swapSplit(PHLWINDOW);

    eDirection              overrideDirection = DIRECTION_DEFAULT;

    friend struct SDwindleNodeData;
};

template <typename CharT>
struct std::formatter<SDwindleNodeData*, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const SDwindleNodeData* const& node, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!node)
            return std::format_to(out, "[Node nullptr]");
        std::format_to(out, "[Node {:x}: workspace: {}, pos: {:j2}, size: {:j2}", (uintptr_t)node, node->workspaceID, node->box.pos(), node->box.size());
        if (!node->isNode && !node->pWindow.expired())
            std::format_to(out, ", window: {:x}", node->pWindow.lock());
        return std::format_to(out, "]");
    }
};
