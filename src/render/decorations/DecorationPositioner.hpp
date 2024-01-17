#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include "../../helpers/Box.hpp"

class CWindow;
class IHyprWindowDecoration;

enum eDecorationPositioningPolicy {
    DECORATION_POSITION_ABSOLUTE = 0, /* Decoration wants absolute positioning */
    DECORATION_POSITION_STICKY,       /* Decoration is stuck to some edge of a window */
};

enum eDecorationEdges {
    DECORATION_EDGE_TOP    = 1 << 0,
    DECORATION_EDGE_BOTTOM = 1 << 1,
    DECORATION_EDGE_LEFT   = 1 << 2,
    DECORATION_EDGE_RIGHT  = 1 << 3
};

/*
Request the positioner to position a decoration

DECORATION_POSITION_ABSOLUTE:
    - desiredExtents has to contain the extents. Edges has to have the edges used.
    - reserved allowed
DECORATION_POSITION_STICKY:
    - one edge allowed
    - priority allowed
    - desiredExtents contains the desired extents. Any other edge than the one selected is ignored.
    - reserved is allowed
*/
struct SDecorationPositioningInfo {
    eDecorationPositioningPolicy policy   = DECORATION_POSITION_ABSOLUTE;
    uint32_t                     edges    = 0;  // enum eDecorationEdges
    uint32_t                     priority = 10; // priority, decos will be evaluated high -> low
    SWindowDecorationExtents     desiredExtents;
    bool                         reserved = false; // if true, geometry will use reserved area
};

/*
A reply from the positioner. This may be sent multiple times, if anything changes.

DECORATION_POSITION_ABSOLUTE:
    - assignedGeometry is empty
DECORATION_POSITION_STICKY:
    - assignedGeometry is relative to the edge's center point
    - ephemeral is sent
*/
struct SDecorationPositioningReply {
    CBox assignedGeometry;
    bool ephemeral = false; // if true, means it's a result of an animation and will change soon.
};

class CDecorationPositioner {
  public:
    CDecorationPositioner();

    Vector2D getEdgeDefinedPoint(uint32_t edges, CWindow* pWindow);

    // called on resize, or insert/removal of a new deco
    void                     onWindowUpdate(CWindow* pWindow);
    void                     uncacheDecoration(IHyprWindowDecoration* deco);
    SWindowDecorationExtents getWindowDecorationReserved(CWindow* pWindow);
    SWindowDecorationExtents getWindowDecorationExtents(CWindow* pWindow, bool inputOnly = false);
    CBox                     getBoxWithIncludedDecos(CWindow* pWindow);
    void                     repositionDeco(IHyprWindowDecoration* deco);
    CBox                     getWindowDecorationBox(IHyprWindowDecoration* deco);
    void                     forceRecalcFor(CWindow* pWindow);

  private:
    struct SWindowPositioningData {
        CWindow*                    pWindow     = nullptr;
        IHyprWindowDecoration*      pDecoration = nullptr;
        SDecorationPositioningInfo  positioningInfo;
        SDecorationPositioningReply lastReply;
        bool                        needsReposition = true;
    };

    struct SWindowData {
        Vector2D                 lastWindowSize = {};
        SWindowDecorationExtents reserved       = {};
        SWindowDecorationExtents extents        = {};
        bool                     needsRecalc    = false;
    };

    std::unordered_map<CWindow*, SWindowData>            m_mWindowDatas;
    std::vector<std::unique_ptr<SWindowPositioningData>> m_vWindowPositioningDatas;

    SWindowPositioningData*                              getDataFor(IHyprWindowDecoration* pDecoration, CWindow* pWindow);
    void                                                 onWindowUnmap(CWindow* pWindow);
    void                                                 onWindowMap(CWindow* pWindow);
    void                                                 sanitizeDatas();
};

inline std::unique_ptr<CDecorationPositioner> g_pDecorationPositioner;