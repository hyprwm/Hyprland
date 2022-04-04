#include "OpenGL.hpp"
#include "../Compositor.hpp"

CHyprOpenGLImpl::CHyprOpenGLImpl() {
    RASSERT(eglMakeCurrent(g_pCompositor->m_sWLREGL->display, EGL_NO_SURFACE, EGL_NO_SURFACE, g_pCompositor->m_sWLREGL->context), "Couldn't make the EGL current!");

    auto *const EXTENSIONS = (const char*)glGetString(GL_EXTENSIONS);

    RASSERT(EXTENSIONS, "Couldn't retrieve openGL extensions!");

    m_iDRMFD = g_pCompositor->m_iDRMFD;

    m_szExtensions = EXTENSIONS;

    Debug::log(LOG, "Creating the Hypr OpenGL Renderer!");
    Debug::log(LOG, "Using: %s", glGetString(GL_VERSION));
    Debug::log(LOG, "Vendor: %s", glGetString(GL_VENDOR));
    Debug::log(LOG, "Renderer: %s", glGetString(GL_RENDERER));
    Debug::log(LOG, "Supported extensions size: %d", std::count(m_szExtensions.begin(), m_szExtensions.end(), ' '));

    // Init shaders

    GLuint prog = createProgram(QUADVERTSRC, QUADFRAGSRC);
    m_qShaderQuad.program = prog;
    m_qShaderQuad.proj = glGetUniformLocation(prog, "proj");
    m_qShaderQuad.color = glGetUniformLocation(prog, "color");
    m_qShaderQuad.posAttrib = glGetUniformLocation(prog, "pos");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA);
    m_shRGBA.program = prog;
    m_shRGBA.proj = glGetUniformLocation(prog, "proj");
    m_shRGBA.tex = glGetUniformLocation(prog, "tex");
    m_shRGBA.alpha = glGetUniformLocation(prog, "alpha");
    m_shRGBA.texAttrib = glGetUniformLocation(prog, "texcoord");
    m_shRGBA.posAttrib = glGetUniformLocation(prog, "pos");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX);
    m_shRGBX.program = prog;
    m_shRGBX.tex = glGetUniformLocation(prog, "tex");
    m_shRGBX.proj = glGetUniformLocation(prog, "proj");
    m_shRGBX.alpha = glGetUniformLocation(prog, "alpha");
    m_shRGBX.texAttrib = glGetUniformLocation(prog, "texcoord");
    m_shRGBX.posAttrib = glGetUniformLocation(prog, "pos");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCEXT);
    m_shEXT.program = prog;
    m_shEXT.tex = glGetUniformLocation(prog, "tex");
    m_shEXT.proj = glGetUniformLocation(prog, "proj");
    m_shEXT.alpha = glGetUniformLocation(prog, "alpha");
    m_shEXT.posAttrib = glGetUniformLocation(prog, "pos");
    m_shEXT.texAttrib = glGetUniformLocation(prog, "texcoord");

    Debug::log(LOG, "Shaders initialized successfully.");

    // End shaders

    RASSERT(eglMakeCurrent(g_pCompositor->m_sWLREGL->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "Couldn't unset current EGL!");

    // Done!
}

GLuint CHyprOpenGLImpl::createProgram(const std::string& vert, const std::string& frag) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert);
    RASSERT(vertCompiled, "Compiling shader failed. VERTEX NULL! Shader source:\n\n%s", vert.c_str());

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag);
    RASSERT(fragCompiled, "Compiling shader failed. FRAGMENT NULL! Shader source:\n\n%s", frag.c_str());

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    RASSERT(ok != GL_FALSE, "createProgram() failed! GL_LINK_STATUS not OK!");

    return prog;
}

GLuint CHyprOpenGLImpl::compileShader(const GLuint& type, std::string src) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    RASSERT(ok != GL_FALSE, "compileShader() failed! GL_COMPILE_STATUS not OK!");

    return shader;
}

void CHyprOpenGLImpl::begin(SMonitor* pMonitor) {
    m_RenderData.pMonitor = pMonitor;

    glViewport(0, 0, pMonitor->vecSize.x, pMonitor->vecSize.y);

    wlr_matrix_projection(m_RenderData.projection, pMonitor->vecSize.x, pMonitor->vecSize.y, WL_OUTPUT_TRANSFORM_NORMAL);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void CHyprOpenGLImpl::end() {
    m_RenderData.pMonitor = nullptr;
}

void CHyprOpenGLImpl::clear(const CColor& color) {
    RASSERT(m_RenderData.pMonitor, "Tried to render without begin()!");

    glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void CHyprOpenGLImpl::scissor(const wlr_box* pBox) {
    if (!pBox) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    glScissor(pBox->x, pBox->y, pBox->width, pBox->height);
    glEnable(GL_SCISSOR_TEST);
}