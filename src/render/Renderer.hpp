#pragma once

#include "../defines.hpp"
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/math/Box.hpp>
#include <list>
#include <optional>
#include "../helpers/Monitor.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "./pass/Pass.hpp"
#include "Renderbuffer.hpp"
#include "../helpers/time/Timer.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/time/Time.hpp"
#include "../../protocols/cursor-shape-v1.hpp"
#include "desktop/view/Popup.hpp"
#include "render/Framebuffer.hpp"
#include "render/Texture.hpp"
#include "render/pass/BorderPassElement.hpp"
#include "render/pass/ClearPassElement.hpp"
#include "render/pass/FramebufferElement.hpp"
#include "render/pass/PreBlurElement.hpp"
#include "render/pass/RectPassElement.hpp"
#include "render/pass/RendererHintsPassElement.hpp"
#include "render/pass/ShadowPassElement.hpp"
#include "render/pass/SurfacePassElement.hpp"
#include "render/pass/TexPassElement.hpp"
#include "render/pass/TextureMatteElement.hpp"

struct SMonitorRule;
class CWorkspace;
class CInputPopup;
class IHLBuffer;
class CEventLoopTimer;
class CRenderPass;

const std::vector<const char*> ASSET_PATHS = {
#ifdef DATAROOTDIR
    DATAROOTDIR,
#endif
    "/usr/share",
    "/usr/local/share",
};
class CToplevelExportProtocolManager;
class CInputManager;
struct SSessionLockSurface;
namespace Screenshare {
    class CScreenshareFrame;
};

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

struct SRenderWorkspaceUntilData {
    PHLLS     ls;
    PHLWINDOW w;
};

enum eRenderProjectionType : uint8_t {
    RPT_MONITOR,
    RPT_MIRROR,
    RPT_FB,
};

struct SRenderData {
    // can be private
    Mat3x3 targetProjection;

    // ----------------------

    // used by public
    Vector2D              fbSize = {-1, -1};
    PHLMONITORREF         pMonitor;

    eRenderProjectionType projectionType = RPT_MONITOR;

    SP<IFramebuffer>      currentFB = nullptr; // current rendering to
    SP<IFramebuffer>      mainFB    = nullptr; // main to render to
    SP<IFramebuffer>      outFB     = nullptr; // out to render to (if offloaded, etc)

    CRegion               damage;
    CRegion               finalDamage; // damage used for funal off -> main

    SRenderModifData      renderModif;
    float                 mouseZoomFactor    = 1.f;
    bool                  mouseZoomUseMouse  = true; // true by default
    bool                  useNearestNeighbor = false;
    bool                  blockScreenShader  = false;

    Vector2D              primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    Vector2D              primarySurfaceUVBottomRight = Vector2D(-1, -1);

    // TODO remove and pass directly
    CBox                   clipBox = {}; // scaled coordinates
    CRegion                clipRegion;

    uint32_t               discardMode    = DISCARD_OPAQUE;
    float                  discardOpacity = 0.f;

    PHLLSREF               currentLS;
    PHLWINDOWREF           currentWindow;
    WP<CWLSurfaceResource> surface;

    bool                   transformDamage = true;
    bool                   noSimplify      = false;
};

struct STFRange {
    float min = 0;
    float max = 80;
};

struct SCMSettings {
    NColorManagement::eTransferFunction  sourceTF = NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22;
    NColorManagement::eTransferFunction  targetTF = NColorManagement::CM_TRANSFER_FUNCTION_GAMMA22;
    STFRange                             srcTFRange;
    STFRange                             dstTFRange;
    float                                srcRefLuminance = 80;
    float                                dstRefLuminance = 80;
    std::array<std::array<double, 3>, 3> convertMatrix;

    bool                                 needsTonemap    = false;
    float                                maxLuminance    = 80;
    float                                dstMaxLuminance = 80;
    std::array<std::array<double, 3>, 3> dstPrimaries2XYZ;
    bool                                 needsSDRmod             = false;
    float                                sdrSaturation           = 1.0;
    float                                sdrBrightnessMultiplier = 1.0;
};

class IHyprRenderer {
  public:
    IHyprRenderer();
    ~IHyprRenderer();

    WP<CHyprOpenGLImpl> glBackend();

