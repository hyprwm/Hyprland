#pragma once

#include "../defines.hpp"
#include <stack>
#include <vector>
#include "../SharedDefs.hpp"
#include "MiscFunctions.hpp"
#include "WLClasses.hpp"
#include <vector>
#include <array>
#include <memory>
#include <xf86drmMode.h>
#include "Timer.hpp"
#include "math/Math.hpp"
#include <optional>
#include "signal/Signal.hpp"
#include "DamageRing.hpp"
#include <aquamarine/output/Output.hpp>
#include <aquamarine/allocator/Swapchain.hpp>

// Enum for the different types of auto directions, e.g. auto-left, auto-up.
enum eAutoDirs : uint8_t {
    DIR_AUTO_NONE = 0, /* None will be treated as right. */
    DIR_AUTO_UP,
    DIR_AUTO_DOWN,
    DIR_AUTO_LEFT,
    DIR_AUTO_RIGHT
};

struct SMonitorRule {
    eAutoDirs           autoDir     = DIR_AUTO_NONE;
    std::string         name        = "";
    Vector2D            resolution  = Vector2D(1280, 720);
    Vector2D            offset      = Vector2D(0, 0);
    float               scale       = 1;
    float               refreshRate = 60;
    bool                disabled    = false;
    wl_output_transform transform   = WL_OUTPUT_TRANSFORM_NORMAL;
    std::string         mirrorOf    = "";
    bool                enable10bit = false;
    drmModeModeInfo     drmMode     = {};
    std::optional<int>  vrr;
};

class CMonitor;
class CSyncTimeline;

class CMonitorState {
  public:
    CMonitorState(CMonitor* owner);
    ~CMonitorState();

    bool commit();
    bool test();
    bool updateSwapchain();

  private:
    void      ensureBufferPresent();

    CMonitor* m_pOwner = nullptr;
};

class CMonitor {
  public:
    CMonitor(SP<Aquamarine::IOutput> output);
    ~CMonitor();

    Vector2D                    vecPosition         = Vector2D(-1, -1); // means unset
    Vector2D                    vecXWaylandPosition = Vector2D(-1, -1); // means unset
    Vector2D                    vecSize             = Vector2D(0, 0);
    Vector2D                    vecPixelSize        = Vector2D(0, 0);
    Vector2D                    vecTransformedSize  = Vector2D(0, 0);

    bool                        primary = false;

    MONITORID                   ID                     = MONITOR_INVALID;
    PHLWORKSPACE                activeWorkspace        = nullptr;
    PHLWORKSPACE                activeSpecialWorkspace = nullptr;
    float                       setScale               = 1; // scale set by cfg
    float                       scale                  = 1; // real scale

    std::string                 szName             = "";
    std::string                 szDescription      = "";
    std::string                 szShortDescription = "";

    Vector2D                    vecReservedTopLeft     = Vector2D(0, 0);
    Vector2D                    vecReservedBottomRight = Vector2D(0, 0);

    drmModeModeInfo             customDrmMode = {};

    CMonitorState               state;
    CDamageRing                 damage;

    SP<Aquamarine::IOutput>     output;
    float                       refreshRate     = 60;
    int                         forceFullFrames = 0;
    bool                        scheduledRecalc = false;
    wl_output_transform         transform       = WL_OUTPUT_TRANSFORM_NORMAL;
    float                       xwaylandScale   = 1.f;
    Mat3x3                      projMatrix;
    std::optional<Vector2D>     forceSize;
    SP<Aquamarine::SOutputMode> currentMode;
    SP<Aquamarine::CSwapchain>  cursorSwapchain;
    uint32_t                    drmFormat     = DRM_FORMAT_INVALID;
    uint32_t                    prevDrmFormat = DRM_FORMAT_INVALID;

    bool                        dpmsStatus       = true;
    bool                        vrrActive        = false; // this can be TRUE even if VRR is not active in the case that this display does not support it.
    bool                        enabled10bit     = false; // as above, this can be TRUE even if 10 bit failed.
    bool                        createdByUser    = false;
    bool                        isUnsafeFallback = false;

