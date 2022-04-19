#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include "../helpers/MiscFunctions.hpp"

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

    #ifdef GLES2
    Debug::log(WARN, "!RENDERER: Using the legacy GLES2 renderer!");
    #endif

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
    m_shRGBA.discardOpaque = glGetUniformLocation(prog, "discardOpaque");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX);
    m_shRGBX.program = prog;
    m_shRGBX.tex = glGetUniformLocation(prog, "tex");
    m_shRGBX.proj = glGetUniformLocation(prog, "proj");
    m_shRGBX.alpha = glGetUniformLocation(prog, "alpha");
    m_shRGBX.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_shRGBX.posAttrib = glGetAttribLocation(prog, "pos");
    m_shRGBX.discardOpaque = glGetUniformLocation(prog, "discardOpaque");

    prog = createProgram(TEXVERTSRC, TEXFRAGSRCEXT);
    m_shEXT.program = prog;
    m_shEXT.tex = glGetUniformLocation(prog, "tex");
    m_shEXT.proj = glGetUniformLocation(prog, "proj");
    m_shEXT.alpha = glGetUniformLocation(prog, "alpha");
    m_shEXT.posAttrib = glGetAttribLocation(prog, "pos");
    m_shEXT.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_shEXT.discardOpaque = glGetUniformLocation(prog, "discardOpaque");

    prog = createProgram(TEXVERTSRC, FRAGBLUR1);
    m_shBLUR1.program = prog;
    m_shBLUR1.tex = glGetUniformLocation(prog, "tex");
    m_shBLUR1.alpha = glGetUniformLocation(prog, "alpha");
    m_shBLUR1.proj = glGetUniformLocation(prog, "proj");
    m_shBLUR1.posAttrib = glGetAttribLocation(prog, "pos");
    m_shBLUR1.texAttrib = glGetAttribLocation(prog, "texcoord");

    prog = createProgram(TEXVERTSRC, FRAGBLUR2);
    m_shBLUR2.program = prog;
    m_shBLUR2.tex = glGetUniformLocation(prog, "tex");
    m_shBLUR2.alpha = glGetUniformLocation(prog, "alpha");
    m_shBLUR2.proj = glGetUniformLocation(prog, "proj");
    m_shBLUR2.posAttrib = glGetAttribLocation(prog, "pos");
    m_shBLUR2.texAttrib = glGetAttribLocation(prog, "texcoord");

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

void CHyprOpenGLImpl::begin(SMonitor* pMonitor, pixman_region32_t* pDamage) {
    m_RenderData.pMonitor = pMonitor;

    glViewport(0, 0, pMonitor->vecSize.x, pMonitor->vecSize.y);

    wlr_matrix_projection(m_RenderData.projection, pMonitor->vecSize.x, pMonitor->vecSize.y, WL_OUTPUT_TRANSFORM_NORMAL); // TODO: this is deprecated

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_iCurrentOutputFb);
    m_iWLROutputFb = m_iCurrentOutputFb;

    // ensure a framebuffer for the monitor exists
    if (m_mMonitorRenderResources.find(pMonitor) == m_mMonitorRenderResources.end() || m_mMonitorRenderResources[pMonitor].primaryFB.m_Size != pMonitor->vecSize) {
        m_mMonitorRenderResources[pMonitor].stencilTex.allocate();

        m_mMonitorRenderResources[pMonitor].primaryFB.m_pStencilTex = &m_mMonitorRenderResources[pMonitor].stencilTex;
        m_mMonitorRenderResources[pMonitor].mirrorFB.m_pStencilTex = &m_mMonitorRenderResources[pMonitor].stencilTex;

        m_mMonitorRenderResources[pMonitor].primaryFB.alloc(pMonitor->vecSize.x * pMonitor->scale, pMonitor->vecSize.y * pMonitor->scale);
        m_mMonitorRenderResources[pMonitor].mirrorFB.alloc(pMonitor->vecSize.x * pMonitor->scale, pMonitor->vecSize.y * pMonitor->scale);

        createBGTextureForMonitor(pMonitor);
    }

    // bind the primary Hypr Framebuffer
    m_mMonitorRenderResources[pMonitor].primaryFB.bind();

    m_RenderData.pDamage = pDamage;

    // clear
    clear(CColor(11, 11, 11, 255));
}

void CHyprOpenGLImpl::end() {
    // end the render, copy the data to the WLR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_iWLROutputFb);
    wlr_box windowBox = {0, 0, m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y};

    clear(CColor(11, 11, 11, 255));

    scaleBox(&windowBox, m_RenderData.pMonitor->scale);
    renderTexture(m_mMonitorRenderResources[m_RenderData.pMonitor].primaryFB.m_cTex, &windowBox, 255.f, 0);

    // reset our data
    m_RenderData.pMonitor = nullptr;
    m_iWLROutputFb = 0;
}

