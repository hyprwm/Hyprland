#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/Color.hpp"
#include "../helpers/time/Timer.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/Format.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include <GLES3/gl32.h>
#include <cstdint>
#include <list>
#include <string>
#include <stack>
#include <map>

#include <cairo/cairo.h>

#include "render/SyncFDManager.hpp"
#include "types.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"
#include "Renderbuffer.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "pass/Pass.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprgraphics/resource/resources/ImageResource.hpp>

#include "../debug/TracyDefines.hpp"
#include "../protocols/core/Compositor.hpp"
#include "ShaderLoader.hpp"
#include "gl/GLFramebuffer.hpp"
#include "gl/GLRenderbuffer.hpp"
#include "pass/TexPassElement.hpp"

#define GLFB(ifb) dc<CGLFramebuffer*>(ifb.get())

struct gbm_device;
namespace Render {
    class IHyprRenderer;
}
class CGradientValueData;

namespace Render::GL {

    struct SVertex {
        float x, y; // position
        float u, v; // uv
    };

    constexpr std::array<SVertex, 4> fullVerts = {{
        {0.0f, 0.0f, 0.0f, 0.0f}, // top-left
        {0.0f, 1.0f, 0.0f, 1.0f}, // bottom-left
        {1.0f, 0.0f, 1.0f, 0.0f}, // top-right
        {1.0f, 1.0f, 1.0f, 1.0f}, // bottom-right
    }};

