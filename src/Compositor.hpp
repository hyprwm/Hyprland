#pragma once

#include <sys/resource.h>

#include <ranges>

#include "managers/XWaylandManager.hpp"
#include "managers/KeybindManager.hpp"
#include "managers/SessionLockManager.hpp"
#include "desktop/Window.hpp"
#include "protocols/types/ColorManagement.hpp"

#include <aquamarine/backend/Backend.hpp>
#include <aquamarine/output/Output.hpp>

class CWLSurfaceResource;
struct SWorkspaceRule;

enum eManagersInitStage : uint8_t {
    STAGE_PRIORITY = 0,
    STAGE_BASICINIT,
    STAGE_LATE
};

class CCompositor {
  public:
    CCompositor(bool onlyConfig = false);
    ~CCompositor();

    wl_display*    m_wlDisplay   = nullptr;
    wl_event_loop* m_wlEventLoop = nullptr;
    struct {
        int  fd             = -1;
        bool syncobjSupport = false;
    } m_drm;

    struct {
        int  fd             = -1;
        bool syncObjSupport = false;
    } m_drmRenderNode;

    bool                                         m_initialized = false;
    SP<Aquamarine::CBackend>                     m_aqBackend;

    std::string                                  m_hyprTempDataRoot = "";

    std::string                                  m_wlDisplaySocket   = "";
    std::string                                  m_instanceSignature = "";
    std::string                                  m_instancePath      = "";
    std::string                                  m_currentSplash     = "error";

    std::vector<PHLMONITOR>                      m_monitors;
    std::vector<PHLMONITOR>                      m_realMonitors; // for all monitors, even those turned off
    std::vector<PHLWINDOW>                       m_windows;
    std::vector<PHLLS>                           m_layers;
    std::vector<PHLWINDOWREF>                    m_windowsFadingOut;
    std::vector<PHLLSREF>                        m_surfacesFadingOut;

    std::unordered_map<std::string, MONITORID>   m_monitorIDMap;
    std::unordered_map<std::string, WORKSPACEID> m_seenMonitorWorkspaceMap; // map of seen monitor names to workspace IDs

    void                                         initServer(std::string socketName, int socketFd);
    void                                         startCompositor();
    void                                         stopCompositor();
    void                                         cleanup();
    void                                         bumpNofile();
    void                                         restoreNofile();

    WP<CWLSurfaceResource>                       m_lastFocus;
    PHLWINDOWREF                                 m_lastWindow;
    PHLMONITORREF                                m_lastMonitor;

    std::vector<PHLWINDOWREF>                    m_windowFocusHistory; // first element is the most recently focused

    bool                                         m_readyToProcess = false;
    bool                                         m_sessionActive  = true;
    bool                                         m_dpmsStateOn    = true;
    bool                                         m_unsafeState    = false; // unsafe state is when there is no monitors
    PHLMONITORREF                                m_unsafeOutput;           // fallback output for the unsafe state
    bool                                         m_isShuttingDown         = false;
    bool                                         m_finalRequests          = false;
    bool                                         m_desktopEnvSet          = false;
    bool                                         m_wantsXwayland          = true;
    bool                                         m_onlyConfigVerification = false;

    // ------------------------------------------------- //

    auto getWorkspaces() {
        return std::views::filter(m_workspaces, [](const auto& e) { return e; });
    }
    std::vector<PHLWORKSPACE> getWorkspacesCopy();
    void                      registerWorkspace(PHLWORKSPACE w);

    //

