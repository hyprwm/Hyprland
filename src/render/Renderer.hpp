#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/Monitor.hpp"
#include "../desktop/LayerSurface.hpp"
#include "OpenGL.hpp"
#include "Renderbuffer.hpp"
#include "../helpers/time/Timer.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/time/Time.hpp"

struct SMonitorRule;
class CWorkspace;
class CWindow;
class CInputPopup;
class IHLBuffer;
class CEventLoopTimer;

enum eDamageTrackingModes : int8_t {
    DAMAGE_TRACKING_INVALID = -1,
    DAMAGE_TRACKING_NONE    = 0,
    DAMAGE_TRACKING_MONITOR,
    DAMAGE_TRACKING_FULL,
};

enum eRenderPassMode : uint8_t {
    RENDER_PASS_ALL = 0,
    RENDER_PASS_MAIN,
    RENDER_PASS_POPUP
};

enum eRenderMode : uint8_t {
    RENDER_MODE_NORMAL              = 0,
    RENDER_MODE_FULL_FAKE           = 1,
    RENDER_MODE_TO_BUFFER           = 2,
    RENDER_MODE_TO_BUFFER_READ_ONLY = 3,
};

class CToplevelExportProtocolManager;
class CInputManager;
struct SSessionLockSurface;

struct SExplicitSyncSettings {
    bool explicitEnabled = false, explicitKMSEnabled = false;
};

class CHyprRenderer {
  public:
    CHyprRenderer();
    ~CHyprRenderer();

    void renderMonitor(PHLMONITOR pMonitor);
    void arrangeLayersForMonitor(const MONITORID&);
    void damageSurface(SP<CWLSurfaceResource>, double, double, double scale = 1.0);
    void damageWindow(PHLWINDOW, bool forceFull = false);
    void damageBox(const CBox&, bool skipFrameSchedule = false);
    void damageBox(const int& x, const int& y, const int& w, const int& h);
    void damageRegion(const CRegion&);
    void damageMonitor(PHLMONITOR);
    void damageMirrorsWith(PHLMONITOR, const CRegion&);
    bool shouldRenderWindow(PHLWINDOW, PHLMONITOR);
    bool shouldRenderWindow(PHLWINDOW);
    void ensureCursorRenderingMode();
    bool shouldRenderCursor();
    void setCursorHidden(bool hide);
    void calculateUVForSurface(PHLWINDOW, SP<CWLSurfaceResource>, PHLMONITOR pMonitor, bool main = false, const Vector2D& projSize = {}, const Vector2D& projSizeUnscaled = {},
                               bool fixMisalignedFSV1 = false);
    std::tuple<float, float, float> getRenderTimes(PHLMONITOR pMonitor); // avg max min
    void                            renderLockscreen(PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry);
    void                            recheckSolitaryForMonitor(PHLMONITOR pMonitor);
    void                            setCursorSurface(SP<CWLSurface> surf, int hotspotX, int hotspotY, bool force = false);
    void                            setCursorFromName(const std::string& name, bool force = false);
    void                            onRenderbufferDestroy(CRenderbuffer* rb);
    SP<CRenderbuffer>               getCurrentRBO();
    bool                            isNvidia();
    void                            makeEGLCurrent();
    void                            unsetEGL();
    SExplicitSyncSettings           getExplicitSyncSettings(SP<Aquamarine::IOutput> output);
    void                            addWindowToRenderUnfocused(PHLWINDOW window);
    void                            makeWindowSnapshot(PHLWINDOW);
    void                            makeRawWindowSnapshot(PHLWINDOW, CFramebuffer*);
    void                            makeLayerSnapshot(PHLLS);
    void                            renderSnapshot(PHLWINDOW);
    void                            renderSnapshot(PHLLS);

    // if RENDER_MODE_NORMAL, provided damage will be written to.
    // otherwise, it will be the one used.
    bool beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode = RENDER_MODE_NORMAL, SP<IHLBuffer> buffer = {}, CFramebuffer* fb = nullptr, bool simple = false);
    void endRender();

    bool m_bBlockSurfaceFeedback = false;
    bool m_bRenderingSnapshot    = false;
    PHLMONITORREF                       m_pMostHzMonitor;
    bool                                m_bDirectScanoutBlocked = false;

    void                                setSurfaceScanoutMode(SP<CWLSurfaceResource> surface, PHLMONITOR monitor); // nullptr monitor resets
    void                                initiateManualCrash();

    bool                                m_bCrashingInProgress = false;
    float                               m_fCrashingDistort    = 0.5f;
    wl_event_source*                    m_pCrashingLoop       = nullptr;
    wl_event_source*                    m_pCursorTicker       = nullptr;

    CTimer                              m_tRenderTimer;

    std::vector<SP<CWLSurfaceResource>> explicitPresented;

    struct {
        int                           hotspotX = 0;
        int                           hotspotY = 0;
        std::optional<SP<CWLSurface>> surf;
        std::string                   name;
    } m_sLastCursorData;

    CRenderPass m_sRenderPass = {};

  private:
    void arrangeLayerArray(PHLMONITOR, const std::vector<PHLLSREF>&, bool, CBox*);
    void renderWorkspaceWindowsFullscreen(PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&); // renders workspace windows (fullscreen) (tiled, floating, pinned, but no special)
    void renderWorkspaceWindows(PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&);           // renders workspace windows (no fullscreen) (tiled, floating, pinned, but no special)
    void renderWindow(PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, eRenderPassMode, bool ignorePosition = false, bool standalone = false);
    void renderLayer(PHLLS, PHLMONITOR, const Time::steady_tp&, bool popups = false, bool lockscreen = false);
    void renderSessionLockSurface(WP<SSessionLockSurface>, PHLMONITOR, const Time::steady_tp&);
    void renderDragIcon(PHLMONITOR, const Time::steady_tp&);
    void renderIMEPopup(CInputPopup*, PHLMONITOR, const Time::steady_tp&);
    void renderWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry);
    void sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now); // sends frame displayed events but doesn't actually render anything
    void renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const Vector2D& translate = {0, 0}, const float& scale = 1.f);
    void renderSessionLockMissing(PHLMONITOR pMonitor);

    bool commitPendingAndDoExplicitSync(PHLMONITOR pMonitor);

    bool m_bCursorHidden                           = false;
    bool m_bCursorHasSurface                       = false;
    SP<CRenderbuffer>       m_pCurrentRenderbuffer = nullptr;
    SP<Aquamarine::IBuffer> m_pCurrentBuffer       = nullptr;
    eRenderMode             m_eRenderMode          = RENDER_MODE_NORMAL;
    bool                    m_bNvidia              = false;

    struct {
        bool hiddenOnTouch    = false;
        bool hiddenOnTimeout  = false;
        bool hiddenOnKeyboard = false;
    } m_sCursorHiddenConditions;

    SP<CRenderbuffer>              getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt);
    std::vector<SP<CRenderbuffer>> m_vRenderbuffers;
    std::vector<PHLWINDOWREF>      m_vRenderUnfocused;
    SP<CEventLoopTimer>            m_tRenderUnfocusedTimer;

    friend class CHyprOpenGLImpl;
    friend class CToplevelExportFrame;
    friend class CInputManager;
    friend class CPointerManager;
    friend class CMonitor;
};

inline UP<CHyprRenderer> g_pHyprRenderer;
