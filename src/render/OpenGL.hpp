#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/Color.hpp"
#include "../helpers/Timer.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/Format.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include <cstdint>
#include <list>
#include <unordered_map>
#include <map>

#include <cairo/cairo.h>

#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"
#include "Transformer.hpp"
#include "Renderbuffer.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <aquamarine/buffer/Buffer.hpp>

#include "../debug/TracyDefines.hpp"

struct gbm_device;
class CHyprRenderer;

inline const float fullVerts[] = {
    1, 0, // top right
    0, 0, // top left
    1, 1, // bottom right
    0, 1, // bottom left
};
inline const float fanVertsFull[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

enum eDiscardMode {
    DISCARD_OPAQUE = 1,
    DISCARD_ALPHA  = 1 << 1
};

struct SRenderModifData {
    enum eRenderModifType {
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

struct SMonitorRenderData {
    CFramebuffer offloadFB;
    CFramebuffer mirrorFB;     // these are used for some effects,
    CFramebuffer mirrorSwapFB; // etc
    CFramebuffer offMainFB;

    CFramebuffer monitorMirrorFB; // used for mirroring outputs, does not contain artifacts like offloadFB

    SP<CTexture> stencilTex = makeShared<CTexture>();

    CFramebuffer blurFB;
    bool         blurFBDirty        = true;
    bool         blurFBShouldRender = false;

    // Shaders
    bool    m_bShadersInitialized = false;
    CShader m_shQUAD;
    CShader m_shRGBA;
    CShader m_shPASSTHRURGBA;
    CShader m_shMATTE;
    CShader m_shRGBX;
    CShader m_shEXT;
    CShader m_shBLUR1;
    CShader m_shBLUR2;
    CShader m_shBLURPREPARE;
    CShader m_shBLURFINISH;
    CShader m_shSHADOW;
    CShader m_shBORDER1;
    CShader m_shGLITCH;
    //
};

struct SCurrentRenderData {
    PHLMONITORREF       pMonitor;
    PHLWORKSPACE        pWorkspace = nullptr;
    Mat3x3              projection;
    Mat3x3              savedProjection;
    Mat3x3              monitorProjection;

    SMonitorRenderData* pCurrentMonData = nullptr;
    CFramebuffer*       currentFB       = nullptr; // current rendering to
    CFramebuffer*       mainFB          = nullptr; // main to render to
    CFramebuffer*       outFB           = nullptr; // out to render to (if offloaded, etc)

    CRegion             damage;
    CRegion             finalDamage; // damage used for funal off -> main

    SRenderModifData    renderModif;
    float               mouseZoomFactor    = 1.f;
    bool                mouseZoomUseMouse  = true; // true by default
    bool                useNearestNeighbor = false;
    bool                forceIntrospection = false; // cleaned in ::end()
    bool                blockScreenShader  = false;
    bool                simplePass         = false;

    Vector2D            primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    Vector2D            primarySurfaceUVBottomRight = Vector2D(-1, -1);

    CBox                clipBox = {}; // scaled coordinates

    uint32_t            discardMode    = DISCARD_OPAQUE;
    float               discardOpacity = 0.f;
};

class CEGLSync {
  public:
    ~CEGLSync();

    EGLSyncKHR sync = nullptr;

    int        fd();
    bool       wait();

  private:
    CEGLSync() = default;

    CFileDescriptor m_iFd;

    friend class CHyprOpenGLImpl;
};

class CGradientValueData;

class CHyprOpenGLImpl {
  public:
    CHyprOpenGLImpl();
    ~CHyprOpenGLImpl();

    void     begin(PHLMONITOR, const CRegion& damage, CFramebuffer* fb = nullptr, std::optional<CRegion> finalDamage = {});
    void     beginSimple(PHLMONITOR, const CRegion& damage, SP<CRenderbuffer> rb = nullptr, CFramebuffer* fb = nullptr);
    void     end();

    void     renderRect(CBox*, const CColor&, int round = 0);
    void     renderRectWithBlur(CBox*, const CColor&, int round = 0, float blurA = 1.f, bool xray = false);
    void     renderRectWithDamage(CBox*, const CColor&, CRegion* damage, int round = 0);
    void     renderTexture(SP<CTexture>, CBox*, float a, int round = 0, bool discardActive = false, bool allowCustomUV = false);
    void     renderTextureWithDamage(SP<CTexture>, CBox*, CRegion* damage, float a, int round = 0, bool discardActive = false, bool allowCustomUV = false,
                                     SP<CSyncTimeline> waitTimeline = nullptr, uint64_t waitPoint = 0);
    void     renderTextureWithBlur(SP<CTexture>, CBox*, float a, SP<CWLSurfaceResource> pSurface, int round = 0, bool blockBlurOptimization = false, float blurA = 1.f);
    void     renderRoundedShadow(CBox*, int round, int range, const CColor& color, float a = 1.0);
    void     renderBorder(CBox*, const CGradientValueData&, int round, int borderSize, float a = 1.0, int outerRound = -1 /* use round */);
    void     renderTextureMatte(SP<CTexture> tex, CBox* pBox, CFramebuffer& matte);

    void     setMonitorTransformEnabled(bool enabled);
    void     setRenderModifEnabled(bool enabled);

    void     saveMatrix();
    void     setMatrixScaleTranslate(const Vector2D& translate, const float& scale);
    void     restoreMatrix();

    void     blend(bool enabled);

    void     makeWindowSnapshot(PHLWINDOW);
    void     makeRawWindowSnapshot(PHLWINDOW, CFramebuffer*);
    void     makeLayerSnapshot(PHLLS);
    void     renderSnapshot(PHLWINDOW);
    void     renderSnapshot(PHLLS);
    bool     shouldUseNewBlurOptimizations(PHLLS pLayer, PHLWINDOW pWindow);

    void     clear(const CColor&);
    void     clearWithTex();
    void     scissor(const CBox*, bool transform = true);
    void     scissor(const pixman_box32*, bool transform = true);
    void     scissor(const int x, const int y, const int w, const int h, bool transform = true);

    void     destroyMonitorResources(PHLMONITOR);

    void     markBlurDirtyForMonitor(PHLMONITOR);

    void     preWindowPass();
    bool     preBlurQueued();
    void     preRender(PHLMONITOR);

    void     saveBufferForMirror(CBox*);
    void     renderMirrored();

    void     applyScreenShader(const std::string& path);

    void     bindOffMain();
    void     renderOffToMain(CFramebuffer* off);
    void     bindBackOnMain();

    void     setDamage(const CRegion& damage, std::optional<CRegion> finalDamage = {});

    uint32_t getPreferredReadFormat(PHLMONITOR pMonitor);
    std::vector<SDRMFormat>                     getDRMFormats();
    EGLImageKHR                                 createEGLImage(const Aquamarine::SDMABUFAttrs& attrs);
    SP<CEGLSync>                                createEGLSync(CFileDescriptor fenceFD = {});
    bool                                        waitForTimelinePoint(SP<CSyncTimeline> timeline, uint64_t point);

    SCurrentRenderData                          m_RenderData;

    GLint                                       m_iCurrentOutputFb = 0;

    CFileDescriptor                             m_iGBMFD;
    gbm_device*                                 m_pGbmDevice   = nullptr;
    EGLContext                                  m_pEglContext  = nullptr;
    EGLDisplay                                  m_pEglDisplay  = nullptr;
    EGLDeviceEXT                                m_pEglDevice   = nullptr;
    uint                                        failedAssetsNo = 0;

    bool                                        m_bReloadScreenShader = true; // at launch it can be set

    PHLWINDOWREF                                m_pCurrentWindow; // hack to get the current rendered window
    PHLLS                                       m_pCurrentLayer;  // hack to get the current rendered layer

    std::map<PHLWINDOWREF, CFramebuffer>        m_mWindowFramebuffers;
    std::map<PHLLSREF, CFramebuffer>            m_mLayerFramebuffers;
    std::map<PHLMONITORREF, SMonitorRenderData> m_mMonitorRenderResources;
    std::map<PHLMONITORREF, CFramebuffer>       m_mMonitorBGFBs;

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
    } m_sProc;

    struct {
        bool EXT_read_format_bgra               = false;
        bool EXT_image_dma_buf_import           = false;
        bool EXT_image_dma_buf_import_modifiers = false;
        bool KHR_display_reference              = false;
        bool IMG_context_priority               = false;
        bool EXT_create_context_robustness      = false;
    } m_sExts;

  private:
    std::list<GLuint>       m_lBuffers;
    std::list<GLuint>       m_lTextures;

    std::vector<SDRMFormat> drmFormats;
    bool                    m_bHasModifiers = false;

    int                     m_iDRMFD = -1;
    std::string             m_szExtensions;

    bool                    m_bFakeFrame            = false;
    bool                    m_bEndFrame             = false;
    bool                    m_bApplyFinalShader     = false;
    bool                    m_bBlend                = false;
    bool                    m_bOffloadedFramebuffer = false;

    CShader                 m_sFinalScreenShader;
    CTimer                  m_tGlobalTimer;

    SP<CTexture>            m_pMissingAssetTexture, m_pBackgroundTexture, m_pLockDeadTexture, m_pLockDead2Texture, m_pLockTtyTextTexture;

    void                    logShaderError(const GLuint&, bool program = false);
    GLuint                  createProgram(const std::string&, const std::string&, bool dynamic = false);
    GLuint                  compileShader(const GLuint&, std::string, bool dynamic = false);
    void                    createBGTextureForMonitor(PHLMONITOR);
    void                    initShaders();
    void                    initDRMFormats();
    void                    initEGL(bool gbm);
    EGLDeviceEXT            eglDeviceFromDRMFD(int drmFD);
    SP<CTexture>            loadAsset(const std::string& file);
    SP<CTexture>            renderText(const std::string& text, CColor col, int pt, bool italic = false);
    void                    initAssets();
    void                    initMissingAssetTexture();

    //
    std::optional<std::vector<uint64_t>> getModsForFormat(EGLint format);

    // returns the out FB, can be either Mirror or MirrorSwap
    CFramebuffer* blurMainFramebufferWithDamage(float a, CRegion* damage);

    void          renderTextureInternalWithDamage(SP<CTexture>, CBox* pBox, float a, CRegion* damage, int round = 0, bool discardOpaque = false, bool noAA = false,
                                                  bool allowCustomUV = false, bool allowDim = false, SP<CSyncTimeline> = nullptr, uint64_t waitPoint = 0);
    void          renderTexturePrimitive(SP<CTexture> tex, CBox* pBox);
    void          renderSplash(cairo_t* const, cairo_surface_t* const, double offset, const Vector2D& size);

    void          preBlurForCurrentMonitor();

    bool          passRequiresIntrospection(PHLMONITOR pMonitor);

    friend class CHyprRenderer;
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;