    PHLMONITOR             getMonitorFromID(const MONITORID&);
    PHLMONITOR             getMonitorFromName(const std::string&);
    PHLMONITOR             getMonitorFromDesc(const std::string&);
    PHLMONITOR             getMonitorFromCursor();
    PHLMONITOR             getMonitorFromVector(const Vector2D&);
    void                   removeWindowFromVectorSafe(PHLWINDOW);
    void                   focusWindow(PHLWINDOW, SP<CWLSurfaceResource> pSurface = nullptr, bool preserveFocusHistory = false);
    void                   focusSurface(SP<CWLSurfaceResource>, PHLWINDOW pWindowOwner = nullptr);
    bool                   monitorExists(PHLMONITOR);
    PHLWINDOW              vectorToWindowUnified(const Vector2D&, uint8_t properties, PHLWINDOW pIgnoreWindow = nullptr);
    SP<CWLSurfaceResource> vectorToLayerSurface(const Vector2D&, std::vector<PHLLSREF>*, Vector2D*, PHLLS*, bool aboveLockscreen = false);
    SP<CWLSurfaceResource> vectorToLayerPopupSurface(const Vector2D&, PHLMONITOR monitor, Vector2D*, PHLLS*);
    SP<CWLSurfaceResource> vectorWindowToSurface(const Vector2D&, PHLWINDOW, Vector2D& sl);
    Vector2D               vectorToSurfaceLocal(const Vector2D&, PHLWINDOW, SP<CWLSurfaceResource>);
    PHLMONITOR             getMonitorFromOutput(SP<Aquamarine::IOutput>);
    PHLMONITOR             getRealMonitorFromOutput(SP<Aquamarine::IOutput>);
    PHLWINDOW              getWindowFromSurface(SP<CWLSurfaceResource>);
    PHLWINDOW              getWindowFromHandle(uint32_t);
    PHLWORKSPACE           getWorkspaceByID(const WORKSPACEID&);
    PHLWORKSPACE           getWorkspaceByName(const std::string&);
    PHLWORKSPACE           getWorkspaceByString(const std::string&);
    PHLWINDOW              getUrgentWindow();
    bool                   isWindowActive(PHLWINDOW);
    void                   changeWindowZOrder(PHLWINDOW, bool);
    void                   cleanupFadingOut(const MONITORID& monid);
    PHLWINDOW              getWindowInDirection(PHLWINDOW, char);
    PHLWINDOW              getWindowInDirection(const CBox& box, PHLWORKSPACE pWorkspace, char dir, PHLWINDOW ignoreWindow = nullptr, bool useVectorAngles = false);
    PHLWINDOW              getWindowCycle(PHLWINDOW cur, bool focusableOnly = false, std::optional<bool> floating = std::nullopt, bool visible = false, bool prev = false);
    PHLWINDOW              getWindowCycleHist(PHLWINDOWREF cur, bool focusableOnly = false, std::optional<bool> floating = std::nullopt, bool visible = false, bool next = false);
    WORKSPACEID            getNextAvailableNamedWorkspace();
    bool                   isPointOnAnyMonitor(const Vector2D&);
    bool                   isPointOnReservedArea(const Vector2D& point, const PHLMONITOR monitor = nullptr);
    CBox                   calculateX11WorkArea();
    PHLMONITOR             getMonitorInDirection(const char&);
    PHLMONITOR             getMonitorInDirection(PHLMONITOR, const char&);
    void                   updateAllWindowsAnimatedDecorationValues();
    MONITORID              getNextAvailableMonitorID(std::string const& name);
    void                   moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false);
    void                   swapActiveWorkspaces(PHLMONITOR, PHLMONITOR);
    PHLMONITOR             getMonitorFromString(const std::string&);
    bool                   workspaceIDOutOfBounds(const WORKSPACEID&);
    void                   setWindowFullscreenInternal(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                   setWindowFullscreenClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                   setWindowFullscreenState(const PHLWINDOW PWINDOW, const SFullscreenState state);
    void                   changeWindowFullscreenModeClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE, const bool ON);
    PHLWINDOW              getX11Parent(PHLWINDOW);
    void                   scheduleFrameForMonitor(PHLMONITOR, Aquamarine::IOutput::scheduleFrameReason reason = Aquamarine::IOutput::AQ_SCHEDULE_CLIENT_UNKNOWN);
    void                   addToFadingOutSafe(PHLLS);
    void                   removeFromFadingOutSafe(PHLLS);
    void                   addToFadingOutSafe(PHLWINDOW);
    PHLWINDOW              getWindowByRegex(const std::string&);
    void                   warpCursorTo(const Vector2D&, bool force = false);
    PHLLS                  getLayerSurfaceFromSurface(SP<CWLSurfaceResource>);
    void                   closeWindow(PHLWINDOW);
    Vector2D               parseWindowVectorArgsRelative(const std::string&, const Vector2D&);
    [[nodiscard]] PHLWORKSPACE          createNewWorkspace(const WORKSPACEID&, const MONITORID&, const std::string& name = "",
                                                           bool isEmpty = true); // will be deleted next frame if left empty and unfocused!
    void                                setActiveMonitor(PHLMONITOR);
    bool                                isWorkspaceSpecial(const WORKSPACEID&);
    WORKSPACEID                         getNewSpecialID();
    void                                performUserChecks();
    void                                moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace);
    PHLWINDOW                           getForceFocus();
    void                                scheduleMonitorStateRecheck();
    void                                arrangeMonitors();
    void                                checkMonitorOverlaps();
    void                                enterUnsafeState();
    void                                leaveUnsafeState();
    void                                setPreferredScaleForSurface(SP<CWLSurfaceResource> pSurface, double scale);
    void                                setPreferredTransformForSurface(SP<CWLSurfaceResource> pSurface, wl_output_transform transform);
    void                                updateSuspendedStates();
    void                                onNewMonitor(SP<Aquamarine::IOutput> output);
    void                                ensurePersistentWorkspacesPresent(const std::vector<SWorkspaceRule>& rules, PHLWORKSPACE pWorkspace = nullptr);
    std::optional<unsigned int>         getVTNr();

    NColorManagement::SImageDescription getPreferredImageDescription();
    bool                                shouldChangePreferredImageDescription();

    bool                                supportsDrmSyncobjTimeline() const;
    std::string                         m_explicitConfigPath;

  private:
    void                         initAllSignals();
    void                         removeAllSignals();
    void                         cleanEnvironment();
    void                         setRandomSplash();
    void                         initManagers(eManagersInitStage stage);
    void                         prepareFallbackOutput();
    void                         createLockFile();
    void                         removeLockFile();
    void                         setMallocThreshold();

    uint64_t                     m_hyprlandPID    = 0;
    wl_event_source*             m_critSigSource  = nullptr;
    rlimit                       m_originalNofile = {};

    std::vector<PHLWORKSPACEREF> m_workspaces;
};

inline UP<CCompositor> g_pCompositor;
