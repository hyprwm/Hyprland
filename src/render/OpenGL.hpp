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

#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"
#include "Renderbuffer.hpp"
#include "desktop/DesktopTypes.hpp"
#include "pass/Pass.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprgraphics/resource/resources/ImageResource.hpp>

#include "../debug/TracyDefines.hpp"
#include "../protocols/core/Compositor.hpp"
#include "render/gl/GLFramebuffer.hpp"
#include "render/gl/GLRenderbuffer.hpp"

#define GLFB(ifb) dc<CGLFramebuffer*>(ifb.get())

struct gbm_device;
class IHyprRenderer;

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

enum eDiscardMode : uint8_t {
    DISCARD_OPAQUE = 1,
    DISCARD_ALPHA  = 1 << 1
};

struct SRenderModifData {
    enum eRenderModifType : uint8_t {
        RMOD_TYPE_SCALE,        /* scale by a float */
        RMOD_TYPE_SCALECENTER,  /* scale by a float from the center */
        RMOD_TYPE_TRANSLATE,    /* translate by a Vector2D */
        RMOD_TYPE_ROTATE,       /* rotate by a float in rad from top left */
        RMOD_TYPE_ROTATECENTER, /* rotate by a float in rad from center */
    };

    std::vector<std::pair<eRenderModifType, std::any>> modifs;

    void                                               applyToBox(CBox& box);
    void                                               applyToRegion(CRegion& rg);
    float                                              combinedScale();

    bool                                               enabled = true;
};

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

enum ePreparedFragmentShader : uint8_t {
    SH_FRAG_QUAD = 0,
    SH_FRAG_PASSTHRURGBA,
    SH_FRAG_MATTE,
    SH_FRAG_EXT,
    SH_FRAG_BLUR1,
    SH_FRAG_BLUR2,
    SH_FRAG_CM_BLURPREPARE,
    SH_FRAG_BLURPREPARE,
    SH_FRAG_BLURFINISH,
    SH_FRAG_SHADOW,
    SH_FRAG_CM_BORDER1,
    SH_FRAG_BORDER1,
    SH_FRAG_GLITCH,

    SH_FRAG_LAST,
};

enum ePreparedFragmentShaderFeature : uint8_t {
    SH_FEAT_UNKNOWN = 0, // all features just in case

    SH_FEAT_RGBA     = (1 << 0), // RGBA/RGBX texture sampling
    SH_FEAT_DISCARD  = (1 << 1), // RGBA/RGBX texture sampling
    SH_FEAT_TINT     = (1 << 2), // uniforms: tint; condition: applyTint
    SH_FEAT_ROUNDING = (1 << 3), // uniforms: radius, roundingPower, topLeft, fullSize; condition: radius > 0
    SH_FEAT_CM       = (1 << 4), // uniforms: srcTFRange, dstTFRange, srcRefLuminance, convertMatrix; condition: !skipCM
    SH_FEAT_TONEMAP  = (1 << 5), // uniforms: maxLuminance, dstMaxLuminance, dstRefLuminance; condition: maxLuminance < dstMaxLuminance * 1.01
    SH_FEAT_SDR_MOD  = (1 << 6), // uniforms: sdrSaturation, sdrBrightnessMultiplier; condition: SDR <-> HDR && (sdrSaturation != 1 || sdrBrightnessMultiplier != 1)

    // uniforms: targetPrimariesXYZ; condition: SH_FEAT_TONEMAP || SH_FEAT_SDR_MOD
};

struct SFragShaderDesc {
    ePreparedFragmentShader id;
    const char*             file;
};

struct SPreparedShaders {
    SPreparedShaders() {
        for (auto& f : frag) {
            f = makeShared<CShader>();
        }
    }

    std::string                           TEXVERTSRC;
    std::string                           TEXVERTSRC320;
    std::array<SP<CShader>, SH_FRAG_LAST> frag;
    std::map<uint8_t, SP<CShader>>        fragVariants;
};

struct SCurrentRenderData {
    PHLMONITORREF          pMonitor;

    SP<IFramebuffer>       mainFB = nullptr; // main to render to
    SP<IFramebuffer>       outFB  = nullptr; // out to render to (if offloaded, etc)

    SP<IRenderbuffer>      m_currentRenderbuffer = nullptr;

    bool                   simplePass = false;

    CRegion                clipRegion;

    uint32_t               discardMode    = DISCARD_OPAQUE;
    float                  discardOpacity = 0.f;

    PHLLSREF               currentLS;
    WP<CWLSurfaceResource> surface;
};

class CEGLSync {
  public:
    static UP<CEGLSync> create();

    ~CEGLSync();

    Hyprutils::OS::CFileDescriptor&  fd();
    Hyprutils::OS::CFileDescriptor&& takeFd();
    bool                             isValid();

  private:
    CEGLSync() = default;

    Hyprutils::OS::CFileDescriptor m_fd;
    EGLSyncKHR                     m_sync  = EGL_NO_SYNC_KHR;
    bool                           m_valid = false;

    friend class CHyprOpenGLImpl;
};

