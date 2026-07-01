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

    void                                setWindowFullscreenInternal(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                                setWindowFullscreenClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE);
    void                                setWindowFullscreenState(const PHLWINDOW PWINDOW, const Desktop::View::SFullscreenState state);
    void                                changeWindowFullscreenModeClient(const PHLWINDOW PWINDOW, const eFullscreenMode MODE, const bool ON);
    Vector2D                            parseWindowVectorArgsRelative(const std::string&, const Vector2D&);
    void                                performUserChecks();
    std::optional<unsigned int>         getVTNr();

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
