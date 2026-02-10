#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/Monitor.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "OpenGL.hpp"
#include "Renderbuffer.hpp"
#include "../helpers/time/Timer.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/time/Time.hpp"
#include "../../protocols/cursor-shape-v1.hpp"

struct SMonitorRule;
class CWorkspace;
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

struct SRenderData {
    PHLMONITORREF pMonitor;
    // Mat3x3        projection;
    // Mat3x3        savedProjection;
    // Mat3x3        monitorProjection;

    // // FIXME: raw pointer galore!
    // SMonitorRenderData*    pCurrentMonData = nullptr;
    // CFramebuffer*          currentFB       = nullptr; // current rendering to
    // CFramebuffer*          mainFB          = nullptr; // main to render to
    // CFramebuffer*          outFB           = nullptr; // out to render to (if offloaded, etc)

    // CRegion                damage;
    // CRegion                finalDamage; // damage used for funal off -> main

    // SRenderModifData       renderModif;
    // float                  mouseZoomFactor    = 1.f;
    // bool                   mouseZoomUseMouse  = true; // true by default
    // bool                   useNearestNeighbor = false;
    // bool                   blockScreenShader  = false;
    // bool                   simplePass         = false;

    // Vector2D               primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    // Vector2D               primarySurfaceUVBottomRight = Vector2D(-1, -1);

    // CBox                   clipBox = {}; // scaled coordinates
    // CRegion                clipRegion;

    // uint32_t               discardMode    = DISCARD_OPAQUE;
    // float                  discardOpacity = 0.f;

    // PHLLSREF               currentLS;
    // PHLWINDOWREF           currentWindow;
    // WP<CWLSurfaceResource> surface;
};

class IHyprRenderer {
  public:
    IHyprRenderer();
    virtual ~IHyprRenderer();

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
    void                            setCursorSurface(SP<Desktop::View::CWLSurface> surf, int hotspotX, int hotspotY, bool force = false);
    void                            setCursorFromName(const std::string& name, bool force = false);
    void                            onRenderbufferDestroy(CRenderbuffer* rb);
    bool                            isNvidia();
    bool                            isIntel();
    bool                            isSoftware();
    bool                            isMgpu();
    void                            makeEGLCurrent();
    void                            unsetEGL();
    void                            addWindowToRenderUnfocused(PHLWINDOW window);
    void                            makeSnapshot(PHLWINDOW);
    void                            makeSnapshot(PHLLS);
    void                            makeSnapshot(WP<Desktop::View::CPopup>);
    void                            renderSnapshot(PHLWINDOW);
    void                            renderSnapshot(PHLLS);
    void                            renderSnapshot(WP<Desktop::View::CPopup>);

    bool                            beginFullFakeRender(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple = false);
    bool                            beginRenderToBuffer(PHLMONITOR pMonitor, CRegion& damage, SP<IHLBuffer> buffer, bool simple = false);
    virtual void                    endRender(const std::function<void()>& renderingDoneCallback = {}) {};

    bool                            m_bBlockSurfaceFeedback = false;
    bool                            m_bRenderingSnapshot    = false;
    PHLMONITORREF                   m_mostHzMonitor;
    bool                            m_directScanoutBlocked = false;

    void                            setSurfaceScanoutMode(SP<CWLSurfaceResource> surface, PHLMONITOR monitor); // nullptr monitor resets
    void                            initiateManualCrash();
    const SRenderData&              renderData();

    bool                            m_crashingInProgress = false;
    float                           m_crashingDistort    = 0.5f;
    wl_event_source*                m_crashingLoop       = nullptr;
    wl_event_source*                m_cursorTicker       = nullptr;

    std::vector<CHLBufferReference> m_usedAsyncBuffers;

    struct {
        int                                          hotspotX      = 0;
        int                                          hotspotY      = 0;
        wpCursorShapeDeviceV1Shape                   shape         = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
        wpCursorShapeDeviceV1Shape                   shapePrevious = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
        CTimer                                       switchedTimer;
        std::optional<SP<Desktop::View::CWLSurface>> surf;
        std::string                                  name;
    } m_lastCursorData;

    CRenderPass       m_renderPass = {};

    bool              commitPendingAndDoExplicitSync(PHLMONITOR pMonitor);                   // TODO? move to protected and fix CMonitorFrameScheduler::onPresented
    SP<CRenderbuffer> getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt); // TODO? move to protected and fix CPointerManager::renderHWCursorBuffer
    void              renderWindow(PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, eRenderPassMode, bool ignorePosition = false,
                                   bool standalone = false); // // TODO? move to protected and fix CToplevelExportFrame

  protected:
    // if RENDER_MODE_NORMAL, provided damage will be written to.
    // otherwise, it will be the one used.
    bool beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode = RENDER_MODE_NORMAL, SP<IHLBuffer> buffer = {}, CFramebuffer* fb = nullptr, bool simple = false);

    virtual bool beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple = false) {
        return false;
    };
    virtual bool beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, CFramebuffer* fb, bool simple = false) {
        return false;
    };
    virtual void initRender() {};
    virtual bool initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
        return false;
    };

    void arrangeLayerArray(PHLMONITOR, const std::vector<PHLLSREF>&, bool, CBox*);
    void renderWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry);
    void renderWorkspaceWindowsFullscreen(PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&); // renders workspace windows (fullscreen) (tiled, floating, pinned, but no special)
    void renderWorkspaceWindows(PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&);           // renders workspace windows (no fullscreen) (tiled, floating, pinned, but no special)
    void renderAllClientsForWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const Vector2D& translate = {0, 0}, const float& scale = 1.f);
    void renderLayer(PHLLS, PHLMONITOR, const Time::steady_tp&, bool popups = false, bool lockscreen = false);
    void renderSessionLockSurface(WP<SSessionLockSurface>, PHLMONITOR, const Time::steady_tp&);
    void renderDragIcon(PHLMONITOR, const Time::steady_tp&);
    void renderIMEPopup(CInputPopup*, PHLMONITOR, const Time::steady_tp&);
    void sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now); // sends frame displayed events but doesn't actually render anything
    void renderSessionLockPrimer(PHLMONITOR pMonitor);
    void renderSessionLockMissing(PHLMONITOR pMonitor);
    void renderBackground(PHLMONITOR pMonitor);

    bool shouldBlur(PHLLS ls);
    bool shouldBlur(PHLWINDOW w);
    bool shouldBlur(WP<Desktop::View::CPopup> p);

    bool m_cursorHidden            = false;
    bool m_cursorHiddenByCondition = false;
    bool m_cursorHasSurface        = false;

    SP<Aquamarine::IBuffer> m_currentBuffer = nullptr;
    eRenderMode             m_renderMode    = RENDER_MODE_NORMAL;
    bool                    m_nvidia        = false;
    bool                    m_intel         = false;
    bool                    m_software      = false;
    bool                    m_mgpu          = false;

    struct {
        bool hiddenOnTouch    = false;
        bool hiddenOnTablet   = false;
        bool hiddenOnTimeout  = false;
        bool hiddenOnKeyboard = false;
    } m_cursorHiddenConditions;

    std::vector<SP<CRenderbuffer>> m_renderbuffers;
    std::vector<PHLWINDOWREF>      m_renderUnfocused;
    SP<CEventLoopTimer>            m_renderUnfocusedTimer;

    friend class CHyprOpenGLImpl; // TODO fix renderer - impl api

  private:
    SRenderData m_renderData;
};

inline UP<IHyprRenderer> g_pHyprRenderer;