void CHyprOpenGLImpl::clear(const CColor& color) {
    RASSERT(m_RenderData.pMonitor, "Tried to render without begin()!");

    glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);

    if (pixman_region32_not_empty(m_RenderData.pDamage)) {
        PIXMAN_DAMAGE_FOREACH(m_RenderData.pDamage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);

            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    scissor((wlr_box*)nullptr);
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

void CHyprOpenGLImpl::scissor(const pixman_box32* pBox) {
    RASSERT(m_RenderData.pMonitor, "Tried to scissor without begin()!");

    if (!pBox) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    glScissor(pBox->x1, pBox->y1, pBox->x2 - pBox->x1, pBox->y2 - pBox->y1);
    glEnable(GL_SCISSOR_TEST);
}

void CHyprOpenGLImpl::renderRect(wlr_box* box, const CColor& col) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    // TODO: respect damage

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

void CHyprOpenGLImpl::renderTexture(wlr_texture* tex, wlr_box* pBox, float alpha, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTexture(CTexture(tex), pBox, alpha, round);
}

void CHyprOpenGLImpl::renderTexture(const CTexture& tex, wlr_box* pBox, float alpha, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    // TODO: optimize this, this is bad
    if (pixman_region32_not_empty(m_RenderData.pDamage)) {
        PIXMAN_DAMAGE_FOREACH(m_RenderData.pDamage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);

            renderTextureInternal(tex, pBox, alpha, round);
        }
    }

    scissor((wlr_box*)nullptr);
}

void CHyprOpenGLImpl::renderTextureInternal(const CTexture& tex, wlr_box* pBox, float alpha, int round, bool discardOpaque) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(WL_OUTPUT_TRANSFORM_NORMAL);
    float matrix[9];
    wlr_matrix_project_box(matrix, pBox, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

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
    glUniform1i(shader->discardOpaque, (int)discardOpaque);

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

void CHyprOpenGLImpl::renderTextureWithBlur(const CTexture& tex, wlr_box* pBox, float a, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture with blur without begin()!");

    // TODO: optimize this, this is bad
    if (pixman_region32_not_empty(m_RenderData.pDamage)) {
        PIXMAN_DAMAGE_FOREACH(m_RenderData.pDamage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);

            renderTextureWithBlurInternal(tex, pBox, a, round);
        }
    }

    scissor((wlr_box*)nullptr);
}

// This is probably not the quickest method possible,
// feel free to contribute if you have a better method.
// cheers.

