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

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_iCurrentOutputFb);
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

void CHyprOpenGLImpl::renderTexture(wlr_texture* tex,float matrix[9], float alpha, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTexture(CTexture(tex), matrix, alpha, round);
}

void CHyprOpenGLImpl::renderTexture(const CTexture& tex, float matrix[9], float alpha, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);

    wlr_matrix_transpose(glMatrix, glMatrix);

    CShader* shader = nullptr;

    glEnable(GL_BLEND);

    switch (tex.m_iType) {
        case TEXTURE_RGBA:
            shader = &m_shRGBA;
            break;
        case TEXTURE_RGBX:
            shader = &m_shRGBX;
            break;
        case TEXTURE_EXTERNAL:
            shader = &m_shEXT;
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

    // round is in px
    // so we need to do some maf

    const auto TOPLEFT = Vector2D(round, round);
    const auto BOTTOMRIGHT = Vector2D(tex.m_vSize.x - round, tex.m_vSize.y - round);
    const auto FULLSIZE = tex.m_vSize;

    // Rounded corners
    glUniform2f(glGetUniformLocation(shader->program, "topLeft"), (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(glGetUniformLocation(shader->program, "bottomRight"), (float)BOTTOMRIGHT.x, (float)BOTTOMRIGHT.y);
    glUniform2f(glGetUniformLocation(shader->program, "fullSize"), (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(glGetUniformLocation(shader->program, "radius"), round);

    glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(shader->posAttrib);
    glEnableVertexAttribArray(shader->texAttrib);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(shader->posAttrib);
    glDisableVertexAttribArray(shader->texAttrib);

    glBindTexture(tex.m_iTarget, 0);
}

void pushVert2D(float x, float y, float* arr, int& counter, wlr_box* box) {
    // 0-1 space god damnit
    arr[counter * 2 + 0] = x / box->width;
    arr[counter * 2 + 1] = y / box->height;
    counter++;
}

void CHyprOpenGLImpl::renderBorder(wlr_box* box, const CColor& col, int thick, int radius) {
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

    // Sides are ONLY for corners meaning they have to be divisible by 4.
    // 32 sides shouldn't be taxing at all on the performance.
    const int SIDES = 32;  // sides
    const int SIDES34 = 24; // 3/4th of the sides
    float verts[(SIDES + 8 + 1) * 4]; // 8 for the connections and 1 because last is doubled (begin/end)
    int vertNo = 0;

    // start from 0,0 tex coord space
    float x = 0, y = 0, w = box->width, h = box->height;

    pushVert2D(x + radius, y + h, verts, vertNo, box);
    pushVert2D(x + w - radius, y + h, verts, vertNo, box);

    float x1 = x + w - radius;
    float y1 = y + h - radius;

    for (int i = 0; i <= SIDES / 4; i++) {
        pushVert2D(x1 + (sin((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), y1 + (cos((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), verts, vertNo, box);
    }

    // Right Line
    pushVert2D(x + w, y + radius, verts, vertNo, box);

    x1 = x + w - radius;
    y1 = y + radius;

    for (int i = SIDES / 4; i <= SIDES / 2; i++) {
        pushVert2D(x1 + (sin((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), y1 + (cos((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), verts, vertNo, box);
    }

    // Top Line
    pushVert2D(x + radius, y, verts, vertNo, box);

    x1 = x + radius;
    y1 = y + radius;

    for (int i = SIDES / 2; i <= SIDES34; i++) {
        pushVert2D(x1 + (sin((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), y1 + (cos((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), verts, vertNo, box);
    }

    // Left Line
    pushVert2D(x, y + h - radius, verts, vertNo, box);

    x1 = x + radius;
    y1 = y + h - radius;

    for (int i = SIDES34; i <= SIDES; i++) {
        pushVert2D(x1 + (sin((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), y1 + (cos((i * (360.f / (float)SIDES) * 3.141526f / 180)) * radius), verts, vertNo, box);
    }

    glVertexAttribPointer(m_shQUAD.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, verts);

    glEnableVertexAttribArray(m_shQUAD.posAttrib);

    glLineWidth(thick);
    glDrawArrays(GL_LINE_STRIP, 0, 41);

    glDisableVertexAttribArray(m_shQUAD.posAttrib);
}

void CHyprOpenGLImpl::makeWindowSnapshot(CWindow* pWindow) {
    // we trust the window is valid.
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);
    wlr_output_attach_render(PMONITOR->output, nullptr);

    begin(PMONITOR);

    const auto PFRAMEBUFFER = &m_mWindowFramebuffers[pWindow];

    PFRAMEBUFFER->m_tTransform = g_pXWaylandManager->getWindowSurface(pWindow)->current.transform;

    PFRAMEBUFFER->alloc(PMONITOR->vecSize.x, PMONITOR->vecSize.y);

    PFRAMEBUFFER->bind();

    clear(CColor(0,0,0,0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, &now, !pWindow->m_bX11DoesntWantBorders);

    // restore original fb
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iCurrentOutputFb);
    glViewport(0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecSize.y);

    end();

    wlr_output_rollback(PMONITOR->output);
}

void CHyprOpenGLImpl::renderSnapshot(CWindow** pWindow) {
    const auto PWINDOW = *pWindow;

    auto it = m_mWindowFramebuffers.begin();
    for (;it != m_mWindowFramebuffers.end(); it++) {
        if (it->first == PWINDOW) {
            break;
        }
    }

    if (it == m_mWindowFramebuffers.end() || !it->second.m_cTex.m_iTexID)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    const auto TRANSFORM = wlr_output_transform_invert(it->second.m_tTransform);
    float matrix[9];
    wlr_box windowBox = {0, 0, PMONITOR->vecSize.x, PMONITOR->vecSize.y};
    wlr_matrix_project_box(matrix, &windowBox, TRANSFORM, 0, PMONITOR->output->transform_matrix);

    renderTexture(it->second.m_cTex, matrix, PWINDOW->m_fAlpha, 0);
}