#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/Color.hpp"
#include "../helpers/Timer.hpp"
#include "../helpers/Region.hpp"
#include <list>
#include <unordered_map>

#include <cairo/cairo.h>

#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"
#include "Transformer.hpp"
#include "Renderbuffer.hpp"

#include <GLES2/gl2ext.h>

#include "../debug/TracyDefines.hpp"

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
};

struct SGLPixelFormat {
    uint32_t drmFormat        = DRM_FORMAT_INVALID;
    GLint    glInternalFormat = 0;
    GLint    glFormat         = 0;
    GLint    glType           = 0;
    bool     withAlpha        = false;
};

struct SMonitorRenderData {
    CFramebuffer offloadFB;
    CFramebuffer mirrorFB;     // these are used for some effects,
    CFramebuffer mirrorSwapFB; // etc
    CFramebuffer offMainFB;

    CFramebuffer monitorMirrorFB; // used for mirroring outputs

    CTexture     stencilTex;

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
    CMonitor*           pMonitor   = nullptr;
    CWorkspace*         pWorkspace = nullptr;
    float               projection[9];
    float               savedProjection[9];

    SMonitorRenderData* pCurrentMonData = nullptr;
    CFramebuffer*       currentFB       = nullptr; // current rendering to
    CFramebuffer*       mainFB          = nullptr; // main to render to
    CFramebuffer*       outFB           = nullptr; // out to render to (if offloaded, etc)

    CRegion             damage;

    SRenderModifData    renderModif;
    float               mouseZoomFactor    = 1.f;
    bool                mouseZoomUseMouse  = true; // true by default
    bool                useNearestNeighbor = false;
    bool                forceIntrospection = false; // cleaned in ::end()

    Vector2D            primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    Vector2D            primarySurfaceUVBottomRight = Vector2D(-1, -1);

    CBox                clipBox = {};

    uint32_t            discardMode    = DISCARD_OPAQUE;
    float               discardOpacity = 0.f;
};

class CGradientValueData;

class CHyprOpenGLImpl {
  public:
    CHyprOpenGLImpl();

    void     begin(CMonitor*, CRegion*, CFramebuffer* fb = nullptr /* if provided, it's not a real frame */);
    void     end();

    void     renderRect(CBox*, const CColor&, CCornerRadiiData radii = 0);
    void     renderRectWithBlur(CBox*, const CColor&, CCornerRadiiData radii = 0, float blurA = 1.f, bool xray = false);
    void     renderRectWithDamage(CBox*, const CColor&, CRegion* damage, CCornerRadiiData radii = 0);
    void     renderTexture(wlr_texture*, CBox*, float a, CCornerRadiiData radii = 0, bool allowCustomUV = false);
    void     renderTexture(const CTexture&, CBox*, float a, CCornerRadiiData radii = 0, bool discardActive = false, bool allowCustomUV = false);
    void     renderTextureWithBlur(const CTexture&, CBox*, float a, wlr_surface* pSurface, CCornerRadiiData radii = 0, bool blockBlurOptimization = false, float blurA = 1.f);
    void     renderRoundedShadow(CBox*, CCornerRadiiData radii, int range, const CColor& color, float a = 1.0);
    void     renderBorder(CBox*, const CGradientValueData&, CCornerRadiiData radii, int borderSize, float a = 1.0, CCornerRadiiData outerRadii = -1 /* use round */);
    void     renderTextureMatte(const CTexture& tex, CBox* pBox, CFramebuffer& matte);

    void     setMonitorTransformEnabled(bool enabled);

    void     saveMatrix();
    void     setMatrixScaleTranslate(const Vector2D& translate, const float& scale);
    void     restoreMatrix();

    void     blend(bool enabled);

    void     makeWindowSnapshot(CWindow*);
    void     makeRawWindowSnapshot(CWindow*, CFramebuffer*);
    void     makeLayerSnapshot(SLayerSurface*);
    void     renderSnapshot(CWindow**);
    void     renderSnapshot(SLayerSurface**);
    bool     shouldUseNewBlurOptimizations(SLayerSurface* pLayer, CWindow* pWindow);

    void     clear(const CColor&);
    void     clearWithTex();
    void     scissor(const CBox*, bool transform = true);
    void     scissor(const pixman_box32*, bool transform = true);
    void     scissor(const int x, const int y, const int w, const int h, bool transform = true);

    void     destroyMonitorResources(CMonitor*);

    void     markBlurDirtyForMonitor(CMonitor*);

    void     preWindowPass();
    bool     preBlurQueued();
    void     preRender(CMonitor*);

    void     saveBufferForMirror();
    void     renderMirrored();

    void     applyScreenShader(const std::string& path);

    void     bindOffMain();
    void     renderOffToMain(CFramebuffer* off);
    void     bindBackOnMain();

    uint32_t getPreferredReadFormat(CMonitor* pMonitor);
    const SGLPixelFormat*                             getPixelFormatFromDRM(uint32_t drmFormat);

    SCurrentRenderData                                m_RenderData;

    GLint                                             m_iCurrentOutputFb = 0;

    bool                                              m_bReloadScreenShader = true; // at launch it can be set

    CWindow*                                          m_pCurrentWindow = nullptr; // hack to get the current rendered window
    SLayerSurface*                                    m_pCurrentLayer  = nullptr; // hack to get the current rendered layer

    std::unordered_map<CWindow*, CFramebuffer>        m_mWindowFramebuffers;
    std::unordered_map<SLayerSurface*, CFramebuffer>  m_mLayerFramebuffers;
    std::unordered_map<CMonitor*, SMonitorRenderData> m_mMonitorRenderResources;
    std::unordered_map<CMonitor*, CFramebuffer>       m_mMonitorBGFBs;

    struct {
        PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES = nullptr;
        PFNEGLDESTROYIMAGEKHRPROC                     eglDestroyImageKHR                     = nullptr;
    } m_sProc;

    struct {
        bool EXT_read_format_bgra = false;
    } m_sExts;

  private:
    std::list<GLuint> m_lBuffers;
    std::list<GLuint> m_lTextures;

    int               m_iDRMFD;
    std::string       m_szExtensions;

    bool              m_bFakeFrame            = false;
    bool              m_bEndFrame             = false;
    bool              m_bApplyFinalShader     = false;
    bool              m_bBlend                = false;
    bool              m_bOffloadedFramebuffer = false;

    CShader           m_sFinalScreenShader;
    CTimer            m_tGlobalTimer;

    GLuint            createProgram(const std::string&, const std::string&, bool dynamic = false);
    GLuint            compileShader(const GLuint&, std::string, bool dynamic = false);
    void              createBGTextureForMonitor(CMonitor*);
    void              initShaders();

    // returns the out FB, can be either Mirror or MirrorSwap
    CFramebuffer* blurMainFramebufferWithDamage(float a, CRegion* damage);

    void          renderTextureInternalWithDamage(const CTexture&, CBox* pBox, float a, CRegion* damage, CCornerRadiiData radii = 0, bool discardOpaque = false, bool noAA = false,
                                                  bool allowCustomUV = false, bool allowDim = false);
    void          renderTexturePrimitive(const CTexture& tex, CBox* pBox);
    void          renderSplash(cairo_t* const, cairo_surface_t* const, double offset, const Vector2D& size);

    void          preBlurForCurrentMonitor();

    bool          passRequiresIntrospection(CMonitor* pMonitor);

    friend class CHyprRenderer;
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;
