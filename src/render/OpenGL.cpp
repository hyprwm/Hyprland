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
    m_shQUAD.texAttrib = glGetAttribLocation(prog, "texcoord");

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

    pixman_region32_init(&m_rOriginalDamageRegion);

    // End

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
        m_mMonitorRenderResources[pMonitor].mirrorSwapFB.m_pStencilTex = &m_mMonitorRenderResources[pMonitor].stencilTex;

        m_mMonitorRenderResources[pMonitor].primaryFB.alloc(pMonitor->vecSize.x * pMonitor->scale, pMonitor->vecSize.y * pMonitor->scale);
        m_mMonitorRenderResources[pMonitor].mirrorFB.alloc(pMonitor->vecSize.x * pMonitor->scale, pMonitor->vecSize.y * pMonitor->scale);
        m_mMonitorRenderResources[pMonitor].mirrorSwapFB.alloc(pMonitor->vecSize.x * pMonitor->scale, pMonitor->vecSize.y * pMonitor->scale);

        createBGTextureForMonitor(pMonitor);
    }

    // bind the primary Hypr Framebuffer
    m_mMonitorRenderResources[pMonitor].primaryFB.bind();

    m_RenderData.pDamage = pDamage;
}

void CHyprOpenGLImpl::end() {
    // end the render, copy the data to the WLR framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_iWLROutputFb);
    wlr_box windowBox = {0, 0, m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y};

    pixman_region32_copy(m_RenderData.pDamage, &m_rOriginalDamageRegion);

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

void CHyprOpenGLImpl::scissor(const int x, const int y, const int w, const int h) {
    wlr_box box = {x,y,w,h};
    scissor(&box);
}

void CHyprOpenGLImpl::renderRect(wlr_box* box, const CColor& col, int round) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    float matrix[9];
    wlr_matrix_project_box(matrix, box, WL_OUTPUT_TRANSFORM_NORMAL, 0, m_RenderData.pMonitor->output->transform_matrix);  // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);

    wlr_matrix_transpose(glMatrix, glMatrix);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_shQUAD.program);

    glUniformMatrix3fv(m_shQUAD.proj, 1, GL_FALSE, glMatrix);
    glUniform4f(m_shQUAD.color, col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f);

    const auto TOPLEFT = Vector2D(round, round);
    const auto BOTTOMRIGHT = Vector2D(box->width - round, box->height - round);
    const auto FULLSIZE = Vector2D(box->width, box->height);

    // Rounded corners
    glUniform2f(glGetUniformLocation(m_shQUAD.program, "topLeft"), (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(glGetUniformLocation(m_shQUAD.program, "bottomRight"), (float)BOTTOMRIGHT.x, (float)BOTTOMRIGHT.y);
    glUniform2f(glGetUniformLocation(m_shQUAD.program, "fullSize"), (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(glGetUniformLocation(m_shQUAD.program, "radius"), round);

    glVertexAttribPointer(m_shQUAD.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(m_shQUAD.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_shQUAD.posAttrib);
    glEnableVertexAttribArray(m_shQUAD.texAttrib);

    if (pixman_region32_not_empty(m_RenderData.pDamage)) {
        PIXMAN_DAMAGE_FOREACH(m_RenderData.pDamage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(m_shQUAD.posAttrib);
    glDisableVertexAttribArray(m_shQUAD.texAttrib);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void CHyprOpenGLImpl::renderTexture(wlr_texture* tex, wlr_box* pBox, float alpha, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTexture(CTexture(tex), pBox, alpha, round);
}

void CHyprOpenGLImpl::renderTexture(const CTexture& tex, wlr_box* pBox, float alpha, int round, bool discardopaque, bool border) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTextureInternalWithDamage(tex, pBox, alpha, m_RenderData.pDamage, round, discardopaque, border);

    scissor((wlr_box*)nullptr);
}

void CHyprOpenGLImpl::renderTextureInternalWithDamage(const CTexture& tex, wlr_box* pBox, float alpha, pixman_region32_t* damage, int round, bool discardOpaque, bool border) {
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
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

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

    if (pixman_region32_not_empty(m_RenderData.pDamage)) {
        PIXMAN_DAMAGE_FOREACH(m_RenderData.pDamage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(shader->posAttrib);
    glDisableVertexAttribArray(shader->texAttrib);

    glBindTexture(tex.m_iTarget, 0);
}

// This probably isn't the fastest
// but it works... well, I guess?
//
// Dual (or more) kawase blur
CFramebuffer* CHyprOpenGLImpl::blurMainFramebufferWithDamage(float a, wlr_box* pBox, pixman_region32_t* originalDamage) {

    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);

    // get transforms for the full monitor
    const auto TRANSFORM = wlr_output_transform_invert(WL_OUTPUT_TRANSFORM_NORMAL);
    float matrix[9];
    wlr_box MONITORBOX = {0, 0, m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y};
    wlr_matrix_project_box(matrix, &MONITORBOX, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);
    wlr_matrix_transpose(glMatrix, glMatrix);

    // get the config settings
    const auto BLURSIZE = g_pConfigManager->getInt("decoration:blur_size");
    const auto BLURPASSES = g_pConfigManager->getInt("decoration:blur_passes");

    // prep damage
    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_copy(&damage, originalDamage);
    wlr_region_expand(&damage, &damage, pow(2, BLURPASSES) * BLURSIZE);

    // helper
    const auto PMIRRORFB = &m_mMonitorRenderResources[m_RenderData.pMonitor].mirrorFB;
    const auto PMIRRORSWAPFB = &m_mMonitorRenderResources[m_RenderData.pMonitor].mirrorSwapFB;

    CFramebuffer* currentRenderToFB = &m_mMonitorRenderResources[m_RenderData.pMonitor].primaryFB;

    // declare the draw func
    auto drawPass = [&](CShader* pShader, pixman_region32_t* pDamage) {
        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(currentRenderToFB->m_cTex.m_iTarget, currentRenderToFB->m_cTex.m_iTexID);

        glTexParameteri(currentRenderToFB->m_cTex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glUseProgram(pShader->program);

        // prep two shaders
        glUniformMatrix3fv(pShader->proj, 1, GL_FALSE, glMatrix);
        glUniform1f(glGetUniformLocation(pShader->program, "radius"), BLURSIZE * (a / 255.f));  // this makes the blursize change with a
        if (pShader == &m_shBLUR1)
            glUniform2f(glGetUniformLocation(m_shBLUR1.program, "halfpixel"), 0.5f / (m_RenderData.pMonitor->vecSize.x / 2.f), 0.5f / (m_RenderData.pMonitor->vecSize.y / 2.f));
        else
            glUniform2f(glGetUniformLocation(m_shBLUR2.program, "halfpixel"), 0.5f / (m_RenderData.pMonitor->vecSize.x * 2.f), 0.5f / (m_RenderData.pMonitor->vecSize.y * 2.f));
        glUniform1i(pShader->tex, 0);

        glVertexAttribPointer(pShader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
        glVertexAttribPointer(pShader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

        glEnableVertexAttribArray(pShader->posAttrib);
        glEnableVertexAttribArray(pShader->texAttrib);

        if (pixman_region32_not_empty(pDamage)) {
            PIXMAN_DAMAGE_FOREACH(pDamage) {
                const auto RECT = RECTSARR[i];
                scissor(&RECT);

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }

        glDisableVertexAttribArray(pShader->posAttrib);
        glDisableVertexAttribArray(pShader->texAttrib);

        if (currentRenderToFB != PMIRRORFB)
            currentRenderToFB = PMIRRORFB;
        else
            currentRenderToFB = PMIRRORSWAPFB;
    };

    // draw the things.
    // first draw is prim -> mirr
    PMIRRORFB->bind();
    glBindTexture(m_mMonitorRenderResources[m_RenderData.pMonitor].primaryFB.m_cTex.m_iTarget, m_mMonitorRenderResources[m_RenderData.pMonitor].primaryFB.m_cTex.m_iTexID);

    // damage region will be scaled, make a temp
    pixman_region32_t tempDamage;
    pixman_region32_init(&tempDamage);
    wlr_region_scale(&tempDamage, &damage, 1.f / 2.f); // when DOWNscaling, we make the region twice as small because it's the TARGET

    drawPass(&m_shBLUR1, &tempDamage);

    // and draw
    for (int i = 1; i < BLURPASSES; ++i) {
        wlr_region_scale(&tempDamage, &damage, 1.f / (1 << (i + 1)));
        drawPass(&m_shBLUR1, &tempDamage);  // down
    }

    for (int i = BLURPASSES - 1; i >= 0; --i) {
        wlr_region_scale(&tempDamage, &damage, 1.f / (1 << i));  // when upsampling we make the region twice as big
        drawPass(&m_shBLUR2, &tempDamage);  // up
    }

    // finish
    pixman_region32_fini(&tempDamage);
    pixman_region32_fini(&damage);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(PMIRRORFB->m_cTex.m_iTarget, 0);

    return currentRenderToFB;
}

void CHyprOpenGLImpl::renderTextureWithBlur(const CTexture& tex, wlr_box* pBox, float a, wlr_surface* pSurface, int round, bool border) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture with blur without begin()!");

    if (g_pConfigManager->getInt("decoration:blur") == 0) {
        renderTexture(tex, pBox, a, round, false, border);
        return;
    }

    // make a damage region for this window
    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_intersect_rect(&damage, m_RenderData.pDamage, pBox->x, pBox->y, pBox->width, pBox->height);  // clip it to the box

    // amazing hack: the surface has an opaque region!
    pixman_region32_t inverseOpaque;
    pixman_region32_init(&inverseOpaque);
    if (a == 255.f) {
        pixman_box32_t monbox = {0, 0, m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y};
        pixman_region32_copy(&inverseOpaque, &pSurface->current.opaque);
        pixman_region32_translate(&inverseOpaque, pBox->x, pBox->y);
        pixman_region32_inverse(&inverseOpaque, &inverseOpaque, &monbox);
        pixman_region32_intersect(&inverseOpaque, &damage, &inverseOpaque);
    } else {
        pixman_region32_copy(&inverseOpaque, &damage);
    }

    if (!pixman_region32_not_empty(&damage))
        return; // if its empty, reject.

    // blur the main FB, it will be rendered onto the mirror
    const auto POUTFB = blurMainFramebufferWithDamage(a, pBox, &inverseOpaque);

    pixman_region32_fini(&inverseOpaque);

    // bind primary
    m_mMonitorRenderResources[m_RenderData.pMonitor].primaryFB.bind();

    // make a stencil for rounded corners to work with blur
    scissor((wlr_box*)nullptr);  // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    renderTexture(tex, pBox, a, round, true);  // discard opaque
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_EQUAL, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    // stencil done. Render everything.
    wlr_box MONITORBOX = {0, 0, m_RenderData.pMonitor->vecSize.x, m_RenderData.pMonitor->vecSize.y};
    if (pixman_region32_not_empty(&damage)) {
        // render our great blurred FB
        renderTextureInternalWithDamage(POUTFB->m_cTex, &MONITORBOX, a, &damage);
        
        // render the window, but clear stencil
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        // and write to it
        glStencilFunc(GL_ALWAYS, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        renderTextureInternalWithDamage(tex, pBox, a, &damage, round);

        // then stop
        glStencilFunc(GL_EQUAL, 1, -1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }

    // disable the stencil (if no border), finalize everything
    if (!border) {
        glStencilMask(-1);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
    } else {
        auto BORDERCOL = m_pCurrentWindow->m_cRealBorderColor.col();
        BORDERCOL.a *= a / 255.f;
        renderBorder(pBox, BORDERCOL, g_pConfigManager->getInt("general:border_size"), round);
    }
    
    glDisable(GL_STENCIL_TEST);
    pixman_region32_fini(&damage);
    scissor((wlr_box*)nullptr);
}

void pushVert2D(float x, float y, float* arr, int& counter, wlr_box* box) {
    // 0-1 space god damnit
    arr[counter * 2 + 0] = x / box->width;
    arr[counter * 2 + 1] = y / box->height;
    counter++;
}

void CHyprOpenGLImpl::renderBorder(wlr_box* box, const CColor& col, int thick, int round) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    // this method assumes a set stencil and scaled box
    box->x -= thick;
    box->y -= thick;
    box->width += 2 * thick;
    box->height += 2 * thick;

    // only draw on non-stencild.
    glStencilFunc(GL_NOTEQUAL, 1, -1);

    // draw a rounded rect
    renderRect(box, col, round);
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

void CHyprOpenGLImpl::makeLayerSnapshot(SLayerSurface* pLayer) {
    // we trust the window is valid.
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pLayer->monitorID);
    wlr_output_attach_render(PMONITOR->output, nullptr);

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    pixman_region32_t fakeDamage;
    pixman_region32_init(&fakeDamage);
    pixman_region32_union_rect(&fakeDamage, &fakeDamage, 0, 0, (int)PMONITOR->vecSize.x, (int)PMONITOR->vecSize.y);

    begin(PMONITOR, &fakeDamage);

    pixman_region32_fini(&fakeDamage);

    const auto PFRAMEBUFFER = &m_mLayerFramebuffers[pLayer];

    PFRAMEBUFFER->m_tTransform = pLayer->layerSurface->surface->current.transform;

    PFRAMEBUFFER->alloc(PMONITOR->vecSize.x, PMONITOR->vecSize.y);

    PFRAMEBUFFER->bind();

    clear(CColor(0, 0, 0, 0));  // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // draw the layer
    g_pHyprRenderer->renderLayer(pLayer, PMONITOR, &now);

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
    for (; it != m_mWindowFramebuffers.end(); it++) {
        if (it->first == PWINDOW) {
            break;
        }
    }

    if (it == m_mWindowFramebuffers.end() || !it->second.m_cTex.m_iTexID)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    wlr_box windowBox = {0, 0, PMONITOR->vecSize.x, PMONITOR->vecSize.y};

    pixman_region32_t fakeDamage;
    pixman_region32_init_rect(&fakeDamage, 0, 0, PMONITOR->vecSize.x, PMONITOR->vecSize.y);

    renderTextureInternalWithDamage(it->second.m_cTex, &windowBox, PWINDOW->m_fAlpha.fl(), &fakeDamage, 0);

    pixman_region32_fini(&fakeDamage);
}

void CHyprOpenGLImpl::renderSnapshot(SLayerSurface** pLayer) {
    RASSERT(m_RenderData.pMonitor, "Tried to render snapshot rect without begin()!");
    const auto PLAYER = *pLayer;

    auto it = m_mLayerFramebuffers.begin();
    for (; it != m_mLayerFramebuffers.end(); it++) {
        if (it->first == PLAYER) {
            break;
        }
    }

    if (it == m_mLayerFramebuffers.end() || !it->second.m_cTex.m_iTexID)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PLAYER->monitorID);

    wlr_box windowBox = {0, 0, PMONITOR->vecSize.x, PMONITOR->vecSize.y};

    pixman_region32_t fakeDamage;
    pixman_region32_init_rect(&fakeDamage, 0, 0, PMONITOR->vecSize.x, PMONITOR->vecSize.y);

    renderTextureInternalWithDamage(it->second.m_cTex, &windowBox, PLAYER->alpha.fl(), &fakeDamage, 0);

    pixman_region32_fini(&fakeDamage);
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
