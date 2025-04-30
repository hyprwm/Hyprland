#pragma once

#include "../defines.hpp"
#include <stack>
#include <vector>
#include "../SharedDefs.hpp"
#include "MiscFunctions.hpp"
#include "WLClasses.hpp"
#include <array>

#include <xf86drmMode.h>
#include "time/Timer.hpp"
#include "math/Math.hpp"
#include <optional>
#include "../protocols/types/ColorManagement.hpp"
#include "signal/Signal.hpp"
#include "DamageRing.hpp"
#include <aquamarine/output/Output.hpp>
#include <aquamarine/allocator/Swapchain.hpp>
#include <hyprutils/os/FileDescriptor.hpp>

// Enum for the different types of auto directions, e.g. auto-left, auto-up.
enum eAutoDirs : uint8_t {
    DIR_AUTO_NONE = 0, /* None will be treated as right. */
    DIR_AUTO_UP,
    DIR_AUTO_DOWN,
    DIR_AUTO_LEFT,
    DIR_AUTO_RIGHT
};

enum eCMType : uint8_t {
    CM_AUTO = 0, // subject to change. srgb for 8bpc, wide for 10bpc if supported
    CM_SRGB,     // default, sRGB primaries
    CM_WIDE,     // wide color gamut, BT2020 primaries
    CM_EDID,     // primaries from edid (known to be inaccurate)
    CM_HDR,      // wide color gamut and HDR PQ transfer function
    CM_HDR_EDID, // same as CM_HDR with edid primaries
};

struct SMonitorRule {
    eAutoDirs           autoDir       = DIR_AUTO_NONE;
    std::string         name          = "";
    Vector2D            resolution    = Vector2D(1280, 720);
    Vector2D            offset        = Vector2D(0, 0);
    float               scale         = 1;
    float               refreshRate   = 60; // Hz
    bool                disabled      = false;
    wl_output_transform transform     = WL_OUTPUT_TRANSFORM_NORMAL;
    std::string         mirrorOf      = "";
    bool                enable10bit   = false;
    eCMType             cmType        = CM_SRGB;
    float               sdrSaturation = 1.0f; // SDR -> HDR
    float               sdrBrightness = 1.0f; // SDR -> HDR
    drmModeModeInfo     drmMode       = {};
    std::optional<int>  vrr;
};

class CMonitor;
class CSyncTimeline;
class CEGLSync;

class CMonitorState {
  public:
    CMonitorState(CMonitor* owner);
    ~CMonitorState() = default;

    bool commit();
    bool test();
    bool updateSwapchain();

  private:
    void      ensureBufferPresent();

    CMonitor* m_owner = nullptr;
};

class CMonitor {
  public:
    CMonitor(SP<Aquamarine::IOutput> output);
    ~CMonitor();

    Vector2D                    m_position         = Vector2D(-1, -1); // means unset
    Vector2D                    m_xWaylandPosition = Vector2D(-1, -1); // means unset
    Vector2D                    m_size             = Vector2D(0, 0);
    Vector2D                    m_pixelSize        = Vector2D(0, 0);
    Vector2D                    m_transformedSize  = Vector2D(0, 0);

    MONITORID                   m_id                     = MONITOR_INVALID;
    PHLWORKSPACE                m_activeWorkspace        = nullptr;
    PHLWORKSPACE                m_activeSpecialWorkspace = nullptr;
    float                       m_setScale               = 1; // scale set by cfg
    float                       m_scale                  = 1; // real scale

    std::string                 m_name             = "";
    std::string                 m_description      = "";
    std::string                 m_shortDescription = "";

    Vector2D                    m_reservedTopLeft     = Vector2D(0, 0);
    Vector2D                    m_reservedBottomRight = Vector2D(0, 0);

    drmModeModeInfo             m_customDrmMode = {};

    CMonitorState               m_state;
    CDamageRing                 m_damage;

    SP<Aquamarine::IOutput>     m_output;
    float                       m_refreshRate     = 60; // Hz
    int                         m_forceFullFrames = 0;
    bool                        m_scheduledRecalc = false;
    wl_output_transform         m_transform       = WL_OUTPUT_TRANSFORM_NORMAL;
    float                       m_xwaylandScale   = 1.f;
    Mat3x3                      m_projMatrix;
    std::optional<Vector2D>     m_forceSize;
    SP<Aquamarine::SOutputMode> m_currentMode;
    SP<Aquamarine::CSwapchain>  m_cursorSwapchain;
    uint32_t                    m_drmFormat     = DRM_FORMAT_INVALID;
    uint32_t                    m_prevDrmFormat = DRM_FORMAT_INVALID;

