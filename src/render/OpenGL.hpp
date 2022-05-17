#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/Color.hpp"
#include <wlr/render/egl.h>
#include <list>
#include <unordered_map>

#include "Shaders.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Framebuffer.hpp"

inline const float matrixFlip180[] = {
	1.0f, 0.0f, 0.0f,
	0.0f, -1.0f, 0.0f,
	0.0f, 0.0f, 1.0f,
};
inline const float fullVerts[] = {
    1, 0,  // top right
    0, 0,  // top left
    1, 1,  // bottom right
    0, 1,  // bottom left
};
inline const float fanVertsFull[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    1.0f, 1.0f,
    -1.0f, 1.0f
};

struct SCurrentRenderData {
    SMonitor*   pMonitor = nullptr;
    float       projection[9];

    pixman_region32_t* pDamage = nullptr;
};

struct SMonitorRenderData {
    CFramebuffer primaryFB;
    CFramebuffer mirrorFB;
    CFramebuffer mirrorSwapFB;

    CTexture     stencilTex;
};

class CHyprOpenGLImpl {
public:

    CHyprOpenGLImpl();

    void    begin(SMonitor*, pixman_region32_t*);
    void    end();

    void    renderRect(wlr_box*, const CColor&, int round = 0);
    void    renderTexture(wlr_texture*, wlr_box*, float a, int round = 0);
    void    renderTexture(const CTexture&, wlr_box*, float a, int round = 0, bool discardOpaque = false, bool border = false);
    void    renderTextureWithBlur(const CTexture&, wlr_box*, float a, wlr_surface* pSurface, int round = 0, bool border = false);

    void    makeWindowSnapshot(CWindow*);
    void    makeLayerSnapshot(SLayerSurface*);
    void    renderSnapshot(CWindow**);
    void    renderSnapshot(SLayerSurface**);

    void    clear(const CColor&);
    void    clearWithTex();
    void    scissor(const wlr_box*);
    void    scissor(const pixman_box32*);
    void    scissor(const int x, const int y, const int w, const int h);

    void    destroyMonitorResources(SMonitor*);

    SCurrentRenderData m_RenderData;

    GLint  m_iCurrentOutputFb = 0;
    GLint  m_iWLROutputFb = 0;

    CWindow* m_pCurrentWindow = nullptr; // hack to get the current rendered window

    pixman_region32_t m_rOriginalDamageRegion; // used for storing the pre-expanded region

    std::unordered_map<CWindow*, CFramebuffer> m_mWindowFramebuffers;
    std::unordered_map<SLayerSurface*, CFramebuffer> m_mLayerFramebuffers;
    std::unordered_map<SMonitor*, SMonitorRenderData> m_mMonitorRenderResources;
    std::unordered_map<SMonitor*, CTexture> m_mMonitorBGTextures;

private:
    std::list<GLuint>       m_lBuffers;
    std::list<GLuint>       m_lTextures;

    int                     m_iDRMFD;
    std::string             m_szExtensions;

    // Shaders
    SQuad                   m_shQUAD;
    CShader                 m_shRGBA;
    CShader                 m_shRGBX;
    CShader                 m_shEXT;
    CShader                 m_shBLUR1;
    CShader                 m_shBLUR2;
    //

    GLuint                  createProgram(const std::string&, const std::string&);
    GLuint                  compileShader(const GLuint&, std::string);
    void                    createBGTextureForMonitor(SMonitor*);

    // returns the out FB, can be either Mirror or MirrorSwap
    CFramebuffer*           blurMainFramebufferWithDamage(float a, wlr_box* pBox, pixman_region32_t* damage);

    void                    renderTextureInternalWithDamage(const CTexture&, wlr_box* pBox, float a, pixman_region32_t* damage, int round = 0, bool discardOpaque = false, bool border = false);
    void                    renderBorder(wlr_box*, const CColor&, int thick = 1, int round = 0);
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;