#pragma once

#include "../defines.hpp"
#include <cstdint>
#include <hyprgraphics/color/Color.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <list>
#include <optional>
#include <vector>
#include <utility>
#include "OpenGL.hpp"
#include "blur/Provider.hpp"
#include "./SyncFDManager.hpp"
#include "./pass/Pass.hpp"
#include "./pass/BorderPassElement.hpp"
#include "./pass/ClearPassElement.hpp"
#include "./pass/FramebufferElement.hpp"
#include "./pass/RectPassElement.hpp"
#include "./pass/RendererHintsPassElement.hpp"
#include "./pass/ShadowPassElement.hpp"
#include "./pass/SurfacePassElement.hpp"
#include "./pass/TexPassElement.hpp"
#include "./pass/TextureMatteElement.hpp"
#include "./pass/TransformedWindowPassElement.hpp"
#include "render/scene/MonitorScene.hpp"
#include "types.hpp"
#include "../output/Monitor.hpp"
#include "../desktop/state/Fadeout.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "Renderbuffer.hpp"
#include "../helpers/time/Timer.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/time/Time.hpp"
#include "../../protocols/cursor-shape-v1.hpp"
#include "desktop/view/Popup.hpp"
#include "Framebuffer.hpp"
#include "Texture.hpp"

#include <hyprgraphics/resource/resources/TextResource.hpp>

struct SMonitorRule;
class CWorkspace;
class CInputPopup;
class IHLBuffer;
class CEventLoopTimer;
class CToplevelExportProtocolManager;
class CInputManager;
struct SSessionLockSurface;
struct SBackdropScope;
namespace Screenshare {
    class CScreenshareFrame;
};
namespace Pointer {
    class CPointerManager;
}

namespace Render {
    using CScopeGuard = Hyprutils::Utils::CScopeGuard;

    class IElementRenderer;
    class CRenderPass;

    class IHyprRenderer {
      public:
        IHyprRenderer();
        virtual ~IHyprRenderer();

        enum eType : uint8_t {
            RT_GL = 1,
            RT_VK = 2,
        };

        virtual eType                   type() = 0;
        WP<Render::GL::CHyprOpenGLImpl> glBackend();

        void                            renderMonitor(PHLMONITOR pMonitor, bool commit = true);
        void                            arrangeLayersForMonitor(const MONITORID&);
        void                            damageSurface(SP<CWLSurfaceResource>, double, double, double scale = 1.0);
        void                            damageWindow(PHLWINDOW, bool forceFull = false);
        void                            damageBox(const CBox&, bool skipFrameSchedule = false);
        void                            damageBox(const int& x, const int& y, const int& w, const int& h);
        void                            damageRegion(const CRegion&);
        void                            damageMonitor(PHLMONITOR);
        void                            damageMirrorsWith(PHLMONITOR, const CRegion&);
        bool                            shouldRenderWindow(const CRenderingContext&, PHLWINDOW, PHLMONITOR);
        bool                            shouldRenderWindow(PHLWINDOW, PHLMONITOR);
        bool                            shouldRenderWindow(PHLWINDOW);
        float                           workspaceRenderAlpha(const CRenderingContext&, PHLWORKSPACE, PHLMONITOR = nullptr) const;
        Vector2D                        workspaceRenderOffset(const CRenderingContext&, PHLWORKSPACE, PHLMONITOR = nullptr) const;
        bool                            workspaceRenderIsAnimating(const CRenderingContext&, PHLWORKSPACE, PHLMONITOR = nullptr) const;
        Vector2D                        windowRenderFloatingOffset(const CRenderingContext&, PHLWINDOW) const;
        bool                            renderingWorkspaceToBuffer(const CRenderingContext&) const;
        // Renders only the target workspace's windows and popups during the target monitor's active render.
        bool renderWorkspaceToBuffer(CRenderingContext&, PHLWORKSPACE, SP<IFramebuffer>, bool sendFeedback = true);
        // Renders the target workspace as a complete monitor scene during the target monitor's active render.
        bool renderWorkspaceSceneToBuffer(CRenderingContext&, PHLWORKSPACE, SP<IFramebuffer>, const Time::steady_tp&, bool sendFeedback = true);
        // Renders and scales the target workspace to an aspect-ratio-matched framebuffer.
        bool renderWorkspaceSceneToBufferScaled(CRenderingContext&, PHLWORKSPACE, SP<IFramebuffer>, const Time::steady_tp&, bool sendFeedback = true);
        // Renders the target monitor's regular scene during its active render.
        bool renderMonitorToBuffer(CRenderingContext&, PHLMONITOR, SP<IFramebuffer>, const Time::steady_tp&, bool sendFeedback = true);
        // Renders the compositor background and layer-shell background plane.
        void                                renderMonitorBackground(CRenderingContext&, PHLMONITOR, const Time::steady_tp&);
        bool                                shouldRenderMonitor(PHLMONITOR);
        void                                ensureCursorRenderingMode();
        bool                                shouldRenderCursor();
        void                                setCursorHidden(bool hide);

