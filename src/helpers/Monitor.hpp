#pragma once

#include "../defines.hpp"
#include <stack>
#include <vector>
#include "../SharedDefs.hpp"
#include "MiscFunctions.hpp"
#include "WLClasses.hpp"
#include <array>
#include "AnimatedVariable.hpp"

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

class CMonitorFrameScheduler;

// Enum for the different types of auto directions, e.g. auto-left, auto-up.
enum eAutoDirs : uint8_t {
    DIR_AUTO_NONE = 0, /* None will be treated as right. */
    DIR_AUTO_UP,
    DIR_AUTO_DOWN,
    DIR_AUTO_LEFT,
    DIR_AUTO_RIGHT,
    DIR_AUTO_CENTER_UP,
    DIR_AUTO_CENTER_DOWN,
    DIR_AUTO_CENTER_LEFT,
    DIR_AUTO_CENTER_RIGHT
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

    bool                supportsWideColor = false; // false does nothing, true overrides EDID
    bool                supportsHDR       = false; // false does nothing, true overrides EDID
    float               sdrMinLuminance   = 0.2f;  // SDR -> HDR
    int                 sdrMaxLuminance   = 80;    // SDR -> HDR

    // Incorrect values will result in reduced luminance range or incorrect tonemapping. Shouldn't damage the HW. Use with care in case of a faulty monitor firmware.
    float              minLuminance    = -1.0f; // >= 0 overrides EDID
    int                maxLuminance    = -1;    // >= 0 overrides EDID
    int                maxAvgLuminance = -1;    // >= 0 overrides EDID

    drmModeModeInfo    drmMode = {};
    std::optional<int> vrr;
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
    Vector2D                    m_xwaylandPosition = Vector2D(-1, -1); // means unset
    eAutoDirs                   m_autoDir          = DIR_AUTO_NONE;
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
    float                       m_sdrMinLuminance  = 0.2f;
    int                         m_sdrMaxLuminance  = 80;
    bool                        m_createdByUser    = false;
    bool                        m_isUnsafeFallback = false;

    bool                        m_pendingFrame    = false; // if we schedule a frame during rendering, reschedule it after
    bool                        m_renderingActive = false;

    bool                        m_ratsScheduled = false;
    CTimer                      m_lastPresentationTimer;

    bool                        m_isBeingLeased = false;

    SMonitorRule                m_activeMonitorRule;

    // explicit sync
    Hyprutils::OS::CFileDescriptor m_inFence; // TODO: remove when aq uses CFileDescriptor

    PHLMONITORREF                  m_self;

    UP<CMonitorFrameScheduler>     m_frameScheduler;

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

    // for special fade/blur
    PHLANIMVAR<float> m_specialFade;

    // for dpms off anim
    PHLANIMVAR<float> m_dpmsBlackOpacity;
    bool              m_pendingDpmsAnimation        = false;
    int               m_pendingDpmsAnimationCounter = 0;

    PHLANIMVAR<float> m_cursorZoom;

    // for fading in the wallpaper because it doesn't happen instantly (it's loaded async)
    PHLANIMVAR<float> m_backgroundOpacity;

    // for initial zoom anim
    PHLANIMVAR<float> m_zoomAnimProgress;
    CTimer            m_newMonitorAnimTimer;
    int               m_zoomAnimFrameCounter = 0;

    struct {
        bool canTear         = false;
        bool nextRenderTorn  = false;
        bool activelyTearing = false;

        bool busy                    = false;
        bool frameScheduledWhileBusy = false;
    } m_tearingState;

    struct {
        CSignalT<> destroy;
        CSignalT<> connect;
        CSignalT<> disconnect;
        CSignalT<> dpmsChanged;
        CSignalT<> modeChanged;
    } m_events;

    std::array<std::vector<PHLLSREF>, 4> m_layerSurfaceLayers;

    // keep in sync with HyprCtl
    enum eDSBlockReason : uint16_t {
        DS_OK = 0,