// 2-pass pseudo-gaussian blur
void CHyprOpenGLImpl::renderTextureWithBlurInternal(const CTexture& tex, wlr_box* pBox, float a, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    // if blur disabled, just render the texture
    if (g_pConfigManager->getInt("decoration:blur") == 0) {
        renderTextureInternal(tex, pBox, a, round);
        return;
    }

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(WL_OUTPUT_TRANSFORM_NORMAL);
    float matrix[9];
    wlr_matrix_project_box(matrix, pBox, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

    // bind the mirror FB and clear it.
    m_mMonitorRenderResources[m_RenderData.pMonitor].mirrorFB.bind();
    clear(CColor(0, 0, 0, 0));

    // init stencil for blurring only behind da window
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    // render our window to the mirror FB while also writing to the stencil, discard opaque pixels
    renderTextureInternal(tex, pBox, a, round, true);

    // then we disable writing to the mask and ONLY accept writing within the stencil
    glStencilFunc(GL_EQUAL, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    // now we bind back the primary FB
    // the mirror FB now has only our window.
    m_mMonitorRenderResources[m_RenderData.pMonitor].primaryFB.bind();

    glEnable(GL_BLEND);

    // now we make the blur by blurring the main framebuffer (it will only affect the stencil)

    // matrix
    float matrixFull[9];
    wlr_box fullMonBox = {0, 0, m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y};
    wlr_matrix_project_box(matrixFull, &fullMonBox, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrixFull);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);

    wlr_matrix_transpose(glMatrix, glMatrix);

    const auto RADIUS = g_pConfigManager->getInt("decoration:blur_size") + 2;
    const auto BLURPASSES = g_pConfigManager->getInt("decoration:blur_passes");
    const auto PFRAMEBUFFER = &m_mMonitorRenderResources[m_RenderData.pMonitor].primaryFB;

    auto drawWithShader = [&](CShader* pShader) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(PFRAMEBUFFER->m_cTex.m_iTarget, PFRAMEBUFFER->m_cTex.m_iTexID);

        glTexParameteri(tex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glUseProgram(pShader->program);

        glUniform1f(glGetUniformLocation(pShader->program, "radius"), RADIUS);
        glUniform2f(glGetUniformLocation(pShader->program, "resolution"), m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y);
        glUniformMatrix3fv(pShader->proj, 1, GL_FALSE, glMatrix);
        glUniform1i(pShader->tex, 0);
        glUniform1f(pShader->alpha, a / 255.f);

        glVertexAttribPointer(pShader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
        glVertexAttribPointer(pShader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

        glEnableVertexAttribArray(pShader->posAttrib);
        glEnableVertexAttribArray(pShader->texAttrib);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(pShader->posAttrib);
        glDisableVertexAttribArray(pShader->texAttrib);
    };

    for (int i = 0; i < BLURPASSES; ++i) {
        drawWithShader(&m_shBLUR1);  // horizontal pass
        drawWithShader(&m_shBLUR2);  // vertical pass
    }

    glBindTexture(tex.m_iTarget, 0);

    // disable the stencil
    glStencilMask(-1);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glDisable(GL_STENCIL_TEST);

    // when the blur is done, let's render the window itself. We can't use mirror because it had discardOpaque
    renderTextureInternal(tex, pBox, a, round);
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

    scaleBox(box, m_RenderData.pMonitor->scale);

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

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    pixman_region32_t fakeDamage;
    pixman_region32_init(&fakeDamage);
    pixman_region32_union_rect(&fakeDamage, &fakeDamage, 0, 0, (int)PMONITOR->vecSize.x, (int)PMONITOR->vecSize.y);

    begin(PMONITOR, &fakeDamage);

    pixman_region32_fini(&fakeDamage);

    const auto PFRAMEBUFFER = &m_mWindowFramebuffers[pWindow];

    PFRAMEBUFFER->m_tTransform = g_pXWaylandManager->getWindowSurface(pWindow)->current.transform;

    PFRAMEBUFFER->alloc(PMONITOR->vecSize.x, PMONITOR->vecSize.y);

    PFRAMEBUFFER->bind();

    clear(CColor(0,0,0,0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // this is a hack but it works :P
    // we need to disable blur or else we will get a black background, as the shader
    // will try to copy the bg to apply blur.
    // this isn't entirely correct, but like, oh well.
    // small todo: maybe make this correct? :P
    const auto BLURVAL = g_pConfigManager->getInt("decoration:blur");
    g_pConfigManager->setInt("decoration:blur", 0);

    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, &now, !pWindow->m_bX11DoesntWantBorders);

    g_pConfigManager->setInt("decoration:blur", BLURVAL);

    // restore original fb
    #ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iCurrentOutputFb);
    #else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iCurrentOutputFb);
    #endif
    glViewport(0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecSize.y);

    end();

    wlr_output_rollback(PMONITOR->output);
}

void CHyprOpenGLImpl::renderSnapshot(CWindow** pWindow) {
    RASSERT(m_RenderData.pMonitor, "Tried to render snapshot rect without begin()!");
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

    wlr_box windowBox = {0, 0, PMONITOR->vecSize.x, PMONITOR->vecSize.y};

    renderTextureInternal(it->second.m_cTex, &windowBox, PWINDOW->m_fAlpha, 0);
}

void CHyprOpenGLImpl::createBGTextureForMonitor(SMonitor* pMonitor) {
    RASSERT(m_RenderData.pMonitor, "Tried to createBGTex without begin()!");

    // release the last tex if exists
    const auto PTEX = &m_mMonitorBGTextures[pMonitor];
    PTEX->destroyTexture();

    PTEX->allocate();

    Debug::log(LOG, "Allocated texture for BGTex");

    // check if wallpapers exist
    if (!std::filesystem::exists("/usr/share/hyprland/wall_8K.png"))
        return; // the texture will be empty, oh well. We'll clear with a solid color anyways.

    // get the adequate tex
    std::string texPath = "/usr/share/hyprland/wall_";
    Vector2D textureSize;
    if (pMonitor->vecSize.x > 7000) {
        textureSize = Vector2D(7680, 4320);
        texPath += "8K.png";
    } else if (pMonitor->vecSize.x > 3000) {
        textureSize = Vector2D(3840, 2160);
        texPath += "4K.png";
    } else {
        textureSize = Vector2D(1920, 1080);
        texPath += "2K.png";
    }

    // create a new one with cairo
    const auto CAIROSURFACE = cairo_image_surface_create_from_png(texPath.c_str());

    const auto CAIRO = cairo_create(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    glBindTexture(GL_TEXTURE_2D, PTEX->m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    #ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    #endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureSize.x, textureSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    cairo_surface_destroy(CAIROSURFACE);
    cairo_destroy(CAIRO);

    Debug::log(LOG, "Background created for monitor %s", pMonitor->szName.c_str());
}

void CHyprOpenGLImpl::clearWithTex() {
    RASSERT(m_RenderData.pMonitor, "Tried to render BGtex without begin()!");
    
    wlr_box box = {0, 0, m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y};

    renderTexture(m_mMonitorBGTextures[m_RenderData.pMonitor], &box, 255, 0);
}

void CHyprOpenGLImpl::destroyMonitorResources(SMonitor* pMonitor) {
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].mirrorFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].primaryFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].stencilTex.destroyTexture();
    g_pHyprOpenGL->m_mMonitorBGTextures[pMonitor].destroyTexture();
    g_pHyprOpenGL->m_mMonitorRenderResources.erase(pMonitor);
    g_pHyprOpenGL->m_mMonitorBGTextures.erase(pMonitor);

    Debug::log(LOG, "Monitor %s -> destroyed all render data", pMonitor->szName.c_str());
}