        std::tuple<float, float, float>     getRenderTimes(PHLMONITOR pMonitor); // avg max min
        void                                ensureLockTexturesRendered(bool load);
        void                                renderLockscreen(CRenderingContext&, PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry);
        void                                setCursorSurface(SP<Desktop::View::CWLSurface> surf, int hotspotX, int hotspotY, bool force = false);
        void                                setCursorFromName(const std::string& name, bool force = false);
        void                                onRenderbufferDestroy(IRenderbuffer* rb);
        bool                                isNvidia();
        bool                                isIntel();
        bool                                isSoftware();
        bool                                isMgpu();
        void                                addWindowToRenderUnfocused(PHLWINDOW window);
        SP<IFramebuffer>                    makeSnapshotFB(PHLWINDOW);
        SP<IFramebuffer>                    makeSnapshotFB(PHLLS);
        SP<IFramebuffer>                    makeSnapshotFB(WP<Desktop::View::CPopup>);
        void                                renderFadeouts(CRenderingContext&, PHLMONITOR monitor, Desktop::eFadeoutPlane plane, PHLWORKSPACE workspace = nullptr);
        bool                                beginFullFakeRender(CRenderingContext&, CRegion& damage, SP<IFramebuffer> fb);
        bool                                beginRenderToBuffer(CRenderingContext&, CRegion& damage, SP<IHLBuffer> buffer, bool simple = false);
        virtual void                        startRenderPass(CRenderingContext&) {};
        virtual void                        endRender(CRenderingContext&, const std::function<void()>& renderingDoneCallback = {}) = 0;

        NColorManagement::PImageDescription workBufferImageDescription(const CRenderingContext&);
        PHLMONITORREF                       m_mostHzMonitor;
        bool                                m_directScanoutBlocked = false;

        void                                setSurfaceScanoutMode(SP<CWLSurfaceResource> surface, PHLMONITOR monitor); // nullptr monitor resets

        void                                initiateManualCrash();
        bool                                m_crashingInProgress = false;
        float                               m_crashingDistort    = 0.5f;
        wl_event_source*                    m_crashingLoop       = nullptr;
        wl_event_source*                    m_cursorTicker       = nullptr;

        struct {
            int                                          hotspotX      = 0;
            int                                          hotspotY      = 0;
            wpCursorShapeDeviceV1Shape                   shape         = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
            wpCursorShapeDeviceV1Shape                   shapePrevious = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
            CTimer                                       switchedTimer;
            std::optional<SP<Desktop::View::CWLSurface>> surf;
            std::string                                  name;
        } m_lastCursorData;

        void         addPassElement(CRenderingContext&, UP<IPassElement>&& element);