        DS_BLOCK_UNKNOWN   = (1 << 0),
        DS_BLOCK_USER      = (1 << 1),
        DS_BLOCK_WINDOWED  = (1 << 2),
        DS_BLOCK_CONTENT   = (1 << 3),
        DS_BLOCK_MIRROR    = (1 << 4),
        DS_BLOCK_RECORD    = (1 << 5),
        DS_BLOCK_SW        = (1 << 6),
        DS_BLOCK_CANDIDATE = (1 << 7),
        DS_BLOCK_SURFACE   = (1 << 8),
        DS_BLOCK_TRANSFORM = (1 << 9),
        DS_BLOCK_DMA       = (1 << 10),
        DS_BLOCK_TEARING   = (1 << 11),
        DS_BLOCK_FAILED    = (1 << 12),
        DS_BLOCK_CM        = (1 << 13),

        DS_CHECKS_COUNT = 14,
    };

    // keep in sync with HyprCtl
    enum eSolitaryCheck : uint32_t {
        SC_OK = 0,

        SC_UNKNOWN      = (1 << 0),
        SC_NOTIFICATION = (1 << 1),
        SC_LOCK         = (1 << 2),
        SC_WORKSPACE    = (1 << 3),
        SC_WINDOWED     = (1 << 4),
        SC_DND          = (1 << 5),
        SC_SPECIAL      = (1 << 6),
        SC_ALPHA        = (1 << 7),
        SC_OFFSET       = (1 << 8),
        SC_CANDIDATE    = (1 << 9),
        SC_OPAQUE       = (1 << 10),
        SC_TRANSFORM    = (1 << 11),
        SC_OVERLAYS     = (1 << 12),
        SC_FLOAT        = (1 << 13),
        SC_WORKSPACES   = (1 << 14),
        SC_SURFACES     = (1 << 15),
        SC_ERRORBAR     = (1 << 16),

        SC_CHECKS_COUNT = 17,
    };

    // keep in sync with HyprCtl
    enum eTearingCheck : uint8_t {
        TC_OK = 0,

        TC_UNKNOWN   = (1 << 0),
        TC_NOT_TORN  = (1 << 1),
        TC_USER      = (1 << 2),
        TC_ZOOM      = (1 << 3),
        TC_SUPPORT   = (1 << 4),
        TC_CANDIDATE = (1 << 5),
        TC_WINDOW    = (1 << 6),

        TC_CHECKS_COUNT = 7,
    };

    // methods
    void        onConnect(bool noRule);
    void        onDisconnect(bool destroy = false);
    void        applyCMType(eCMType cmType);
    bool        applyMonitorRule(SMonitorRule* pMonitorRule, bool force = false);
    void        addDamage(const pixman_region32_t* rg);
    void        addDamage(const CRegion& rg);
    void        addDamage(const CBox& box);
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
    uint32_t    isSolitaryBlocked(bool full = false);
    void        recheckSolitary();
    uint8_t     isTearingBlocked(bool full = false);
    bool        updateTearing();
    uint16_t    isDSBlocked(bool full = false);
    bool        attemptDirectScanout();
    void        setCTM(const Mat3x3& ctm);
    void        onCursorMovedOnMonitor();
    void        setDPMS(bool on);

    void        debugLastPresentation(const std::string& message);

    bool        supportsWideColor();
    bool        supportsHDR();
    float       minLuminance(float defaultValue = 0);
    int         maxLuminance(int defaultValue = 80);
    int         maxAvgLuminance(int defaultValue = 80);

    bool        wantsWideColor();
    bool        wantsHDR();

    bool        inHDR();

    /// Has an active workspace with a real fullscreen window
    bool                                               inFullscreenMode();
    std::optional<NColorManagement::SImageDescription> getFSImageDescription();

    bool                                               needsCM();
    /// Can do CM without shader
    bool                                canNoShaderCM();
    bool                                doesNoShaderCM();

    bool                                m_enabled             = false;
    bool                                m_renderingInitPassed = false;
    WP<CWindow>                         m_previousFSWindow;
    NColorManagement::SImageDescription m_imageDescription;
    bool                                m_noShaderCTM = false; // sets drm CTM, restore needed

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
    void                    commitDPMSState(bool state);

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

    bool  m_supportsWideColor = false;
    bool  m_supportsHDR       = false;
    float m_minLuminance      = -1.0f;
    int   m_maxLuminance      = -1;
    int   m_maxAvgLuminance   = -1;
};
