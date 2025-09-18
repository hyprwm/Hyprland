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

struct SRenderWorkspaceUntilData {
    PHLLS     ls;
    PHLWINDOW w;
};

class CHyprRenderer {
  public:
    CHyprRenderer();
    ~CHyprRenderer();

    void renderMonitor(PHLMONITOR pMonitor, bool commit = true);
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
    void                            setCursorSurface(SP<CWLSurface> surf, int hotspotX, int hotspotY, bool force = false);
    void                            setCursorFromName(const std::string& name, bool force = false);
    void                            onRenderbufferDestroy(CRenderbuffer* rb);
    SP<CRenderbuffer>               getCurrentRBO();
    bool                            isNvidia();
    bool                            isIntel();
    bool                            isSoftware();
    bool                            isMgpu();
    void                            makeEGLCurrent();
    void                            unsetEGL();
    void                            addWindowToRenderUnfocused(PHLWINDOW window);
    void                            makeSnapshot(PHLWINDOW);
    void                            makeSnapshot(PHLLS);
    void                            makeSnapshot(WP<CPopup>);
    void                            renderSnapshot(PHLWINDOW);
    void                            renderSnapshot(PHLLS);
    void                            renderSnapshot(WP<CPopup>);

    // if RENDER_MODE_NORMAL, provided damage will be written to.
    // otherwise, it will be the one used.
    bool beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode = RENDER_MODE_NORMAL, SP<IHLBuffer> buffer = {}, CFramebuffer* fb = nullptr, bool simple = false);
    void endRender(const std::function<void()>& renderingDoneCallback = {});

    bool m_bBlockSurfaceFeedback = false;
    bool m_bRenderingSnapshot    = false;
    PHLMONITORREF                   m_mostHzMonitor;
    bool                            m_directScanoutBlocked = false;

    void                            setSurfaceScanoutMode(SP<CWLSurfaceResource> surface, PHLMONITOR monitor); // nullptr monitor resets
    void                            initiateManualCrash();

    bool                            m_crashingInProgress = false;
    float                           m_crashingDistort    = 0.5f;
    wl_event_source*                m_crashingLoop       = nullptr;
    wl_event_source*                m_cursorTicker       = nullptr;

    std::vector<CHLBufferReference> m_usedAsyncBuffers;

    struct {
        int                           hotspotX = 0;
        int                           hotspotY = 0;
        std::optional<SP<CWLSurface>> surf;
        std::string                   name;
    } m_lastCursorData;

    CRenderPass m_renderPass = {};

  private:
    void arrangeLayerArray(PHLMONITOR, const std::vector<PHLLSREF>&, bool, CBox*);
    void renderWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry);
    void renderWorkspaceWindowsFullscreen(PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&); // renders workspace windows (fullscreen) (tiled, floating, pinned, but no special)
    void renderWorkspaceWindows(PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&);           // renders workspace windows (no fullscreen) (tiled, floating, pinned, but no special)
    void renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const Vector2D& translate = {0, 0}, const float& scale = 1.f);
    void renderWindow(PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, eRenderPassMode, bool ignorePosition = false, bool standalone = false);
    void renderLayer(PHLLS, PHLMONITOR, const Time::steady_tp&, bool popups = false, bool lockscreen = false);
    void renderSessionLockSurface(WP<SSessionLockSurface>, PHLMONITOR, const Time::steady_tp&);
    void renderDragIcon(PHLMONITOR, const Time::steady_tp&);
    void renderIMEPopup(CInputPopup*, PHLMONITOR, const Time::steady_tp&);
    void sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now); // sends frame displayed events but doesn't actually render anything
    void renderSessionLockPrimer(PHLMONITOR pMonitor);
    void renderSessionLockMissing(PHLMONITOR pMonitor);
    void renderBackground(PHLMONITOR pMonitor);

    bool commitPendingAndDoExplicitSync(PHLMONITOR pMonitor);

    bool shouldBlur(PHLLS ls);
    bool shouldBlur(PHLWINDOW w);
    bool shouldBlur(WP<CPopup> p);

    bool m_cursorHidden                           = false;
    bool m_cursorHasSurface                       = false;
    SP<CRenderbuffer>       m_currentRenderbuffer = nullptr;
    SP<Aquamarine::IBuffer> m_currentBuffer       = nullptr;
    eRenderMode             m_renderMode          = RENDER_MODE_NORMAL;
    bool                    m_nvidia              = false;
    bool                    m_intel               = false;
    bool                    m_software            = false;
    bool                    m_mgpu                = false;

    struct {
        bool hiddenOnTouch    = false;
        bool hiddenOnTimeout  = false;
        bool hiddenOnKeyboard = false;
    } m_cursorHiddenConditions;

    SP<CRenderbuffer>              getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt);
    std::vector<SP<CRenderbuffer>> m_renderbuffers;
    std::vector<PHLWINDOWREF>      m_renderUnfocused;
    SP<CEventLoopTimer>            m_renderUnfocusedTimer;

    friend class CHyprOpenGLImpl;
    friend class CToplevelExportFrame;
    friend class CInputManager;
    friend class CPointerManager;
    friend class CMonitor;
    friend class CMonitorFrameScheduler;
};

inline UP<CHyprRenderer> g_pHyprRenderer;