        SP<ITexture> renderSplash(const std::function<SP<ITexture>(const int, const int, unsigned char* const)>& handleData, const int fontSize, const int maxWidth = 1024,
                                  const int maxHeight = 1024);
        CHyprColor   getConvertedColor(const CRenderingContext&, const CHyprColor& color);

        virtual SP<IRenderbuffer>    getOrCreateRenderbuffer(SP<Aquamarine::IBuffer> buffer,
                                                             uint32_t                fmt); // TODO? move to protected and fix CPointerManager::renderHWCursorBuffer
        bool                         commitPendingAndDoExplicitSync(PHLMONITOR pMonitor, std::optional<Monitor::CDamageRing::CTransaction> damage = std::nullopt,
                                                                    const CRegion& renderedDamage = {}); // TODO? move to protected and fix CMonitorFrameScheduler::onPresented
        SP<ITexture>                 m_screencopyDeniedTexture;                                          // TODO? make readonly
        uint                         m_failedAssetsNo     = 0;                                           // TODO? make readonly
        bool                         m_reloadScreenShader = true;                                        // at launch it can be set
        CTimer                       m_globalTimer;

        void                         draw(CRenderingContext& context, WP<IPassElement> element, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CBorderPassElement::SBorderData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CClearPassElement::SClearData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CFramebufferElement::SFramebufferElementData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CRectPassElement::SRectData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CRendererHintsPassElement::SData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CShadowPassElement::SShadowData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CSurfacePassElement::SRenderData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CTexPassElement::SRenderData& data, const CRegion& damage = {});
        void                         draw(CRenderingContext&, const CTextureMatteElement::STextureMatteData& data, const CRegion& damage = {});
        virtual void                 bindFB(CRenderingContext&, SP<IFramebuffer> fb);
        UP<CScopeGuard>              bindTempFB(CRenderingContext&, SP<IFramebuffer> fb);
        virtual UP<ISyncFDManager>   createSyncFDManager()                                                                                                                     = 0;
        virtual WP<IElementRenderer> elementRenderer()                                                                                                                         = 0;
        virtual SP<ITexture>         createStencilTexture(const int width, const int height)                                                                                   = 0;
        virtual SP<ITexture>         createTexture(bool opaque = false)                                                                                                        = 0;
        virtual SP<ITexture>         createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false) = 0;
        virtual SP<ITexture>         createTexture(const Aquamarine::SDMABUFAttrs&, bool opaque = false)                                                                       = 0;
        virtual SP<ITexture>         createTexture(const int width, const int height, unsigned char* const)                                                                    = 0;
        virtual SP<ITexture>         createTexture(cairo_surface_t* cairo)                                                                                                     = 0;
        virtual SP<ITexture>         createTexture(std::span<const float> lut3D, size_t N)                                                                                     = 0;
        virtual SP<ITexture>         createTexture(const SP<Aquamarine::IBuffer> buffer, bool keepDataCopy = false);
        virtual SP<ITexture>         renderText(const std::string& text, CHyprColor col, int pt, bool italic = false, const std::string& fontFamily = "", int maxWidth = 0,
                                                int weight = 400);
        virtual SP<ITexture>         renderText(Hyprgraphics::CTextResource::STextResourceData&& data);
        SP<ITexture>                 loadAsset(const std::string& filename);
        virtual bool                 shouldUseNewBlurOptimizations(const CRenderingContext&, PHLLS pLayer, PHLWINDOW pWindow);
        virtual bool                 explicitSyncSupported()                                                                                                                  = 0;
        virtual bool                 fp16Supported()                                                                                                                          = 0;
        virtual std::vector<SDRMFormat> getDRMFormats()                                                                                                                       = 0;
        virtual std::vector<uint64_t>   getDRMFormatModifiers(DRMFormat format)                                                                                               = 0;
        virtual SP<IFramebuffer>        createFB(const std::string& name = "")                                                                                                = 0;
        virtual void                    disableScissor(CRenderingContext&)                                                                                                    = 0;
        virtual void                    blend(bool enabled)                                                                                                                   = 0;
        virtual void             drawShadow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) = 0;
        virtual void             drawShadow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                            const Config::CGradientValueData& grad2, float lerp, float a)                                                                     = 0;
        virtual void             drawGlow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a)   = 0;
        virtual void             drawGlow(CRenderingContext&, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                          const Config::CGradientValueData& grad2, float lerp, float a)                                                                       = 0;
        virtual void             setViewport(int x, int y, int width, int height)                                                                                             = 0;

        bool                     preBlurQueued(const CRenderingContext&);
        void                     sendFrameEventsToWorkspace(PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now);

        void                     setProjectionType(CRenderingContext&, const Vector2D& fbSize);
        void                     setProjectionType(CRenderingContext&, eRenderProjectionType projectionType);
        Mat3x3                   getBoxProjection(const CRenderingContext&, const CBox& box, std::optional<eTransform> transform = std::nullopt);
        Mat3x3                   projectBoxToTarget(const CRenderingContext&, const CBox& box, std::optional<eTransform> transform = std::nullopt);

        SP<IFramebuffer>         blurMainFramebuffer(CRenderingContext&, float strength, const CRegion& originalDamage, const Render::SBlurContext& blurContext = {});
        void                     beginBackdropScope(CRenderingContext&, SP<SBackdropScope> scope);
        void                     endBackdropScope(CRenderingContext&, SP<SBackdropScope> scope);
        virtual SP<IFramebuffer> blurFramebuffer(CRenderingContext&, SP<IFramebuffer> source, float strength, const CRegion& originalDamage,
                                                 const Render::SBlurContext& blurContext = {})   = 0;
        virtual void             refreshBlurProvider()                                           = 0;
        virtual void             expandBlurDamage(CRegion& damage, float multiplier = 1.F) const = 0;
        virtual bool             blurProviderIsAnimated(const CRenderingContext&) const          = 0;
        virtual bool             blurProviderRequiresLiveBlur() const                            = 0;
        void                     scheduleFrameForAnimatedBlur(const CRenderingContext&, const CRegion& damage, bool usesPrecomputedBlur);
        void                     preBlurForCurrentMonitor(CRenderingContext&, const CRegion& fakeDamage);

        SCMSettings              getCMSettings(const CRenderingContext&, const NColorManagement::PImageDescription imageDescription,
                                               const NColorManagement::PImageDescription targetImageDescription, SP<CWLSurfaceResource> surface = nullptr, bool modifySDR = false,
                                               float sdrMinLuminance = -1.0f, int sdrMaxLuminance = -1, bool shouldUseSurface = false);
        virtual bool             reloadShaders(const std::string& path = "") = 0;

      protected:
        virtual void              renderOffToMain(CRenderingContext&, SP<IFramebuffer> off)                     = 0;
        virtual SP<IRenderbuffer> getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) = 0;
        void                      renderMirrored(CRenderingContext&);
        void                      setDamage(CRenderingContext&, const CRegion& damage_, std::optional<CRegion> finalDamage);
        // if RENDER_MODE_NORMAL, provided damage will be written to.
        // otherwise, it will be the one used.
        bool beginRender(CRenderingContext&, CRegion& damage, eRenderMode mode = RENDER_MODE_NORMAL, SP<IHLBuffer> buffer = {}, SP<IFramebuffer> fb = nullptr, bool simple = false,
                         std::optional<Monitor::CDamageRing::CTransaction>* damageTransaction = nullptr);

        virtual bool beginRenderInternal(CRenderingContext&, CRegion& damage, bool simple = false) {
            return false;
        };
        virtual bool beginFullFakeRenderInternal(CRenderingContext&, CRegion& damage, SP<IFramebuffer> fb, bool simple = false) {
            return false;
        };
        virtual void initRender() {};
        virtual bool initRenderBuffer(CRenderingContext&, SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
            return false;
        };

        SP<ITexture>         getBackground(CRenderingContext&, PHLMONITOR pMonitor);
        virtual SP<ITexture> getBlurTexture(const CRenderingContext&, PHLMONITORREF pMonitor);

        SP<ITexture>         m_lockDeadTexture;
        SP<ITexture>         m_lockDead2Texture;
        SP<ITexture>         m_lockDead3Texture;
        SP<ITexture>         m_lockTtyTextTexture;
        void                 handleFullscreenSettings(PHLMONITOR pMonitor);
        bool                 renderWorkspaceToBufferInternal(CRenderingContext&, PHLWORKSPACE, SP<IFramebuffer>, const Time::steady_tp&, bool sendFeedback, bool fullScene,
                                                             bool scaleToBuffer = false);

        // old private:
        void         arrangeLayerArray(PHLMONITOR, const std::vector<PHLLSREF>&, bool, CBox*);
        void         renderWorkspace(CRenderingContext&, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const CBox& geometry);
        void         renderIME(CRenderingContext&, PHLMONITOR pMonitor, const Time::steady_tp& now, const CBox& geometry);
        void         renderWorkspaceWindowsFullscreen(CRenderingContext&, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&);
        void         renderWorkspaceWindows(CRenderingContext&, PHLMONITOR, PHLWORKSPACE, const Time::steady_tp&);
        void         renderAllClientsForWorkspace(CRenderingContext&, PHLMONITOR pMonitor, PHLWORKSPACE pWorkspace, const Time::steady_tp& now, const Vector2D& translate = {0, 0},
                                                  const float& scale = 1.f);
        void         renderWindow(CRenderingContext&, PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, eRenderPassMode, bool ignorePosition = false, bool standalone = false);
        void         renderLayer(CRenderingContext&, PHLLS, PHLMONITOR, const Time::steady_tp&, bool popups = false, bool lockscreen = false);
        void         renderSessionLockSurface(CRenderingContext&, WP<SSessionLockSurface>, PHLMONITOR, const Time::steady_tp&);
        void         renderDragIcon(CRenderingContext&, PHLMONITOR, const Time::steady_tp&);
        void         renderIMEPopup(CRenderingContext&, CInputPopup*, PHLMONITOR, const Time::steady_tp&);
        void         renderSessionLockPrimer(CRenderingContext&, PHLMONITOR pMonitor);
        void         renderSessionLockMissing(CRenderingContext&, PHLMONITOR pMonitor);
        void         renderBackground(CRenderingContext&, PHLMONITOR pMonitor);
        void         requestBackgroundResource();
        std::string  resolveAssetPath(const std::string& file);
        void         initMissingAssetTexture();
        void         initAssets();
        SP<ITexture> m_missingAssetTexture;
        ASP<Hyprgraphics::CImageResource> m_backgroundResource;
        bool                              m_backgroundResourceFailed = false;

        bool                              shouldBlur(const CRenderingContext&, PHLLS ls);
        bool                              shouldBlur(const CRenderingContext&, PHLWINDOW w);
        bool                              shouldBlur(const CRenderingContext&, WP<Desktop::View::CPopup> p);

        bool                              m_cursorHidden            = false;
        bool                              m_cursorHiddenByCondition = false;
        bool                              m_cursorHasSurface        = false;
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
        friend class Render::GL::CHyprOpenGLImpl;
        friend class CToplevelExportFrame;
        friend class Screenshare::CScreenshareFrame;
        friend class CInputManager;
        friend class Pointer::CPointerManager;
        friend class Monitor::CMonitor;
        friend class CMonitorFrameScheduler;
        friend class CMonitorScene;

      private:
        void bindOffMain(CRenderingContext&);
        void bindBackOnMain(CRenderingContext&);
    };

}

inline UP<Render::IHyprRenderer> g_pHyprRenderer;
