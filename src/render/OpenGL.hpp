#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/Color.hpp"
#include "../helpers/Timer.hpp"
#include <list>
#include <unordered_map>

#include <cairo/cairo.h>

#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"

class CHyprRenderer;

inline const float fullVerts[] = {
    1, 0, // top right
    0, 0, // top left
    1, 1, // bottom right
    0, 1, // bottom left
};
inline const float fanVertsFull[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

enum eDiscardMode {
    DISCARD_OPAQUE    = 1,
    DISCARD_ALPHAZERO = 1 << 1
};

struct SMonitorRenderData {
    CFramebuffer primaryFB;
    CFramebuffer mirrorFB;     // these are used for some effects,
    CFramebuffer mirrorSwapFB; // etc

    CFramebuffer monitorMirrorFB; // used for mirroring outputs

    CTexture     stencilTex;

    CFramebuffer blurFB;
    bool         blurFBDirty        = true;
    bool         blurFBShouldRender = false;

    wlr_box      backgroundTexBox;

    // Shaders
    bool    m_bShadersInitialized = false;
    CShader m_shQUAD;
    CShader m_shRGBA;
    CShader m_shPASSTHRURGBA;
    CShader m_shRGBX;
    CShader m_shEXT;
    CShader m_shBLUR1;
    CShader m_shBLUR2;
    CShader m_shSHADOW;
    CShader m_shBORDER1;
    CShader m_shGLITCH;
    //
};

struct SCurrentRenderData {
    CMonitor*           pMonitor = nullptr;
    float               projection[9];

    SMonitorRenderData* pCurrentMonData = nullptr;

    pixman_region32_t*  pDamage = nullptr;

    Vector2D            primarySurfaceUVTopLeft     = Vector2D(-1, -1);
    Vector2D            primarySurfaceUVBottomRight = Vector2D(-1, -1);

    wlr_box             clipBox = {};

    uint32_t            discardMode = DISCARD_OPAQUE;
};

class CGradientValueData;

class CHyprOpenGLImpl {
  public:
    CHyprOpenGLImpl();

    void                                       begin(CMonitor*, pixman_region32_t*, bool fake = false);
    void                                       end();

    void                                       renderRect(wlr_box*, const CColor&, int round = 0);
    void                                       renderRectWithDamage(wlr_box*, const CColor&, pixman_region32_t* damage, int round = 0);
    void                                       renderTexture(wlr_texture*, wlr_box*, float a, int round = 0, bool allowCustomUV = false);
    void                                       renderTexture(const CTexture&, wlr_box*, float a, int round = 0, bool discardActive = false, bool allowCustomUV = false);
    void                                       renderTextureWithBlur(const CTexture&, wlr_box*, float a, wlr_surface* pSurface, int round = 0, bool blockBlurOptimization = false);
    void                                       renderRoundedShadow(wlr_box*, int round, int range, float a = 1.0);
    void                                       renderBorder(wlr_box*, const CGradientValueData&, int round, float a = 1.0);

    void                                       makeWindowSnapshot(CWindow*);
    void                                       makeRawWindowSnapshot(CWindow*, CFramebuffer*);
    void                                       makeLayerSnapshot(SLayerSurface*);
    void                                       renderSnapshot(CWindow**);
    void                                       renderSnapshot(SLayerSurface**);

    void                                       clear(const CColor&);
    void                                       clearWithTex();
    void                                       scissor(const wlr_box*, bool transform = true);
    void                                       scissor(const pixman_box32*, bool transform = true);
    void                                       scissor(const int x, const int y, const int w, const int h, bool transform = true);

    void                                       destroyMonitorResources(CMonitor*);

    void                                       markBlurDirtyForMonitor(CMonitor*);

    void                                       preWindowPass();
    void                                       preRender(CMonitor*);

    void                                       saveBufferForMirror();
    void                                       renderMirrored();

    void                                       applyScreenShader(const std::string& path);

    SCurrentRenderData                         m_RenderData;

    GLint                                      m_iCurrentOutputFb = 0;
    GLint                                      m_iWLROutputFb     = 0;

    bool                                       m_bReloadScreenShader = true; // at launch it can be set

    CWindow*                                   m_pCurrentWindow = nullptr; // hack to get the current rendered window

    pixman_region32_t                          m_rOriginalDamageRegion; // used for storing the pre-expanded region

    std::unordered_map<CWindow*, CFramebuffer> m_mWindowFramebuffers;
    std::unordered_map<SLayerSurface*, CFramebuffer>  m_mLayerFramebuffers;
    std::unordered_map<CMonitor*, SMonitorRenderData> m_mMonitorRenderResources;
    std::unordered_map<CMonitor*, CTexture>           m_mMonitorBGTextures;

  private:
    std::list<GLuint> m_lBuffers;
    std::list<GLuint> m_lTextures;

    int               m_iDRMFD;
    std::string       m_szExtensions;

    bool              m_bFakeFrame        = false;
    bool              m_bEndFrame         = false;
    bool              m_bApplyFinalShader = false;

    CShader           m_sFinalScreenShader;
    CTimer            m_tGlobalTimer;

    GLuint            createProgram(const std::string&, const std::string&, bool dynamic = false);
    GLuint            compileShader(const GLuint&, std::string, bool dynamic = false);
    void              createBGTextureForMonitor(CMonitor*);
    void              initShaders();

    // returns the out FB, can be either Mirror or MirrorSwap
    CFramebuffer* blurMainFramebufferWithDamage(float a, wlr_box* pBox, pixman_region32_t* damage);

    void          renderTextureInternalWithDamage(const CTexture&, wlr_box* pBox, float a, pixman_region32_t* damage, int round = 0, bool discardOpaque = false, bool noAA = false,
                                                  bool allowCustomUV = false, bool allowDim = false);
    void          renderSplash(cairo_t* const, cairo_surface_t* const, double);

    void          preBlurForCurrentMonitor();

    friend class CHyprRenderer;
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;
