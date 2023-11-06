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

#include "../debug/TracyDefines.hpp"

class CHyprRenderer;

inline const float fullVerts[] = {
    1, 0, // top right
    0, 0, // top left
    1, 1, // bottom right
    0, 1, // bottom left
};
inline const float fanVertsFull[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};

enum eDiscardMode
{
    DISCARD_OPAQUE = 1,
    DISCARD_ALPHA  = 1 << 1
};

struct SRenderModifData {
    Vector2D translate = {};
    float    scale     = 1.f;
};

struct SMonitorRenderData {
    CFramebuffer primaryFB;
    CFramebuffer mirrorFB;     // these are used for some effects,
    CFramebuffer mirrorSwapFB; // etc
    CFramebuffer offMainFB;

    CFramebuffer monitorMirrorFB; // used for mirroring outputs

    CTexture     stencilTex;

    CFramebuffer blurFB;
    bool         blurFBDirty        = true;
    bool         blurFBShouldRender = false;

    CBox         backgroundTexBox;

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
    CFramebuffer*       currentFB       = nullptr;

    CRegion             damage;

    SRenderModifData    renderModif;
    float               mouseZoomFactor    = 1.f;
    bool                mouseZoomUseMouse  = true; // true by default
    bool                useNearestNeighbor = false;

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

    void               begin(CMonitor*, CRegion*, bool fake = false);
    void               end();
    void               bindWlrOutputFb();

    void               renderRect(CBox*, const CColor&, int round = 0);
    void               renderRectWithBlur(CBox*, const CColor&, int round = 0, float blurA = 1.f);
    void               renderRectWithDamage(CBox*, const CColor&, CRegion* damage, int round = 0);
    void               renderTexture(wlr_texture*, CBox*, float a, int round = 0, bool allowCustomUV = false);
    void               renderTexture(const CTexture&, CBox*, float a, int round = 0, bool discardActive = false, bool allowCustomUV = false);
    void               renderTextureWithBlur(const CTexture&, CBox*, float a, wlr_surface* pSurface, int round = 0, bool blockBlurOptimization = false, float blurA = 1.f);
    void               renderRoundedShadow(CBox*, int round, int range, float a = 1.0);
    void               renderBorder(CBox*, const CGradientValueData&, int round, int borderSize, float a = 1.0, int outerRound = -1 /* use round */);
    void               renderTextureMatte(const CTexture& tex, CBox* pBox, CFramebuffer& matte);

    void               setMonitorTransformEnabled(bool enabled);

    void               saveMatrix();
    void               setMatrixScaleTranslate(const Vector2D& translate, const float& scale);
    void               restoreMatrix();

    void               blend(bool enabled);

    void               makeWindowSnapshot(CWindow*);
    void               makeRawWindowSnapshot(CWindow*, CFramebuffer*);
    void               makeLayerSnapshot(SLayerSurface*);
    void               renderSnapshot(CWindow**);
    void               renderSnapshot(SLayerSurface**);

    void               clear(const CColor&);
    void               clearWithTex();
    void               scissor(const CBox*, bool transform = true);
    void               scissor(const pixman_box32*, bool transform = true);
    void               scissor(const int x, const int y, const int w, const int h, bool transform = true);

    void               destroyMonitorResources(CMonitor*);

    void               markBlurDirtyForMonitor(CMonitor*);

    void               preWindowPass();
    bool               preBlurQueued();
    void               preRender(CMonitor*);

    void               saveBufferForMirror();
    void               renderMirrored();

    void               applyScreenShader(const std::string& path);

    void               bindOffMain();
    void               renderOffToMain(CFramebuffer* off);
    void               bindBackOnMain();

    SCurrentRenderData m_RenderData;

    GLint              m_iCurrentOutputFb = 0;
    GLint              m_iWLROutputFb     = 0;

    bool               m_bReloadScreenShader = true; // at launch it can be set

    CWindow*           m_pCurrentWindow = nullptr; // hack to get the current rendered window
    SLayerSurface*     m_pCurrentLayer  = nullptr; // hack to get the current rendered layer

    std::unordered_map<CWindow*, CFramebuffer>        m_mWindowFramebuffers;
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
    bool              m_bBlend            = false;

    CShader           m_sFinalScreenShader;
    CTimer            m_tGlobalTimer;

    GLuint            createProgram(const std::string&, const std::string&, bool dynamic = false);
    GLuint            compileShader(const GLuint&, std::string, bool dynamic = false);
    void              createBGTextureForMonitor(CMonitor*);
    void              initShaders();

    // returns the out FB, can be either Mirror or MirrorSwap
    CFramebuffer* blurMainFramebufferWithDamage(float a, CRegion* damage);

    void          renderTextureInternalWithDamage(const CTexture&, CBox* pBox, float a, CRegion* damage, int round = 0, bool discardOpaque = false, bool noAA = false,
                                                  bool allowCustomUV = false, bool allowDim = false);
    void          renderTexturePrimitive(const CTexture& tex, CBox* pBox);
    void          renderSplash(cairo_t* const, cairo_surface_t* const, double);

    void          preBlurForCurrentMonitor();

    bool          shouldUseNewBlurOptimizations(SLayerSurface* pLayer, CWindow* pWindow);

    friend class CHyprRenderer;
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;
