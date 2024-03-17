#pragma once

#include "IHyprLayout.hpp"

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

    CWindow*                         pWindow = nullptr;

    std::array<SDwindleNodeData*, 2> children = {nullptr, nullptr};

    bool                             splitTop = false; // for preserve_split

    CBox                             box = {0};

    int                              workspaceID = -1;

    float                            splitRatio = 1.f;

    bool                             valid = true;

    bool                             ignoreFullscreenChecks = false;

    // For list lookup
    bool operator==(const SDwindleNodeData& rhs) const {
        return pWindow == rhs.pWindow && workspaceID == rhs.workspaceID && box == rhs.box && pParent == rhs.pParent && children[0] == rhs.children[0] &&
            children[1] == rhs.children[1];
    }

    void                recalcSizePosRecursive(bool force = false, bool horizontalOverride = false, bool verticalOverride = false);
    void                getAllChildrenRecursive(std::deque<SDwindleNodeData*>*);
    CHyprDwindleLayout* layout = nullptr;
};

class CHyprDwindleLayout : public IHyprLayout {
  public:
    virtual void                     onWindowCreatedTiling(CWindow*, eDirection direction = DIRECTION_DEFAULT);
    virtual void                     onWindowRemovedTiling(CWindow*);
    virtual bool                     isWindowTiled(CWindow*);
    virtual void                     recalculateMonitor(const int&);
    virtual void                     recalculateWindow(CWindow*);
    virtual void                     onBeginDragWindow();
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
    std::list<SDwindleNodeData> m_lDwindleNodesData;

    struct {
        bool started = false;
        bool pseudo  = false;
        bool xExtent = false;
        bool yExtent = false;
    } m_PseudoDragFlags;

    std::optional<Vector2D> m_vOverrideFocalPoint; // for onWindowCreatedTiling.

    int                     getNodesOnWorkspace(const int&);
    void                    applyNodeDataToWindow(SDwindleNodeData*, bool force = false);
    void                    calculateWorkspace(const int& ws);
    SDwindleNodeData*       getNodeFromWindow(CWindow*);
    SDwindleNodeData*       getFirstNodeOnWorkspace(const int&);
    SDwindleNodeData*       getClosestNodeOnWorkspace(const int&, const Vector2D&);
    SDwindleNodeData*       getMasterNodeOnWorkspace(const int&);

    void                    toggleSplit(CWindow*);
    void                    swapSplit(CWindow*);

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
        if (!node->isNode && node->pWindow)
            std::format_to(out, ", window: {:x}", node->pWindow);
        return std::format_to(out, "]");
    }
};
