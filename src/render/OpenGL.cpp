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
    m_shQUAD.program = prog;
    m_shQUAD.proj = glGetUniformLocation(prog, "proj");
    m_shQUAD.color = glGetUniformLocation(prog, "color");
    m_shQUAD.posAttrib = glGetAttribLocation(prog, "pos");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA);
    m_shRGBA.program = prog;
    m_shRGBA.proj = glGetUniformLocation(prog, "proj");
    m_shRGBA.tex = glGetUniformLocation(prog, "tex");
    m_shRGBA.alpha = glGetUniformLocation(prog, "alpha");
    m_shRGBA.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_shRGBA.posAttrib = glGetAttribLocation(prog, "pos");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX);
    m_shRGBX.program = prog;
    m_shRGBX.tex = glGetUniformLocation(prog, "tex");
    m_shRGBX.proj = glGetUniformLocation(prog, "proj");
    m_shRGBX.alpha = glGetUniformLocation(prog, "alpha");
    m_shRGBX.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_shRGBX.posAttrib = glGetAttribLocation(prog, "pos");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCEXT);
    m_shEXT.program = prog;
    m_shEXT.tex = glGetUniformLocation(prog, "tex");
    m_shEXT.proj = glGetUniformLocation(prog, "proj");
    m_shEXT.alpha = glGetUniformLocation(prog, "alpha");
    m_shEXT.posAttrib = glGetAttribLocation(prog, "pos");
    m_shEXT.texAttrib = glGetAttribLocation(prog, "texcoord");

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

    wlr_matrix_projection(m_RenderData.projection, pMonitor->vecSize.x, pMonitor->vecSize.y, WL_OUTPUT_TRANSFORM_NORMAL); // TODO: this is deprecated

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
    RASSERT(m_RenderData.pMonitor, "Tried to scissor without begin()!");

    if (!pBox) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    glScissor(pBox->x, pBox->y, pBox->width, pBox->height);
    glEnable(GL_SCISSOR_TEST);
}

void CHyprOpenGLImpl::renderRect(wlr_box* box, const CColor& col) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    float matrix[9];
    wlr_matrix_project_box(matrix, box, WL_OUTPUT_TRANSFORM_NORMAL, 0, m_RenderData.pMonitor->output->transform_matrix);  // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);

    wlr_matrix_transpose(glMatrix, glMatrix);

    if (col.a == 255.f) 
        glDisable(GL_BLEND);
    else
        glEnable(GL_BLEND);

    glUseProgram(m_shQUAD.program);

    glUniformMatrix3fv(m_shQUAD.proj, 1, GL_FALSE, glMatrix);
    glUniform4f(m_shQUAD.color, col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f);

    glVertexAttribPointer(m_shQUAD.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_shQUAD.posAttrib);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(m_shQUAD.posAttrib);
}

void CHyprOpenGLImpl::renderTexture(wlr_texture* tex,float matrix[9], float alpha) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTexture(CTexture(tex), matrix, alpha);
}

void CHyprOpenGLImpl::renderTexture(const CTexture& tex, float matrix[9], float alpha) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);

    wlr_matrix_transpose(glMatrix, glMatrix);

    CShader* shader = nullptr;

    switch (tex.m_iType) {
        case TEXTURE_RGBA:
            shader = &m_shRGBA;
            glEnable(GL_BLEND);
            break;
        case TEXTURE_RGBX:
            shader = &m_shRGBX;
            if (alpha == 255.f)
                glDisable(GL_BLEND);
            break;
        case TEXTURE_EXTERNAL:
            shader = &m_shEXT;
            glEnable(GL_BLEND);
            break;
        default:
            RASSERT(false, "tex.m_iTarget unsupported!");
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex.m_iTarget, tex.m_iTexID);

    glTexParameteri(tex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glUseProgram(shader->program);

    glUniformMatrix3fv(shader->proj, 1, GL_FALSE, glMatrix);
    glUniform1i(shader->tex, 0);
    glUniform1f(shader->alpha, alpha / 255.f);

    glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(shader->posAttrib);
    glEnableVertexAttribArray(shader->texAttrib);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(shader->posAttrib);
    glDisableVertexAttribArray(shader->texAttrib);

    glBindTexture(tex.m_iTarget, 0);
}