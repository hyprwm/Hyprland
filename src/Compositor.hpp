#pragma once

#include <sys/resource.h>

#include <ranges>

#include "helpers/math/Direction.hpp"
#include "managers/XWaylandManager.hpp"
#include "managers/KeybindManager.hpp"
#include "managers/SessionLockManager.hpp"
#include "desktop/view/Window.hpp"
#include "helpers/cm/ColorManagement.hpp"
#include "config/shared/workspace/WorkspaceRule.hpp"

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
    bool                                         m_safeMode    = false;
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
    std::vector<SP<Desktop::View::IView>>        m_otherViews;

    std::unordered_map<std::string, MONITORID>   m_monitorIDMap;
    std::unordered_map<std::string, WORKSPACEID> m_seenMonitorWorkspaceMap; // map of seen monitor names to workspace IDs

    void                                         initServer(std::string socketName, int socketFd);
    void                                         startCompositor();
    void                                         stopCompositor();
    void                                         cleanup();
    void                                         bumpNofile();
    void                                         restoreNofile();
    bool                                         setWatchdogFd(int fd);

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

    [[nodiscard]] PHLMONITOR             getMonitorFromID(const MONITORID&);
    [[nodiscard]] PHLMONITOR             getMonitorFromName(const std::string&);
    [[nodiscard]] PHLMONITOR             getMonitorFromDesc(const std::string&);
    [[nodiscard]] PHLMONITOR             getMonitorFromCursor();
    [[nodiscard]] PHLMONITOR             getMonitorFromVector(const Vector2D&);
    void                                 removeWindowFromVectorSafe(PHLWINDOW);
    [[nodiscard]] bool                   monitorExists(PHLMONITOR);
    [[nodiscard]] PHLWINDOW              vectorToWindowUnified(const Vector2D&, uint16_t properties, PHLWINDOW pIgnoreWindow = nullptr);
    [[nodiscard]] SP<CWLSurfaceResource> vectorToLayerSurface(const Vector2D&, std::vector<PHLLSREF>*, Vector2D*, PHLLS*, bool aboveLockscreen = false);
    [[nodiscard]] SP<CWLSurfaceResource> vectorToLayerPopupSurface(const Vector2D&, PHLMONITOR monitor, Vector2D*, PHLLS*);
    [[nodiscard]] SP<CWLSurfaceResource> vectorWindowToSurface(const Vector2D&, PHLWINDOW, Vector2D& sl);
    [[nodiscard]] Vector2D               vectorToSurfaceLocal(const Vector2D&, PHLWINDOW, SP<CWLSurfaceResource>);
    [[nodiscard]] PHLMONITOR             getMonitorFromOutput(SP<Aquamarine::IOutput>);
    [[nodiscard]] PHLMONITOR             getRealMonitorFromOutput(SP<Aquamarine::IOutput>);
    [[nodiscard]] PHLWINDOW              getWindowFromSurface(SP<CWLSurfaceResource>);
    [[nodiscard]] PHLWINDOW              getWindowFromHandle(uint32_t);
    [[nodiscard]] PHLWORKSPACE           getWorkspaceByID(const WORKSPACEID&);
    [[nodiscard]] PHLWORKSPACE           getWorkspaceByName(const std::string&);
    [[nodiscard]] PHLWORKSPACE           getWorkspaceByString(const std::string&);
    [[nodiscard]] PHLWINDOW              getUrgentWindow();
    [[nodiscard]] bool                   isWindowActive(PHLWINDOW);
    void                                 changeWindowZOrder(PHLWINDOW, bool);
    void                                 cleanupFadingOut(const MONITORID& monid);
    [[nodiscard]] PHLWINDOW              getWindowInDirection(PHLWINDOW, Math::eDirection);
    [[nodiscard]] PHLWINDOW   getWindowInDirection(const CBox& box, PHLWORKSPACE pWorkspace, Math::eDirection dir, PHLWINDOW ignoreWindow = nullptr, bool useVectorAngles = false);
    [[nodiscard]] PHLWINDOW   getWindowCycle(PHLWINDOW cur, bool focusableOnly = false, std::optional<bool> floating = std::nullopt, bool visible = false, bool prev = false,
                                             bool allowFullscreenBlocked = false);
    [[nodiscard]] PHLWINDOW   getWindowCycleHist(PHLWINDOWREF cur, bool focusableOnly = false, std::optional<bool> floating = std::nullopt, bool visible = false, bool next = false,
                                                 bool allowFullscreenBlocked = false);
    [[nodiscard]] WORKSPACEID getNextAvailableNamedWorkspace();
    [[nodiscard]] bool        isPointOnAnyMonitor(const Vector2D&);
    [[nodiscard]] bool        isPointOnReservedArea(const Vector2D& point, const PHLMONITOR monitor = nullptr);
    [[nodiscard]] std::optional<CBox> calculateX11WorkArea();
    [[nodiscard]] PHLMONITOR          getMonitorInDirection(Math::eDirection);
    [[nodiscard]] PHLMONITOR          getMonitorInDirection(PHLMONITOR, Math::eDirection);
    void                              updateAllWindowsAnimatedDecorationValues();
    [[nodiscard]] MONITORID           getNextAvailableMonitorID(std::string const& name);
    void                              moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false);
    void                              swapActiveWorkspaces(PHLMONITOR, PHLMONITOR);
    [[nodiscard]] PHLMONITOR          getMonitorFromString(const std::string&);
    [[nodiscard]] bool                workspaceIDOutOfBounds(const WORKSPACEID&);
    void                              setWindowFullscreenInternal(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                              setWindowFullscreenClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                              setWindowFullscreenState(const PHLWINDOW PWINDOW, const Desktop::View::SFullscreenState state);
    void                              changeWindowFullscreenModeClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE, const bool ON);
    [[nodiscard]] PHLWINDOW           getX11Parent(PHLWINDOW);
    void                              scheduleFrameForMonitor(PHLMONITOR, Aquamarine::IOutput::scheduleFrameReason reason = Aquamarine::IOutput::AQ_SCHEDULE_CLIENT_UNKNOWN);
    void                              addToFadingOutSafe(PHLLS);
    void                              removeFromFadingOutSafe(PHLLS);
    void                              addToFadingOutSafe(PHLWINDOW);
    [[nodiscard]] PHLWINDOW           getWindowByRegex(const std::string&);
    void                              warpCursorTo(const Vector2D&, bool force = false);
    [[nodiscard]] PHLLS               getLayerSurfaceFromSurface(SP<CWLSurfaceResource>);
    [[nodiscard]] Vector2D            parseWindowVectorArgsRelative(const std::string&, const Vector2D&);
    [[nodiscard]] PHLWORKSPACE        createNewWorkspace(const WORKSPACEID&, const MONITORID&, const std::string& name = "",
                                                         bool isEmpty = true); // will be deleted next frame if left empty and unfocused!
    [[nodiscard]] bool                isWorkspaceSpecial(const WORKSPACEID&);
    [[nodiscard]] WORKSPACEID         getNewSpecialID();
    void                              performUserChecks();
    void                              moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace);
    [[nodiscard]] PHLWINDOW           getForceFocus();
    void                              scheduleMonitorStateRecheck();
    void                              arrangeMonitors();
    void                              checkMonitorOverlaps();
    void                              enterUnsafeState();
    void                              leaveUnsafeState();
    void                              setPreferredScaleForSurface(SP<CWLSurfaceResource> pSurface, double scale);
    void                              setPreferredTransformForSurface(SP<CWLSurfaceResource> pSurface, wl_output_transform transform);
    void                              updateSuspendedStates();
    void                              onNewMonitor(SP<Aquamarine::IOutput> output);
    void                              ensurePersistentWorkspacesPresent(const std::vector<Config::CWorkspaceRule>& rules, PHLWORKSPACE pWorkspace = nullptr);
    void                              ensurePersistentWorkspacesPresent(PHLWORKSPACE pWorkspace = nullptr);
    void                              ensureWorkspacesOnAssignedMonitors();
    [[nodiscard]] std::optional<unsigned int>         getVTNr();
    [[nodiscard]] bool                                isVRRActiveOnAnyMonitor() const;

    [[nodiscard]] NColorManagement::PImageDescription getPreferredImageDescription();
    [[nodiscard]] NColorManagement::PImageDescription getHDRImageDescription();
    bool                                              shouldChangePreferredImageDescription();

    [[nodiscard]] bool                                supportsDrmSyncobjTimeline() const;
    std::string                                       m_explicitConfigPath;

  private:
    void                           initAllSignals();
    void                           removeAllSignals();
    void                           cleanEnvironment();
    void                           setRandomSplash();
    void                           initManagers(eManagersInitStage stage);
    void                           prepareFallbackOutput();
    void                           createLockFile();
    void                           removeLockFile();
    void                           setMallocThreshold();
    void                           openSafeModeBox();

    uint64_t                       m_hyprlandPID    = 0;
    wl_event_source*               m_critSigSource  = nullptr;
    rlimit                         m_originalNofile = {};
    Hyprutils::OS::CFileDescriptor m_watchdogWriteFd;

    std::vector<PHLWORKSPACEREF>   m_workspaces;
};

inline UP<CCompositor> g_pCompositor;
