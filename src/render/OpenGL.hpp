#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/Color.hpp"
#include <wlr/render/egl.h>
#include <list>

#include "Shaders.hpp"
#include "Shader.hpp"
#include "Texture.hpp"

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

    void    clear(const CColor&);
    void    scissor(const wlr_box*);

    SCurrentRenderData m_RenderData;

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
    //

    GLuint                  createProgram(const std::string&, const std::string&);
    GLuint                  compileShader(const GLuint&, std::string);
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;