    bool                        m_dpmsStatus       = true;
    bool                        m_vrrActive        = false; // this can be TRUE even if VRR is not active in the case that this display does not support it.
    bool                        m_enabled10bit     = false; // as above, this can be TRUE even if 10 bit failed.
    eCMType                     m_cmType           = CM_SRGB;
    float                       m_sdrSaturation    = 1.0f;
    float                       m_sdrBrightness    = 1.0f;
    bool                        m_createdByUser    = false;
    bool                        m_isUnsafeFallback = false;

    bool                        m_pendingFrame    = false; // if we schedule a frame during rendering, reschedule it after
    bool                        m_renderingActive = false;

    wl_event_source*            m_renderTimer   = nullptr; // for RAT
    bool                        m_ratsScheduled = false;
    CTimer                      m_lastPresentationTimer;

    bool                        m_isBeingLeased = false;

    SMonitorRule                m_activeMonitorRule;

    // explicit sync
    SP<CSyncTimeline>              m_inTimeline;
    Hyprutils::OS::CFileDescriptor m_inFence;
    SP<CEGLSync>                   m_eglSync;
    uint64_t                       m_inTimelinePoint = 0;

    PHLMONITORREF                  m_self;

    // mirroring
    PHLMONITORREF              m_mirrorOf;
    std::vector<PHLMONITORREF> m_mirrors;

    // ctm
    Mat3x3 m_ctm        = Mat3x3::identity();
    bool   m_ctmUpdated = false;

    // for tearing
    PHLWINDOWREF m_solitaryClient;

    // for direct scanout
    PHLWINDOWREF m_lastScanout;
    bool         m_scanoutNeedsCursorUpdate = false;

    struct {
        bool canTear         = false;
        bool nextRenderTorn  = false;
        bool activelyTearing = false;

        bool busy                    = false;
        bool frameScheduledWhileBusy = false;
    } m_tearingState;

    struct {
        CSignal destroy;
        CSignal connect;
        CSignal disconnect;
        CSignal dpmsChanged;
        CSignal modeChanged;
    } m_events;

    std::array<std::vector<PHLLSREF>, 4> m_layerSurfaceLayers;

    // methods
    void                                onConnect(bool noRule);
    void                                onDisconnect(bool destroy = false);
    bool                                applyMonitorRule(SMonitorRule* pMonitorRule, bool force = false);
    void                                addDamage(const pixman_region32_t* rg);
    void                                addDamage(const CRegion& rg);
    void                                addDamage(const CBox& box);
    bool                                shouldSkipScheduleFrameOnMouseEvent();
    void                                setMirror(const std::string&);
    bool                                isMirror();
    bool                                matchesStaticSelector(const std::string& selector) const;
    float                               getDefaultScale();
    void                                changeWorkspace(const PHLWORKSPACE& pWorkspace, bool internal = false, bool noMouseMove = false, bool noFocus = false);
    void                                changeWorkspace(const WORKSPACEID& id, bool internal = false, bool noMouseMove = false, bool noFocus = false);
    void                                setSpecialWorkspace(const PHLWORKSPACE& pWorkspace);
    void                                setSpecialWorkspace(const WORKSPACEID& id);
    void                                moveTo(const Vector2D& pos);
    Vector2D                            middle();
    void                                updateMatrix();
    WORKSPACEID                         activeWorkspaceID();
    WORKSPACEID                         activeSpecialWorkspaceID();
    CBox                                logicalBox();
    void                                scheduleDone();
    bool                                attemptDirectScanout();
    void                                setCTM(const Mat3x3& ctm);
    void                                onCursorMovedOnMonitor();

    void                                debugLastPresentation(const std::string& message);
    void                                onMonitorFrame();

    bool                                m_enabled             = false;
    bool                                m_renderingInitPassed = false;
    WP<CWindow>                         m_previousFSWindow;
    NColorManagement::SImageDescription m_imageDescription;

    // For the list lookup

    bool operator==(const CMonitor& rhs) {
        return m_position == rhs.m_position && m_size == rhs.m_size && m_name == rhs.m_name;
    }

    // workspace previous per monitor functionality
    SWorkspaceIDName getPrevWorkspaceIDName(const WORKSPACEID id);
    void             addPrevWorkspaceID(const WORKSPACEID id);

  private:
    void                    setupDefaultWS(const SMonitorRule&);
    WORKSPACEID             findAvailableDefaultWS();

    bool                    m_doneScheduled = false;
    std::stack<WORKSPACEID> m_prevWorkSpaces;

    struct {
        CHyprSignalListener frame;
        CHyprSignalListener destroy;
        CHyprSignalListener state;
        CHyprSignalListener needsFrame;
        CHyprSignalListener presented;
        CHyprSignalListener commit;
    } m_listeners;
};