    void                renderMonitor(PHLMONITOR pMonitor, bool commit = true);
    void                arrangeLayersForMonitor(const MONITORID&);
    void                damageSurface(SP<CWLSurfaceResource>, double, double, double scale = 1.0);
    void                damageWindow(PHLWINDOW, bool forceFull = false);
    void                damageBox(const CBox&, bool skipFrameSchedule = false);
    void                damageBox(const int& x, const int& y, const int& w, const int& h);
    void                damageRegion(const CRegion&);
    void                damageMonitor(PHLMONITOR);
    void                damageMirrorsWith(PHLMONITOR, const CRegion&);
    bool                shouldRenderWindow(PHLWINDOW, PHLMONITOR);
    bool                shouldRenderWindow(PHLWINDOW);
    void                ensureCursorRenderingMode();
    bool                shouldRenderCursor();
    void                setCursorHidden(bool hide);
    void calculateUVForSurface(PHLWINDOW, SP<CWLSurfaceResource>, PHLMONITOR pMonitor, bool main = false, const Vector2D& projSize = {}, const Vector2D& projSizeUnscaled = {},
                               bool fixMisalignedFSV1 = false);
    std::tuple<float, float, float> getRenderTimes(PHLMONITOR pMonitor); // avg max min
    void                            ensureLockTexturesRendered(bool load);
    void                            renderLockscreen(PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry);
    void                            setCursorSurface(SP<Desktop::View::CWLSurface> surf, int hotspotX, int hotspotY, bool force = false);
    void                            setCursorFromName(const std::string& name, bool force = false);
    void                            onRenderbufferDestroy(IRenderbuffer* rb);
    SP<IRenderbuffer>               getCurrentRBO();
    bool                            isNvidia();
    bool                            isIntel();
    bool                            isSoftware();
    bool                            isMgpu();
    void                            addWindowToRenderUnfocused(PHLWINDOW window);
    void                            makeSnapshot(PHLWINDOW);
    void                            makeSnapshot(PHLLS);
    void                            makeSnapshot(WP<Desktop::View::CPopup>);
    void                            renderSnapshot(PHLWINDOW);
    void                            renderSnapshot(PHLLS);
    void                            renderSnapshot(WP<Desktop::View::CPopup>);

    //
    NColorManagement::PImageDescription workBufferImageDescription();

    // if RENDER_MODE_NORMAL, provided damage will be written to.
    // otherwise, it will be the one used.
    bool beginRender(PHLMONITOR pMonitor, CRegion& damage, eRenderMode mode = RENDER_MODE_NORMAL, SP<IHLBuffer> buffer = {}, SP<IFramebuffer> fb = nullptr, bool simple = false);
    void startRenderPass() {};
    void endRender(const std::function<void()>& renderingDoneCallback = {});

    bool m_bBlockSurfaceFeedback = false;
    bool m_bRenderingSnapshot    = false;
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

