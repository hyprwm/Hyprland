#pragma once

#include "defines.hpp"
#include "events/Events.hpp"
#include "helpers/SubsurfaceTree.hpp"
#include "helpers/AnimatedVariable.hpp"
#include "render/decorations/IHyprWindowDecoration.hpp"
#include <deque>

struct SWindowSpecialRenderData {
    float alpha = 1.f;
    float alphaInactive = -1.f; // -1 means unset

    // set by the layout
    bool rounding = true;
};  

struct SWindowAdditionalConfigData {
    std::string animationStyle = "";
    int rounding = -1; // -1 means no
    bool forceNoBlur = false;
    bool forceOpaque = false;
};

class CWindow {
public:
    CWindow();
    ~CWindow();

    DYNLISTENER(commitWindow);
    DYNLISTENER(mapWindow);
    DYNLISTENER(unmapWindow);
    DYNLISTENER(destroyWindow);
    DYNLISTENER(setTitleWindow);
    DYNLISTENER(setGeometryX11U);
    DYNLISTENER(fullscreenWindow);
    DYNLISTENER(newPopupXDG);
    DYNLISTENER(requestMove);
    DYNLISTENER(requestMinimize);
    DYNLISTENER(requestMaximize);
    DYNLISTENER(requestResize);
    DYNLISTENER(activateX11);
    DYNLISTENER(configureX11);
    DYNLISTENER(toplevelClose);
    DYNLISTENER(toplevelActivate);
    DYNLISTENER(toplevelFullscreen);
   // DYNLISTENER(newSubsurfaceWindow);

    union {
        wlr_xdg_surface* xdg;
        wlr_xwayland_surface* xwayland;
    } m_uSurface;

    // this is the position and size of the "bounding box"
    Vector2D            m_vPosition = Vector2D(0,0);
    Vector2D            m_vSize = Vector2D(0,0);

    // this is the real position and size used to draw the thing
    CAnimatedVariable m_vRealPosition;
    CAnimatedVariable m_vRealSize;

    // for not spamming the protocols
    Vector2D            m_vReportedPosition;
    Vector2D            m_vReportedSize;

    // for restoring floating statuses
    Vector2D            m_vLastFloatingSize;

    // this is used for pseudotiling
    bool                m_bIsPseudotiled = false;
    Vector2D            m_vPseudoSize = Vector2D(0,0);

    uint64_t        m_iTags = 0;
    bool            m_bIsFloating = false;
    bool            m_bDraggingTiled = false; // for dragging around tiled windows
    bool            m_bIsFullscreen = false;
    uint64_t        m_iMonitorID = -1;
    std::string     m_szTitle = "";
    int             m_iWorkspaceID = -1;

    bool            m_bIsMapped = false;

    bool            m_bRequestsFloat = false;

    // This is for fullscreen apps
    bool            m_bCreatedOverFullscreen = false;

    // XWayland stuff
    bool            m_bIsX11 = false;
    bool            m_bMappedX11 = false;
    CWindow*        m_pX11Parent = nullptr;
    uint64_t        m_iX11Type = 0;
    bool            m_bIsModal = false;
    bool            m_bX11DoesntWantBorders = false;
    //

    // For nofocus
    bool            m_bNoFocus = false;
    bool            m_bNoInitialFocus = false;

    SSurfaceTreeNode* m_pSurfaceTree = nullptr;

    // Animated border
    CAnimatedVariable m_cRealBorderColor;

    // Fade in-out
    CAnimatedVariable m_fAlpha;
    bool            m_bFadingOut = false;
    bool            m_bReadyToDelete = false;
    Vector2D        m_vOriginalClosedPos; // these will be used for calculations later on in
    Vector2D        m_vOriginalClosedSize; // drawing the closing animations

    // For hidden windows and stuff
    bool            m_bHidden = false;

    // Foreign Toplevel proto
    wlr_foreign_toplevel_handle_v1* m_phForeignToplevel = nullptr;

    // Window decorations
    std::deque<std::unique_ptr<IHyprWindowDecoration>> m_dWindowDecorations;
    std::vector<IHyprWindowDecoration*> m_vDecosToRemove;

    // Special render data, rules, etc
    SWindowSpecialRenderData m_sSpecialRenderData;
    SWindowAdditionalConfigData m_sAdditionalConfigData;

    // for alpha
    CAnimatedVariable m_fActiveInactiveAlpha;

    // animated shadow color
    CAnimatedVariable m_cRealShadowColor;

    // for toplevel monitor events
    uint64_t          m_iLastToplevelMonitorID = -1;
    uint64_t          m_iLastSurfaceMonitorID = -1;

    // For the list lookup
    bool operator==(const CWindow& rhs) {
        return m_uSurface.xdg == rhs.m_uSurface.xdg && m_uSurface.xwayland == rhs.m_uSurface.xwayland && m_vPosition == rhs.m_vPosition && m_vSize == rhs.m_vSize && m_bFadingOut == rhs.m_bFadingOut;
    }

    // methods
    wlr_box         getFullWindowBoundingBox();
    wlr_box         getWindowIdealBoundingBoxIgnoreReserved();
    void            updateWindowDecos();
    pid_t           getPID();
    IHyprWindowDecoration* getDecorationByType(eDecorationType);
    void            createToplevelHandle();
    void            destroyToplevelHandle();
    void            updateToplevel();
    void            updateSurfaceOutputs();
};