class CGradientValueData;

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
        const CRegion*         damage  = nullptr;
        SP<CWLSurfaceResource> surface = nullptr;
        float                  a       = 1.F;
        bool                   blur    = false;
        float                  blurA = 1.F, overallA = 1.F;
        int                    round                 = 0;
        float                  roundingPower         = 2.F;
        bool                   discardActive         = false;
        bool                   allowCustomUV         = false;
        bool                   allowDim              = true;
        bool                   noAA                  = false;
        bool                   blockBlurOptimization = false;
        GLenum                 wrapX = GL_CLAMP_TO_EDGE, wrapY = GL_CLAMP_TO_EDGE;
        bool                   cmBackToSRGB = false;
        SP<CMonitor>           cmBackToSRGBSource;
    };

    struct SBorderRenderData {
        int   round         = 0;
        float roundingPower = 2.F;
        int   borderSize    = 1;
        float a             = 1.0;
        int   outerRound    = -1; /* use round */
    };

    void                                    makeEGLCurrent();
    void                                    begin(PHLMONITOR, const CRegion& damage, SP<IFramebuffer> fb = nullptr, std::optional<CRegion> finalDamage = {});
    void                                    beginSimple(PHLMONITOR, const CRegion& damage, SP<IRenderbuffer> rb = nullptr, SP<IFramebuffer> fb = nullptr);
    void                                    end();

    void                                    renderRect(const CBox&, const CHyprColor&, SRectRenderData data);
    void                                    renderTexture(SP<ITexture>, const CBox&, STextureRenderData data);
    void                                    renderRoundedShadow(const CBox&, int round, float roundingPower, int range, const CHyprColor& color, float a = 1.0);
    void                                    renderBorder(const CBox&, const CGradientValueData&, SBorderRenderData data);
    void                                    renderBorder(const CBox&, const CGradientValueData&, const CGradientValueData&, float lerp, SBorderRenderData data);
    void                                    renderTextureMatte(SP<ITexture> tex, const CBox& pBox, SP<IFramebuffer> matte);

    void                                    setViewport(GLint x, GLint y, GLsizei width, GLsizei height);
    void                                    setCapStatus(int cap, bool status);

    void                                    blend(bool enabled);

    void                                    clear(const CHyprColor&);
    void                                    scissor(const CBox&, bool transform = true);
    void                                    scissor(const pixman_box32*, bool transform = true);
    void                                    scissor(const int x, const int y, const int w, const int h, bool transform = true);

    void                                    destroyMonitorResources(PHLMONITORREF);

    void                                    markBlurDirtyForMonitor(PHLMONITOR);

    void                                    preRender(PHLMONITOR);

    void                                    saveBufferForMirror(const CBox&);

    void                                    applyScreenShader(const std::string& path);

    void                                    bindOffMain();
    void                                    renderOffToMain(IFramebuffer* off);
    void                                    bindBackOnMain();

    std::vector<SDRMFormat>                 getDRMFormats();
    EGLImageKHR                             createEGLImage(const Aquamarine::SDMABUFAttrs& attrs);

    bool                                    initShaders();

    WP<CShader>                             useShader(WP<CShader> prog);

    bool                                    explicitSyncSupported();
    WP<CShader>                             getSurfaceShader(uint8_t features);

    bool                                    m_shadersInitialized = false;
    SP<SPreparedShaders>                    m_shaders;
    std::map<std::string, std::string>      m_includes;

    SCurrentRenderData                      m_renderData;

    Hyprutils::OS::CFileDescriptor          m_gbmFD;
    gbm_device*                             m_gbmDevice  = nullptr;
    EGLContext                              m_eglContext = nullptr;
    EGLDisplay                              m_eglDisplay = nullptr;
    EGLDeviceEXT                            m_eglDevice  = nullptr;

    std::map<PHLMONITORREF, CGLFramebuffer> m_monitorBGFBs;

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
    void                             requestBackgroundResource();

    // for the final shader
    std::array<CTimer, POINTER_PRESSED_HISTORY_LENGTH>   m_pressedHistoryTimers    = {};
    std::array<Vector2D, POINTER_PRESSED_HISTORY_LENGTH> m_pressedHistoryPositions = {};
    GLint                                                m_pressedHistoryKilled    = 0;
    GLint                                                m_pressedHistoryTouched   = 0;

    //
    std::optional<std::vector<uint64_t>> getModsForFormat(EGLint format);

    // returns the out FB, can be either Mirror or MirrorSwap
    SP<IFramebuffer> blurMainFramebufferWithDamage(float a, CRegion* damage);
    SP<IFramebuffer> blurFramebufferWithDamage(float a, CRegion* damage, CGLFramebuffer& source);

    void             passCMUniforms(WP<CShader>, const NColorManagement::PImageDescription imageDescription, const NColorManagement::PImageDescription targetImageDescription,
                                    bool modifySDR = false, float sdrMinLuminance = -1.0f, int sdrMaxLuminance = -1);
    void             passCMUniforms(WP<CShader>, const NColorManagement::PImageDescription imageDescription);
    void             renderTexturePrimitive(SP<ITexture> tex, const CBox& box);
    void             renderRectInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
    void             renderRectWithBlurInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
    void             renderRectWithDamageInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
    void             renderTextureInternal(SP<ITexture>, const CBox&, const STextureRenderData& data);
    void             renderTextureWithBlurInternal(SP<ITexture>, const CBox&, const STextureRenderData& data);

    void             preBlurForCurrentMonitor(CRegion* fakeDamage);

    friend class IHyprRenderer;
    friend class CHyprGLRenderer;
    friend class CTexPassElement;
    friend class CPreBlurElement;
    friend class CSurfacePassElement;
};

inline UP<CHyprOpenGLImpl> g_pHyprOpenGL;