    bool                        pendingFrame    = false; // if we schedule a frame during rendering, reschedule it after
    bool                        renderingActive = false;

    wl_event_source*            renderTimer  = nullptr; // for RAT
    bool                        RATScheduled = false;
    CTimer                      lastPresentationTimer;

    bool                        isBeingLeased = false;

    SMonitorRule                activeMonitorRule;

    // explicit sync
    SP<CSyncTimeline> inTimeline;
    SP<CSyncTimeline> outTimeline;
    uint64_t          commitSeq = 0;

    PHLMONITORREF     self;

    // mirroring
    PHLMONITORREF              pMirrorOf;
    std::vector<PHLMONITORREF> mirrors;

    // ctm
    Mat3x3 ctm        = Mat3x3::identity();
    bool   ctmUpdated = false;

    // for tearing
    PHLWINDOWREF solitaryClient;

    // for direct scanout
    PHLWINDOWREF lastScanout;

    struct {
        bool canTear         = false;
        bool nextRenderTorn  = false;
        bool activelyTearing = false;

        bool busy                    = false;
        bool frameScheduledWhileBusy = false;
    } tearingState;

    struct {
        CSignal destroy;
        CSignal connect;
        CSignal disconnect;
        CSignal dpmsChanged;
        CSignal modeChanged;
    } events;

    std::array<std::vector<PHLLSREF>, 4> m_aLayerSurfaceLayers;

    // methods
    void        onConnect(bool noRule);
    void        onDisconnect(bool destroy = false);
    bool        applyMonitorRule(SMonitorRule* pMonitorRule, bool force = false);
    void        addDamage(const pixman_region32_t* rg);
    void        addDamage(const CRegion* rg);
    void        addDamage(const CBox* box);
    bool        shouldSkipScheduleFrameOnMouseEvent();
    void        setMirror(const std::string&);
    bool        isMirror();
    bool        matchesStaticSelector(const std::string& selector) const;
    float       getDefaultScale();
    void        changeWorkspace(const PHLWORKSPACE& pWorkspace, bool internal = false, bool noMouseMove = false, bool noFocus = false);
    void        changeWorkspace(const WORKSPACEID& id, bool internal = false, bool noMouseMove = false, bool noFocus = false);
    void        setSpecialWorkspace(const PHLWORKSPACE& pWorkspace);
    void        setSpecialWorkspace(const WORKSPACEID& id);
    void        moveTo(const Vector2D& pos);
    Vector2D    middle();
    void        updateMatrix();
    WORKSPACEID activeWorkspaceID();
    WORKSPACEID activeSpecialWorkspaceID();
    CBox        logicalBox();
    void        scheduleDone();
    bool        attemptDirectScanout();
    void        setCTM(const Mat3x3& ctm);

    void        debugLastPresentation(const std::string& message);
    void        onMonitorFrame();

    bool        m_bEnabled             = false;
    bool        m_bRenderingInitPassed = false;
    WP<CWindow> m_previousFSWindow;

    // For the list lookup

    bool operator==(const CMonitor& rhs) {
        return vecPosition == rhs.vecPosition && vecSize == rhs.vecSize && szName == rhs.szName;
    }

    // workspace previous per monitor functionality
    SWorkspaceIDName getPrevWorkspaceIDName(const WORKSPACEID id);
    void             addPrevWorkspaceID(const WORKSPACEID id);

  private:
    void                    setupDefaultWS(const SMonitorRule&);
    WORKSPACEID             findAvailableDefaultWS();

    bool                    doneScheduled = false;
    std::stack<WORKSPACEID> prevWorkSpaces;

    struct {
        CHyprSignalListener frame;
        CHyprSignalListener destroy;
        CHyprSignalListener state;
        CHyprSignalListener needsFrame;
        CHyprSignalListener presented;
        CHyprSignalListener commit;
    } listeners;
};