    inline const float               fanVertsFull[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

    enum eMonitorRenderFBs : uint8_t {
        FB_MONITOR_RENDER_MAIN    = 0,
        FB_MONITOR_RENDER_CURRENT = 1,
        FB_MONITOR_RENDER_OUT     = 2,
    };

    enum eMonitorExtraRenderFBs : uint8_t {
        FB_MONITOR_RENDER_EXTRA_OFFLOAD = 0,
        FB_MONITOR_RENDER_EXTRA_MIRROR,
        FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP,
        FB_MONITOR_RENDER_EXTRA_OFF_MAIN,
        FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR,
        FB_MONITOR_RENDER_EXTRA_BLUR,
    };

    struct SFragShaderDesc {
        Render::ePreparedFragmentShader id;
        const char*                     file;
    };

    struct SPreparedShaders {
        // SPreparedShaders() {
        //     for (auto& f : frag) {
        //         f = makeShared<CShader>();
        //     }
        // }

        std::string TEXVERTSRC;
        std::string TEXVERTSRC320;
        // std::array<SP<CShader>, SH_FRAG_LAST> frag;
        // std::map<uint8_t, SP<CShader>>        fragVariants;
        std::array<std::map<Render::ShaderFeatureFlags, SP<CShader>>, Render::SH_FRAG_LAST> fragVariants;
    };

    struct SCurrentRenderData {
        PHLMONITORREF            pMonitor;
        Mat3x3                   projection;
        Mat3x3                   savedProjection;
        Mat3x3                   monitorProjection;

        SP<IFramebuffer>         currentFB = nullptr; // current rendering to
        SP<IFramebuffer>         mainFB    = nullptr; // main to render to
        SP<IFramebuffer>         outFB     = nullptr; // out to render to (if offloaded, etc)

        CRegion                  damage;
        CRegion                  finalDamage; // damage used for funal off -> main

        Render::SRenderModifData renderModif;
        float                    mouseZoomFactor    = 1.f;
        bool                     mouseZoomUseMouse  = true; // true by default
        bool                     useNearestNeighbor = false;
        bool                     blockScreenShader  = false;
        bool                     simplePass         = false;
        bool                     transformDamage    = true;
        bool                     noSimplify         = false;

        Vector2D                 primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        Vector2D                 primarySurfaceUVBottomRight = Vector2D(-1, -1);

        CBox                     clipBox = {}; // scaled coordinates
        CRegion                  clipRegion;

        uint32_t                 discardMode    = DISCARD_OPAQUE;
        float                    discardOpacity = 0.f;

        PHLLSREF                 currentLS;
        PHLWINDOWREF             currentWindow;
        WP<CWLSurfaceResource>   surface;
    };

    class CEGLSync : public ISyncFDManager {
      public:
        static UP<CEGLSync> create();
        ~CEGLSync() override;

        bool isValid() override;

      private:
        CEGLSync() : ISyncFDManager() {};

        EGLSyncKHR m_sync = EGL_NO_SYNC_KHR;

        friend class CHyprOpenGLImpl;
    };

    class CHyprOpenGLImpl {
      public:
        CHyprOpenGLImpl();
        ~CHyprOpenGLImpl();

        struct SRectRenderData {
            const CRegion* damage        = nullptr;
            int            round         = 0;
            float          roundingPower = 2.F;
            bool           blur          = false;
            float          blurA         = 1.F;
            bool           xray          = false;
        };

        struct STextureRenderData {
            bool                   blur  = false;
            float                  blurA = 1.F, overallA = 1.F;
            bool                   blockBlurOptimization = false;
            SP<ITexture>           blurredBG;

            const CRegion*         damage        = nullptr;
            SP<CWLSurfaceResource> surface       = nullptr;
            float                  a             = 1.F;
            int                    round         = 0;
            float                  roundingPower = 2.F;
            bool                   discardActive = false;
            bool                   allowCustomUV = false;
            bool                   allowDim      = true;
            bool                   noAA          = false; // unused
            GLenum                 wrapX = GL_CLAMP_TO_EDGE, wrapY = GL_CLAMP_TO_EDGE;
            bool                   cmBackToSRGB   = false;
            bool                   finalMonitorCM = false;
            SP<CMonitor>           cmBackToSRGBSource;

            uint32_t               discardMode    = DISCARD_OPAQUE;
            float                  discardOpacity = 0.f;

            CRegion                clipRegion;
            PHLLSREF               currentLS;

            Vector2D               primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            Vector2D               primarySurfaceUVBottomRight = Vector2D(-1, -1);
        };

        struct SBorderRenderData {
            int   round         = 0;
            float roundingPower = 2.F;
            int   borderSize    = 1;
            float a             = 1.0;
            int   outerRound    = -1; /* use round */
        };

        void                                      makeEGLCurrent();
        void                                      begin(PHLMONITOR, const CRegion& damage, SP<IFramebuffer> fb = nullptr, std::optional<CRegion> finalDamage = {});
        void                                      beginSimple(PHLMONITOR, const CRegion& damage, SP<IRenderbuffer> rb = nullptr, SP<IFramebuffer> fb = nullptr);
        void                                      end();

        void                                      renderRect(const CBox&, const CHyprColor&, SRectRenderData data);
        void                                      renderTexture(SP<ITexture>, const CBox&, STextureRenderData data);
        void                                      renderRoundedShadow(const CBox&, int round, float roundingPower, int range, const CHyprColor& color, float a = 1.0);
        void                                      renderBorder(const CBox&, const CGradientValueData&, SBorderRenderData data);
        void                                      renderBorder(const CBox&, const CGradientValueData&, const CGradientValueData&, float lerp, SBorderRenderData data);
        void                                      renderTextureMatte(SP<ITexture> tex, const CBox& pBox, SP<IFramebuffer> matte);
        void                                      renderTexturePrimitive(SP<ITexture> tex, const CBox& box);

        void                                      setViewport(GLint x, GLint y, GLsizei width, GLsizei height);
        void                                      setCapStatus(int cap, bool status);

        void                                      blend(bool enabled);

        void                                      scissor(const CBox&, bool transform = true);
        void                                      scissor(const pixman_box32*, bool transform = true);
        void                                      scissor(const int x, const int y, const int w, const int h, bool transform = true);

        void                                      destroyMonitorResources(PHLMONITORREF);

        void                                      preRender(PHLMONITOR);

        void                                      saveBufferForMirror(const CBox&);

        void                                      applyScreenShader(const std::string& path);

        void                                      renderOffToMain(SP<IFramebuffer> off);

        std::vector<SDRMFormat>                   getDRMFormats();
        std::vector<uint64_t>                     getDRMFormatModifiers(DRMFormat format);
        EGLImageKHR                               createEGLImage(const Aquamarine::SDMABUFAttrs& attrs);

        bool                                      initShaders(const std::string& path = "");

        WP<CShader>                               useShader(WP<CShader> prog);

        bool                                      explicitSyncSupported();
        WP<CShader>                               getShaderVariant(Render::ePreparedFragmentShader frag, Render::ShaderFeatureFlags features = 0);

        bool                                      m_shadersInitialized = false;
        SP<SPreparedShaders>                      m_shaders;

        Hyprutils::OS::CFileDescriptor            m_gbmFD;
        gbm_device*                               m_gbmDevice  = nullptr;
        EGLContext                                m_eglContext = nullptr;
        EGLDisplay                                m_eglDisplay = nullptr;
        EGLDeviceEXT                              m_eglDevice  = nullptr;

        std::map<PHLMONITORREF, SP<IFramebuffer>> m_monitorBGFBs;

        struct {
            PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES = nullptr;
            PFNGLEGLIMAGETARGETTEXTURE2DOESPROC           glEGLImageTargetTexture2DOES           = nullptr;
            PFNEGLCREATEIMAGEKHRPROC                      eglCreateImageKHR                      = nullptr;
            PFNEGLDESTROYIMAGEKHRPROC                     eglDestroyImageKHR                     = nullptr;
            PFNEGLQUERYDMABUFFORMATSEXTPROC               eglQueryDmaBufFormatsEXT               = nullptr;
            PFNEGLQUERYDMABUFMODIFIERSEXTPROC             eglQueryDmaBufModifiersEXT             = nullptr;
            PFNEGLGETPLATFORMDISPLAYEXTPROC               eglGetPlatformDisplayEXT               = nullptr;
            PFNEGLDEBUGMESSAGECONTROLKHRPROC              eglDebugMessageControlKHR              = nullptr;
            PFNEGLQUERYDEVICESEXTPROC                     eglQueryDevicesEXT                     = nullptr;
            PFNEGLQUERYDEVICESTRINGEXTPROC                eglQueryDeviceStringEXT                = nullptr;
            PFNEGLQUERYDISPLAYATTRIBEXTPROC               eglQueryDisplayAttribEXT               = nullptr;
            PFNEGLCREATESYNCKHRPROC                       eglCreateSyncKHR                       = nullptr;
            PFNEGLDESTROYSYNCKHRPROC                      eglDestroySyncKHR                      = nullptr;
            PFNEGLDUPNATIVEFENCEFDANDROIDPROC             eglDupNativeFenceFDANDROID             = nullptr;
            PFNEGLWAITSYNCKHRPROC                         eglWaitSyncKHR                         = nullptr;
        } m_proc;

        struct {
            bool EXT_read_format_bgra               = false;
            bool EXT_image_dma_buf_import           = false;
            bool EXT_image_dma_buf_import_modifiers = false;
            bool KHR_context_flush_control          = false;
            bool KHR_display_reference              = false;
            bool IMG_context_priority               = false;
            bool EXT_create_context_robustness      = false;
            bool EGL_ANDROID_native_fence_sync_ext  = false;
        } m_exts;

        enum eEGLContextVersion : uint8_t {
            EGL_CONTEXT_GLES_2_0 = 0,
            EGL_CONTEXT_GLES_3_0,
            EGL_CONTEXT_GLES_3_2,
        };

        eEGLContextVersion m_eglContextVersion = EGL_CONTEXT_GLES_3_2;

        enum eCachedCapStatus : uint8_t {
            CAP_STATUS_BLEND = 0,
            CAP_STATUS_SCISSOR_TEST,
            CAP_STATUS_STENCIL_TEST,
            CAP_STATUS_END
        };

      private:
        struct {
            GLint   x      = 0;
            GLint   y      = 0;
            GLsizei width  = 0;
            GLsizei height = 0;
        } m_lastViewport;

        std::array<bool, CAP_STATUS_END> m_capStatus = {};

        std::vector<SDRMFormat>          m_drmFormats;
        bool                             m_hasModifiers = false;

        int                              m_drmFD = -1;
        std::string                      m_extensions;

        bool                             m_fakeFrame            = false;
        bool                             m_applyFinalShader     = false;
        bool                             m_blend                = false;
        bool                             m_offloadedFramebuffer = false;
        bool                             m_cmSupported          = true;

        SP<CShader>                      m_finalScreenShader;
        GLuint                           m_currentProgram;

        void                             initDRMFormats();
        void                             initEGL(bool gbm);
        EGLDeviceEXT                     eglDeviceFromDRMFD(int drmFD);

        // for the final shader
        std::array<CTimer, POINTER_PRESSED_HISTORY_LENGTH>   m_pressedHistoryTimers    = {};
        std::array<Vector2D, POINTER_PRESSED_HISTORY_LENGTH> m_pressedHistoryPositions = {};
        GLint                                                m_pressedHistoryKilled    = 0;
        GLint                                                m_pressedHistoryTouched   = 0;

        //
        std::optional<std::vector<uint64_t>> getModsForFormat(EGLint format);

        // returns the out FB, can be either Mirror or MirrorSwap
        SP<IFramebuffer> blurFramebufferWithDamage(float a, CRegion* damage, CGLFramebuffer& source);

        void             passCMUniforms(WP<CShader>, const NColorManagement::PImageDescription imageDescription, const NColorManagement::PImageDescription targetImageDescription,
                                        bool modifySDR, float sdrMinLuminance, int sdrMaxLuminance, const SCMSettings& settings);
        void             passCMUniforms(WP<CShader>, const NColorManagement::PImageDescription imageDescription, const NColorManagement::PImageDescription targetImageDescription,
                                        bool modifySDR = false, float sdrMinLuminance = -1.0f, int sdrMaxLuminance = -1);
        void             passCMUniforms(WP<CShader>, const NColorManagement::PImageDescription imageDescription);
        void             renderRectInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
        void             renderRectWithBlurInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
        void             renderRectWithDamageInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
        WP<CShader>      renderToOutputInternal();
        WP<CShader>      renderToFBInternal(SP<ITexture> tex, const STextureRenderData& data, eTextureType texType, const CBox& newBox);
        void             renderTextureInternal(SP<ITexture>, const CBox&, const STextureRenderData& data);
        void             renderTextureWithBlurInternal(SP<ITexture>, const CBox&, const STextureRenderData& data);

        friend class IHyprRenderer;
        friend class CHyprGLRenderer;
        friend class CGLElementRenderer;
        friend class CTexPassElement;
        friend class CPreBlurElement;
        friend class CSurfacePassElement;
    };

    inline UP<CHyprOpenGLImpl> g_pHyprOpenGL;
}
