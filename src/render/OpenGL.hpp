#pragma once

#include "../defines.hpp"
#include "../helpers/Monitor.hpp"
#include "../helpers/Color.hpp"
#include <wlr/render/egl.h>
#include <list>

#include "Shaders.hpp"
#include "Shader.hpp"

struct SCurrentRenderData {
    SMonitor*   pMonitor = nullptr;
    float       projection[9];
};

class CHyprOpenGLImpl {
public:

    CHyprOpenGLImpl();

    void    begin(SMonitor*);
    void    end();

    void    clear(const CColor&);
    void    scissor(const wlr_box*);

    SCurrentRenderData m_RenderData;

private:
    std::list<GLuint>       m_lBuffers;
    std::list<GLuint>       m_lTextures;

    int                     m_iDRMFD;
    std::string             m_szExtensions;

    // Shaders
    SQuad                   m_qShaderQuad;
    CShader                 m_shRGBA;
    CShader                 m_shRGBX;
    CShader                 m_shEXT;
    //

    GLuint                  createProgram(const std::string&, const std::string&);
    GLuint                  compileShader(const GLuint&, std::string);
};

inline std::unique_ptr<CHyprOpenGLImpl> g_pHyprOpenGL;