#pragma once

#include <sys/resource.h>

#include <ranges>

#include "helpers/math/Direction.hpp"
#include "managers/XWaylandManager.hpp"
#include "managers/KeybindManager.hpp"
#include "managers/SessionLockManager.hpp"
#include "desktop/view/Window.hpp"
#include "desktop/state/FadingOutState.hpp"
#include "desktop/state/LayerState.hpp"
#include "desktop/state/OtherViewState.hpp"
#include "desktop/state/ViewState.hpp"
#include "desktop/state/WindowState.hpp"
#include "helpers/cm/ColorManagement.hpp"

#include <aquamarine/backend/Backend.hpp>
#include <aquamarine/output/Output.hpp>

class CWLSurfaceResource;

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

    bool                     m_initialized = false;
    bool                     m_safeMode    = false;
    SP<Aquamarine::CBackend> m_aqBackend;

    std::string              m_hyprTempDataRoot = "";

    std::string              m_wlDisplaySocket   = "";
    std::string              m_instanceSignature = "";
    std::string              m_instancePath      = "";
    std::string              m_currentSplash     = "error";

    void                     initServer(std::string socketName, int socketFd);
    void                     startCompositor();
    void                     stopCompositor();
    void                     cleanup();
    void                     bumpNofile();
    void                     restoreNofile();
    bool                     setWatchdogFd(int fd);

    bool                     m_sessionActive          = true;
    bool                     m_dpmsStateOn            = true;
    bool                     m_isShuttingDown         = false;
    bool                     m_finalRequests          = false;
    bool                     m_desktopEnvSet          = false;
    bool                     m_wantsXwayland          = true;
    bool                     m_onlyConfigVerification = false;

    // ------------------------------------------------- //

    bool                isWindowActive(PHLWINDOW);
    void                changeWindowZOrder(PHLWINDOW, bool);
    PHLWINDOW           getWindowInDirection(PHLWINDOW, Math::eDirection);
    PHLWINDOW           getWindowInDirection(const CBox& box, PHLWORKSPACE pWorkspace, Math::eDirection dir, bool floatingPreference, PHLWINDOW ignoreWindow = nullptr,
                                             bool useVectorAngles = false);
    PHLWINDOW           getWindowCycle(PHLWINDOW cur, bool focusableOnly = false, std::optional<bool> floating = std::nullopt, bool visible = false, bool prev = false,
                                       bool allowFullscreenBlocked = false);
    PHLWINDOW           getWindowCycleHist(PHLWINDOWREF cur, bool focusableOnly = false, std::optional<bool> floating = std::nullopt, bool visible = false, bool next = false,
                                           bool allowFullscreenBlocked = false);
    bool                isPointOnAnyMonitor(const Vector2D&);
    bool                isPointOnReservedArea(const Vector2D& point, const PHLMONITOR monitor = nullptr);
    std::optional<CBox> calculateX11WorkArea();
    void                updateAllWindowsAnimatedDecorationValues();
    void                moveWorkspaceToMonitor(PHLWORKSPACE, PHLMONITOR, bool noWarpCursor = false);
    void                swapActiveWorkspaces(PHLMONITOR, PHLMONITOR);
    void                setWindowFullscreenInternal(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                setWindowFullscreenClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                setWindowFullscreenState(const PHLWINDOW PWINDOW, const Desktop::View::SFullscreenState state);
    void                changeWindowFullscreenModeClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE, const bool ON);
    PHLWINDOW           getX11Parent(PHLWINDOW);
    void                warpCursorTo(const Vector2D&, bool force = false);
    Vector2D            parseWindowVectorArgsRelative(const std::string&, const Vector2D&);
    void                performUserChecks();
    void                moveWindowToWorkspaceSafe(PHLWINDOW pWindow, PHLWORKSPACE pWorkspace);
    void                setPreferredScaleForSurface(SP<CWLSurfaceResource> pSurface, double scale);
    void                setPreferredTransformForSurface(SP<CWLSurfaceResource> pSurface, wl_output_transform transform);
    void                updateSuspendedStates();
    std::optional<unsigned int>         getVTNr();
    bool                                isVRRActiveOnAnyMonitor() const;

    NColorManagement::PImageDescription getPreferredImageDescription();
    NColorManagement::PImageDescription getHDRImageDescription();
    bool                                shouldChangePreferredImageDescription();

    bool                                supportsDrmSyncobjTimeline() const;
    std::string                         m_explicitConfigPath;

  private:
    void                           initAllSignals();
    void                           removeAllSignals();
    void                           cleanEnvironment();
    void                           setRandomSplash();
    void                           initManagers(eManagersInitStage stage);
    void                           createLockFile();
    void                           removeLockFile();
    void                           setMallocThreshold();
    void                           openSafeModeBox();

    uint64_t                       m_hyprlandPID    = 0;
    wl_event_source*               m_critSigSource  = nullptr;
    rlimit                         m_originalNofile = {};
    Hyprutils::OS::CFileDescriptor m_watchdogWriteFd;
};

inline UP<CCompositor> g_pCompositor;