    SP<IRenderbuffer> getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt); // TODO? move to protected and fix CPointerManager::renderHWCursorBuffer
    SRenderData       m_renderData;                                                          // TODO? move to protected and fix CRenderPass
    SP<ITexture>      m_screencopyDeniedTexture;                                             // TODO? make readonly
    uint              m_failedAssetsNo     = 0;                                              // TODO? make readonly
    bool              m_reloadScreenShader = true;                                           // at launch it can be set
    CTimer            m_globalTimer;

    SP<ITexture>      createStencilTexture(const int width, const int height);
    SP<ITexture>      createTexture(bool opaque = false);
    SP<ITexture>      createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false);
    SP<ITexture>      createTexture(const Aquamarine::SDMABUFAttrs&, bool opaque = false);
    SP<ITexture>      createTexture(const int width, const int height, unsigned char* const);
    SP<ITexture>      createTexture(cairo_surface_t* cairo);
    SP<ITexture>      createTexture(const SP<Aquamarine::IBuffer> buffer, bool keepDataCopy = false);
    SP<ITexture>      createTexture(std::span<const float> lut3D, size_t N);
    SP<ITexture>      renderText(const std::string& text, CHyprColor col, int pt, bool italic = false, const std::string& fontFamily = "", int maxWidth = 0, int weight = 400);
    SP<ITexture>      loadAsset(const std::string& filename);
    bool              shouldUseNewBlurOptimizations(PHLLS pLayer, PHLWINDOW pWindow);
    bool              explicitSyncSupported();
    std::vector<SDRMFormat> getDRMFormats();
    std::vector<uint64_t>   getDRMFormatModifiers(DRMFormat format);
    SP<IFramebuffer>        createFB(const std::string& name = "");
    void                    pushMonitorTransformEnabled(bool enabled);
    void                    popMonitorTransformEnabled();
    bool                    monitorTransformEnabled();

    void                    setProjectionType(const Vector2D& fbSize);
    void                    setProjectionType(eRenderProjectionType projectionType);
    Mat3x3                  getBoxProjection(const CBox& box, std::optional<eTransform> transform = std::nullopt);
    Mat3x3                  projectBoxToTarget(const CBox& box, std::optional<eTransform> transform = std::nullopt);

    SCMSettings             getCMSettings(const NColorManagement::PImageDescription imageDescription, const NColorManagement::PImageDescription targetImageDescription,
                                          SP<CWLSurfaceResource> surface = nullptr, bool modifySDR = false, float sdrMinLuminance = -1.0f, int sdrMaxLuminance = -1);
    bool                    reloadShaders(const std::string& path = "");

    void                    draw(CBorderPassElement* element, const CRegion& damage);
    void                    draw(CClearPassElement* element, const CRegion& damage);
    void                    draw(CFramebufferElement* element, const CRegion& damage);
    void                    draw(CPreBlurElement* element, const CRegion& damage);
    void                    draw(CRectPassElement* element, const CRegion& damage);
    void                    draw(CRendererHintsPassElement* element, const CRegion& damage);
    void                    draw(CShadowPassElement* element, const CRegion& damage);
    void                    draw(CSurfacePassElement* element, const CRegion& damage);
    void                    draw(CTexPassElement* element, const CRegion& damage);
    void                    draw(CTextureMatteElement* element, const CRegion& damage);
    void                    draw(WP<IPassElement> element, const CRegion& damage);

    SP<ITexture>            m_lockDeadTexture;
    SP<ITexture>            m_lockDead2Texture;
    SP<ITexture>            m_lockTtyTextTexture;
    bool                    m_monitorTransformEnabled = false; // do not modify directly
    std::stack<bool>        m_monitorTransformStack;

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

    void requestBackgroundResource();
    //
    SP<IRenderbuffer>                 getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt);
    std::string                       resolveAssetPath(const std::string& file);
    void                              initMissingAssetTexture();
    void                              initAssets();
    SP<ITexture>                      m_missingAssetTexture;
    ASP<Hyprgraphics::CImageResource> m_backgroundResource;
    bool                              m_backgroundResourceFailed = false;

    bool                              shouldBlur(PHLLS ls);
    bool                              shouldBlur(PHLWINDOW w);
    bool                              shouldBlur(WP<Desktop::View::CPopup> p);

    bool                              m_cursorHidden            = false;
    bool                              m_cursorHiddenByCondition = false;
    bool                              m_cursorHasSurface        = false;
    SP<IRenderbuffer>                 m_currentRenderbuffer     = nullptr;
    SP<Aquamarine::IBuffer>           m_currentBuffer           = nullptr;
    eRenderMode                       m_renderMode              = RENDER_MODE_NORMAL;
    bool                              m_nvidia                  = false;
    bool                              m_intel                   = false;
    bool                              m_software                = false;
    bool                              m_mgpu                    = false;

    struct {
        bool hiddenOnTouch    = false;
        bool hiddenOnTablet   = false;
        bool hiddenOnTimeout  = false;
        bool hiddenOnKeyboard = false;
    } m_cursorHiddenConditions;

    std::vector<SP<IRenderbuffer>> m_renderbuffers;
    std::vector<PHLWINDOWREF>      m_renderUnfocused;
    SP<CEventLoopTimer>            m_renderUnfocusedTimer;

    friend class CRenderPass;
    friend class CHyprOpenGLImpl;
    friend class CToplevelExportFrame;
    friend class Screenshare::CScreenshareFrame;
    friend class CInputManager;
    friend class CPointerManager;
    friend class CMonitor;
    friend class CMonitorFrameScheduler;
};

inline UP<IHyprRenderer> g_pHyprRenderer;
