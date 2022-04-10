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

struct SCurrentRenderData {
    SMonitor*   pMonitor = nullptr;
    float       projection[9];
};

class CHyprOpenGLImpl {
public:

    CHyprOpenGLImpl();

    void    begin(SMonitor*);
    void    end();

    void    renderRect(wlr_box*, const CColor&);
    void    renderTexture(wlr_texture*, float matrix[9], float a, int round = 0);
    void    renderTexture(const CTexture&, float matrix[9], float a, int round = 0);
    void    renderTextureWithBlur(const CTexture&, float matrix[9], float a, int round = 0);
    void    renderBorder(wlr_box*, const CColor&, int thick = 1, int round = 0);

    void    makeWindowSnapshot(CWindow*);
    void    renderSnapshot(CWindow**);

    void    clear(const CColor&);
    void    clearWithTex();
    void    scissor(const wlr_box*);

    SCurrentRenderData m_RenderData;

    GLint  m_iCurrentOutputFb = 0;
    GLint  m_iWLROutputFb = 0;

    std::unordered_map<CWindow*, CFramebuffer> m_mWindowFramebuffers;
    std::unordered_map<SMonitor*, CFramebuffer> m_mMonitorFramebuffers;
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
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;