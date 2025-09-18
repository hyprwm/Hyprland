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
#include <unordered_map>
#include <stack>
#include <map>

#include <cairo/cairo.h>

#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"
#include "Renderbuffer.hpp"
#include "pass/Pass.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/os/FileDescriptor.hpp>
#include <hyprgraphics/resource/resources/ImageResource.hpp>

#include "../debug/TracyDefines.hpp"
#include "../protocols/core/Compositor.hpp"

struct gbm_device;
class CHyprRenderer;

inline const float fullVerts[] = {
    1, 0, // top right
    0, 0, // top left
    1, 1, // bottom right
    0, 1, // bottom left
};
inline const float fanVertsFull[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

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

struct SPreparedShaders {
    std::string TEXVERTSRC;
    std::string TEXVERTSRC320;
    SShader     m_shQUAD;
    SShader     m_shRGBA;
    SShader     m_shPASSTHRURGBA;
    SShader     m_shMATTE;
    SShader     m_shRGBX;
    SShader     m_shEXT;
    SShader     m_shBLUR1;
    SShader     m_shBLUR2;
    SShader     m_shBLURPREPARE;
    SShader     m_shBLURFINISH;
    SShader     m_shSHADOW;
    SShader     m_shBORDER1;
    SShader     m_shGLITCH;
    SShader     m_shCM;
};

struct SMonitorRenderData {
    CFramebuffer offloadFB;
    CFramebuffer mirrorFB;     // these are used for some effects,
    CFramebuffer mirrorSwapFB; // etc
    CFramebuffer offMainFB;
    CFramebuffer monitorMirrorFB; // used for mirroring outputs, does not contain artifacts like offloadFB
    CFramebuffer blurFB;

    SP<CTexture> stencilTex = makeShared<CTexture>();

    bool         blurFBDirty        = true;
    bool         blurFBShouldRender = false;
};

struct SCurrentRenderData {
    PHLMONITORREF pMonitor;
    Mat3x3        projection;
    Mat3x3        savedProjection;
    Mat3x3        monitorProjection;

    // FIXME: raw pointer galore!
    SMonitorRenderData*    pCurrentMonData = nullptr;
    CFramebuffer*          currentFB       = nullptr; // current rendering to
    CFramebuffer*          mainFB          = nullptr; // main to render to
    CFramebuffer*          outFB           = nullptr; // out to render to (if offloaded, etc)

    CRegion                damage;
    CRegion                finalDamage; // damage used for funal off -> main

    SRenderModifData       renderModif;
    float                  mouseZoomFactor    = 1.f;
    bool                   mouseZoomUseMouse  = true; // true by default
    bool                   useNearestNeighbor = false;
    bool                   blockScreenShader  = false;
    bool                   simplePass         = false;

    Vector2D               primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    Vector2D               primarySurfaceUVBottomRight = Vector2D(-1, -1);

    CBox                   clipBox = {}; // scaled coordinates
    CRegion                clipRegion;

    uint32_t               discardMode    = DISCARD_OPAQUE;
    float                  discardOpacity = 0.f;

    PHLLSREF               currentLS;
    PHLWINDOWREF           currentWindow;
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

    void         begin(PHLMONITOR, const CRegion& damage, CFramebuffer* fb = nullptr, std::optional<CRegion> finalDamage = {});
    void         beginSimple(PHLMONITOR, const CRegion& damage, SP<CRenderbuffer> rb = nullptr, CFramebuffer* fb = nullptr);
    void         end();

    void         renderRect(const CBox&, const CHyprColor&, SRectRenderData data);
    void         renderTexture(SP<CTexture>, const CBox&, STextureRenderData data);
    void         renderRoundedShadow(const CBox&, int round, float roundingPower, int range, const CHyprColor& color, float a = 1.0);
    void         renderBorder(const CBox&, const CGradientValueData&, SBorderRenderData data);
    void         renderBorder(const CBox&, const CGradientValueData&, const CGradientValueData&, float lerp, SBorderRenderData data);
    void         renderTextureMatte(SP<CTexture> tex, const CBox& pBox, CFramebuffer& matte);

    void         pushMonitorTransformEnabled(bool enabled);
    void         popMonitorTransformEnabled();

    void         setRenderModifEnabled(bool enabled);
    void         setViewport(GLint x, GLint y, GLsizei width, GLsizei height);
    void         setCapStatus(int cap, bool status);

    void         saveMatrix();
    void         setMatrixScaleTranslate(const Vector2D& translate, const float& scale);
    void         restoreMatrix();

    void         blend(bool enabled);

    bool         shouldUseNewBlurOptimizations(PHLLS pLayer, PHLWINDOW pWindow);

    void         clear(const CHyprColor&);
    void         clearWithTex();
    void         scissor(const CBox&, bool transform = true);
    void         scissor(const pixman_box32*, bool transform = true);
    void         scissor(const int x, const int y, const int w, const int h, bool transform = true);

    void         destroyMonitorResources(PHLMONITORREF);

    void         markBlurDirtyForMonitor(PHLMONITOR);

    void         preWindowPass();
    bool         preBlurQueued();
    void         preRender(PHLMONITOR);

    void         saveBufferForMirror(const CBox&);
    void         renderMirrored();

    void         applyScreenShader(const std::string& path);

    void         bindOffMain();
    void         renderOffToMain(CFramebuffer* off);
    void         bindBackOnMain();

    std::string  resolveAssetPath(const std::string& file);
    SP<CTexture> loadAsset(const std::string& file);
    SP<CTexture> texFromCairo(cairo_surface_t* cairo);
    SP<CTexture> renderText(const std::string& text, CHyprColor col, int pt, bool italic = false, const std::string& fontFamily = "", int maxWidth = 0, int weight = 400);

    void         setDamage(const CRegion& damage, std::optional<CRegion> finalDamage = {});

    uint32_t     getPreferredReadFormat(PHLMONITOR pMonitor);
    std::vector<SDRMFormat>                     getDRMFormats();
    EGLImageKHR                                 createEGLImage(const Aquamarine::SDMABUFAttrs& attrs);

    bool                                        initShaders();

    GLuint                                      createProgram(const std::string&, const std::string&, bool dynamic = false, bool silent = false);
    GLuint                                      compileShader(const GLuint&, std::string, bool dynamic = false, bool silent = false);
    void                                        useProgram(GLuint prog);

    void                                        ensureLockTexturesRendered(bool load);

    bool                                        explicitSyncSupported();

    bool                                        m_shadersInitialized = false;
    SP<SPreparedShaders>                        m_shaders;

    SCurrentRenderData                          m_renderData;

    Hyprutils::OS::CFileDescriptor              m_gbmFD;
    gbm_device*                                 m_gbmDevice      = nullptr;
    EGLContext                                  m_eglContext     = nullptr;
    EGLDisplay                                  m_eglDisplay     = nullptr;
    EGLDeviceEXT                                m_eglDevice      = nullptr;
    uint                                        m_failedAssetsNo = 0;

    bool                                        m_reloadScreenShader = true; // at launch it can be set

    std::map<PHLWINDOWREF, CFramebuffer>        m_windowFramebuffers;
    std::map<PHLLSREF, CFramebuffer>            m_layerFramebuffers;
    std::map<WP<CPopup>, CFramebuffer>          m_popupFramebuffers;
    std::map<PHLMONITORREF, SMonitorRenderData> m_monitorRenderResources;
    std::map<PHLMONITORREF, CFramebuffer>       m_monitorBGFBs;

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
        bool KHR_display_reference              = false;
        bool IMG_context_priority               = false;
        bool EXT_create_context_robustness      = false;
        bool EGL_ANDROID_native_fence_sync_ext  = false;
    } m_exts;

    SP<CTexture> m_screencopyDeniedTexture;

    enum eEGLContextVersion : uint8_t {
        EGL_CONTEXT_GLES_2_0 = 0,
        EGL_CONTEXT_GLES_3_0,
        EGL_CONTEXT_GLES_3_2,
    };

    eEGLContextVersion m_eglContextVersion = EGL_CONTEXT_GLES_3_2;

  private:
    struct {
        GLint   x      = 0;
        GLint   y      = 0;
        GLsizei width  = 0;
        GLsizei height = 0;
    } m_lastViewport;

    std::unordered_map<int, bool>     m_capStatus;

    std::vector<SDRMFormat>           m_drmFormats;
    bool                              m_hasModifiers = false;

    int                               m_drmFD = -1;
    std::string                       m_extensions;

    bool                              m_fakeFrame            = false;
    bool                              m_applyFinalShader     = false;
    bool                              m_blend                = false;
    bool                              m_offloadedFramebuffer = false;
    bool                              m_cmSupported          = true;

    bool                              m_monitorTransformEnabled = false; // do not modify directly
    std::stack<bool>                  m_monitorTransformStack;
    SP<CTexture>                      m_missingAssetTexture;
    SP<CTexture>                      m_lockDeadTexture;
    SP<CTexture>                      m_lockDead2Texture;
    SP<CTexture>                      m_lockTtyTextTexture;
    SShader                           m_finalScreenShader;
    CTimer                            m_globalTimer;
    GLuint                            m_currentProgram;
    ASP<Hyprgraphics::CImageResource> m_backgroundResource;
    bool                              m_backgroundResourceFailed = false;

    void                              logShaderError(const GLuint&, bool program = false, bool silent = false);
    void                              createBGTextureForMonitor(PHLMONITOR);
    void                              initDRMFormats();
    void                              initEGL(bool gbm);
    EGLDeviceEXT                      eglDeviceFromDRMFD(int drmFD);
    void                              initAssets();
    void                              initMissingAssetTexture();
    void                              requestBackgroundResource();

    //
    std::optional<std::vector<uint64_t>> getModsForFormat(EGLint format);

    // returns the out FB, can be either Mirror or MirrorSwap
    CFramebuffer* blurMainFramebufferWithDamage(float a, CRegion* damage);
    CFramebuffer* blurFramebufferWithDamage(float a, CRegion* damage, CFramebuffer& source);

    void          passCMUniforms(SShader&, const NColorManagement::SImageDescription& imageDescription, const NColorManagement::SImageDescription& targetImageDescription,
                                 bool modifySDR = false, float sdrMinLuminance = -1.0f, int sdrMaxLuminance = -1);
    void          passCMUniforms(SShader&, const NColorManagement::SImageDescription& imageDescription);
    void          renderTexturePrimitive(SP<CTexture> tex, const CBox& box);
    void          renderSplash(cairo_t* const, cairo_surface_t* const, double offset, const Vector2D& size);
    void          renderRectInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
    void          renderRectWithBlurInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
    void          renderRectWithDamageInternal(const CBox&, const CHyprColor&, const SRectRenderData& data);
    void          renderTextureInternal(SP<CTexture>, const CBox&, const STextureRenderData& data);
    void          renderTextureWithBlurInternal(SP<CTexture>, const CBox&, const STextureRenderData& data);

    void          preBlurForCurrentMonitor();

    friend class CHyprRenderer;
    friend class CTexPassElement;
    friend class CPreBlurElement;
    friend class CSurfacePassElement;
};

inline UP<CHyprOpenGLImpl> g_pHyprOpenGL;
