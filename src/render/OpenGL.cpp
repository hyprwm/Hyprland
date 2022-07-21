#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include "../helpers/MiscFunctions.hpp"

CHyprOpenGLImpl::CHyprOpenGLImpl() {
    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL)), "Couldn't unset current EGL!");

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

    prog = createProgram(QUADVERTSRC, FRAGSHADOW);
    m_shSHADOW.program = prog;
    m_shSHADOW.proj = glGetUniformLocation(prog, "proj");
    m_shSHADOW.posAttrib = glGetAttribLocation(prog, "pos");
    m_shSHADOW.texAttrib = glGetAttribLocation(prog, "texcoord");

    prog = createProgram(QUADVERTSRC, FRAGBORDER1);
    m_shBORDER1.program = prog;
    m_shBORDER1.proj = glGetUniformLocation(prog, "proj");
    m_shBORDER1.posAttrib = glGetAttribLocation(prog, "pos");
    m_shBORDER1.texAttrib = glGetAttribLocation(prog, "texcoord");

    Debug::log(LOG, "Shaders initialized successfully.");

    // End shaders

    pixman_region32_init(&m_rOriginalDamageRegion);

    // End

    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "Couldn't unset current EGL!");

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

void CHyprOpenGLImpl::begin(SMonitor* pMonitor, pixman_region32_t* pDamage, bool fake) {
    m_RenderData.pMonitor = pMonitor;

    glViewport(0, 0, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);

    matrixProjection(m_RenderData.projection, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, WL_OUTPUT_TRANSFORM_NORMAL);

    m_RenderData.pCurrentMonData = &m_mMonitorRenderResources[pMonitor];

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_iCurrentOutputFb);
    m_iWLROutputFb = m_iCurrentOutputFb;

    // ensure a framebuffer for the monitor exists
    if (m_mMonitorRenderResources.find(pMonitor) == m_mMonitorRenderResources.end() || m_RenderData.pCurrentMonData->primaryFB.m_Size != pMonitor->vecPixelSize) {
        m_RenderData.pCurrentMonData->stencilTex.allocate();

        m_RenderData.pCurrentMonData->primaryFB.m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->mirrorFB.m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->mirrorSwapFB.m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;

        m_RenderData.pCurrentMonData->primaryFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);
        m_RenderData.pCurrentMonData->mirrorFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);
        m_RenderData.pCurrentMonData->mirrorSwapFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);

        createBGTextureForMonitor(pMonitor);
    }

    // bind the primary Hypr Framebuffer
    m_RenderData.pCurrentMonData->primaryFB.bind();

    m_RenderData.pDamage = pDamage;

    m_bFakeFrame = fake;
}

void CHyprOpenGLImpl::end() {
    // end the render, copy the data to the WLR framebuffer
    if (!m_bFakeFrame) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_iWLROutputFb);
        wlr_box monbox = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};

        pixman_region32_copy(m_RenderData.pDamage, &m_rOriginalDamageRegion);

        clear(CColor(11, 11, 11, 255));

        m_bEndFrame = true;

        renderTexture(m_RenderData.pCurrentMonData->primaryFB.m_cTex, &monbox, 255.f, 0);

        m_bEndFrame = false;
    }

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

    wlr_box newBox = *pBox;

    int w, h;
    wlr_output_transformed_resolution(m_RenderData.pMonitor->output, &w, &h);

    const auto TR = wlr_output_transform_invert(m_RenderData.pMonitor->transform);
    wlr_box_transform(&newBox, &newBox, TR, w, h);

    glScissor(newBox.x, newBox.y, newBox.width, newBox.height);
    glEnable(GL_SCISSOR_TEST);
}

void CHyprOpenGLImpl::scissor(const pixman_box32* pBox) {
    RASSERT(m_RenderData.pMonitor, "Tried to scissor without begin()!");

    if (!pBox) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    wlr_box newBox = {pBox->x1, pBox->y1, pBox->x2 - pBox->x1, pBox->y2 - pBox->y1};

    scissor(&newBox);
}

void CHyprOpenGLImpl::scissor(const int x, const int y, const int w, const int h) {
    wlr_box box = {x,y,w,h};
    scissor(&box);
}

void CHyprOpenGLImpl::renderRect(wlr_box* box, const CColor& col, int round) {
    renderRectWithDamage(box, col, m_RenderData.pDamage, round);
}

void CHyprOpenGLImpl::renderRectWithDamage(wlr_box* box, const CColor& col, pixman_region32_t* damage, int round) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    float matrix[9];
    wlr_matrix_project_box(matrix, box, wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), 0, m_RenderData.pMonitor->output->transform_matrix);  // TODO: write own, don't use WLR here

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

    static auto *const PMULTISAMPLEEDGES = &g_pConfigManager->getConfigValuePtr("decoration:multisample_edges")->intValue;

    // Rounded corners
    glUniform2f(m_shQUAD.getUniformLocation("topLeft"), (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(m_shQUAD.getUniformLocation("bottomRight"), (float)BOTTOMRIGHT.x, (float)BOTTOMRIGHT.y);
    glUniform2f(m_shQUAD.getUniformLocation("fullSize"), (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(m_shQUAD.getUniformLocation("radius"), round);
    glUniform1i(m_shQUAD.getUniformLocation("primitiveMultisample"), (int)(*PMULTISAMPLEEDGES == 1 && round != 0));

    glVertexAttribPointer(m_shQUAD.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(m_shQUAD.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_shQUAD.posAttrib);
    glEnableVertexAttribArray(m_shQUAD.texAttrib);

    if (pixman_region32_not_empty(damage)) {
        PIXMAN_DAMAGE_FOREACH(damage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(m_shQUAD.posAttrib);
    glDisableVertexAttribArray(m_shQUAD.texAttrib);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void CHyprOpenGLImpl::renderTexture(wlr_texture* tex, wlr_box* pBox, float alpha, int round, bool allowCustomUV) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTexture(CTexture(tex), pBox, alpha, round, false, allowCustomUV);
}

void CHyprOpenGLImpl::renderTexture(const CTexture& tex, wlr_box* pBox, float alpha, int round, bool discardopaque, bool allowCustomUV) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTextureInternalWithDamage(tex, pBox, alpha, m_RenderData.pDamage, round, discardopaque, false, allowCustomUV);

    scissor((wlr_box*)nullptr);
}

void CHyprOpenGLImpl::renderTextureInternalWithDamage(const CTexture& tex, wlr_box* pBox, float alpha, pixman_region32_t* damage, int round, bool discardOpaque, bool noAA, bool allowCustomUV) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform);
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
    const auto BOTTOMRIGHT = Vector2D(pBox->width - round, pBox->height - round);
    const auto FULLSIZE = Vector2D(pBox->width, pBox->height);
    static auto *const PMULTISAMPLEEDGES = &g_pConfigManager->getConfigValuePtr("decoration:multisample_edges")->intValue;

    // Rounded corners
    glUniform2f(shader->getUniformLocation("topLeft"), (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(shader->getUniformLocation("bottomRight"), (float)BOTTOMRIGHT.x, (float)BOTTOMRIGHT.y);
    glUniform2f(shader->getUniformLocation("fullSize"), (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(shader->getUniformLocation("radius"), round);
    glUniform1i(shader->getUniformLocation("primitiveMultisample"), (int)(*PMULTISAMPLEEDGES == 1 && round != 0 && !noAA));

    glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    const float verts[] = {
        m_RenderData.primarySurfaceUVBottomRight.x, m_RenderData.primarySurfaceUVTopLeft.y,      // top right
        m_RenderData.primarySurfaceUVTopLeft.x, m_RenderData.primarySurfaceUVTopLeft.y,          // top left
        m_RenderData.primarySurfaceUVBottomRight.x, m_RenderData.primarySurfaceUVBottomRight.y,  // bottom right
        m_RenderData.primarySurfaceUVTopLeft.x, m_RenderData.primarySurfaceUVBottomRight.y,      // bottom left
    };

    if (allowCustomUV && m_RenderData.primarySurfaceUVTopLeft != Vector2D(-1, -1)) {
        glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
    } else {
        glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    }

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
    wlr_box MONITORBOX = {0, 0, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y};
    wlr_matrix_project_box(matrix, &MONITORBOX, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);
    wlr_matrix_transpose(glMatrix, glMatrix);

    // get the config settings
    static auto *const PBLURSIZE = &g_pConfigManager->getConfigValuePtr("decoration:blur_size")->intValue;
    static auto *const PBLURPASSES = &g_pConfigManager->getConfigValuePtr("decoration:blur_passes")->intValue;

    // prep damage
    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_copy(&damage, originalDamage);
    wlr_region_expand(&damage, &damage, pow(2, *PBLURPASSES) * *PBLURSIZE);

    // helper
    const auto PMIRRORFB = &m_RenderData.pCurrentMonData->mirrorFB;
    const auto PMIRRORSWAPFB = &m_RenderData.pCurrentMonData->mirrorSwapFB;

    CFramebuffer* currentRenderToFB = &m_RenderData.pCurrentMonData->primaryFB;

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
        glUniform1f(pShader->getUniformLocation("radius"), *PBLURSIZE * (a / 255.f));  // this makes the blursize change with a
        if (pShader == &m_shBLUR1)
            glUniform2f(m_shBLUR1.getUniformLocation("halfpixel"), 0.5f / (m_RenderData.pMonitor->vecPixelSize.x / 2.f), 0.5f / (m_RenderData.pMonitor->vecPixelSize.y / 2.f));
        else
            glUniform2f(m_shBLUR2.getUniformLocation("halfpixel"), 0.5f / (m_RenderData.pMonitor->vecPixelSize.x * 2.f), 0.5f / (m_RenderData.pMonitor->vecPixelSize.y * 2.f));
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
    glBindTexture(m_RenderData.pCurrentMonData->primaryFB.m_cTex.m_iTarget, m_RenderData.pCurrentMonData->primaryFB.m_cTex.m_iTexID);

    // damage region will be scaled, make a temp
    pixman_region32_t tempDamage;
    pixman_region32_init(&tempDamage);
    wlr_region_scale(&tempDamage, &damage, 1.f / 2.f); // when DOWNscaling, we make the region twice as small because it's the TARGET

    drawPass(&m_shBLUR1, &tempDamage);

    // and draw
    for (int i = 1; i < *PBLURPASSES; ++i) {
        wlr_region_scale(&tempDamage, &damage, 1.f / (1 << (i + 1)));
        drawPass(&m_shBLUR1, &tempDamage);  // down
    }

    for (int i = *PBLURPASSES - 1; i >= 0; --i) {
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

void CHyprOpenGLImpl::renderTextureWithBlur(const CTexture& tex, wlr_box* pBox, float a, wlr_surface* pSurface, int round) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture with blur without begin()!");

    static auto *const PBLURENABLED = &g_pConfigManager->getConfigValuePtr("decoration:blur")->intValue;
    static auto* const PNOBLUROVERSIZED = &g_pConfigManager->getConfigValuePtr("decoration:no_blur_on_oversized")->intValue;

    if (*PBLURENABLED == 0 || (*PNOBLUROVERSIZED && m_RenderData.primarySurfaceUVTopLeft != Vector2D(-1, -1)) || (m_pCurrentWindow && m_pCurrentWindow->m_sAdditionalConfigData.forceNoBlur)) {
        renderTexture(tex, pBox, a, round, false, true);
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
        pixman_box32_t monbox = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
        pixman_region32_copy(&inverseOpaque, &pSurface->current.opaque);
        pixman_region32_translate(&inverseOpaque, pBox->x, pBox->y);
        pixman_region32_inverse(&inverseOpaque, &inverseOpaque, &monbox);
        pixman_region32_intersect(&inverseOpaque, &damage, &inverseOpaque);
    } else {
        pixman_region32_copy(&inverseOpaque, &damage);
    }

    if (!pixman_region32_not_empty(&inverseOpaque)) {
        renderTexture(tex, pBox, a, round, false, true); // reject blurring a fully opaque window
        return;
    }

    // blur the main FB, it will be rendered onto the mirror
    const auto POUTFB = blurMainFramebufferWithDamage(a, pBox, &inverseOpaque);

    pixman_region32_fini(&inverseOpaque);

    // bind primary
    m_RenderData.pCurrentMonData->primaryFB.bind();

    // make a stencil for rounded corners to work with blur
    scissor((wlr_box*)nullptr);  // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    renderTexture(tex, pBox, a, round, true, true);  // discard opaque
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_EQUAL, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    // stencil done. Render everything.
    wlr_box MONITORBOX = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    if (pixman_region32_not_empty(&damage)) {
        // render our great blurred FB
        static auto *const PBLURIGNOREOPACITY = &g_pConfigManager->getConfigValuePtr("decoration:blur_ignore_opacity")->intValue;
        renderTextureInternalWithDamage(POUTFB->m_cTex, &MONITORBOX, *PBLURIGNOREOPACITY ? 255.f : a, &damage, 0, false, false, false);

        // render the window, but clear stencil
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);

        // draw window
        glDisable(GL_STENCIL_TEST);
        renderTextureInternalWithDamage(tex, pBox, a, &damage, round, false, false, true);
    }

    glStencilMask(-1);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    pixman_region32_fini(&damage);
    scissor((wlr_box*)nullptr);
}

void pushVert2D(float x, float y, float* arr, int& counter, wlr_box* box) {
    // 0-1 space god damnit
    arr[counter * 2 + 0] = x / box->width;
    arr[counter * 2 + 1] = y / box->height;
    counter++;
}

void CHyprOpenGLImpl::renderBorder(wlr_box* box, const CColor& col, int round) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    static auto *const PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;
    static auto *const PMULTISAMPLE = &g_pConfigManager->getConfigValuePtr("decoration:multisample_edges")->intValue;

    // adjust box
    box->x -= *PBORDERSIZE;
    box->y -= *PBORDERSIZE;
    box->width += 2 * *PBORDERSIZE;
    box->height += 2 * *PBORDERSIZE;

    round += *PBORDERSIZE;

    float matrix[9];
    wlr_matrix_project_box(matrix, box, wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), 0, m_RenderData.pMonitor->output->transform_matrix);  // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);

    wlr_matrix_transpose(glMatrix, glMatrix);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_shBORDER1.program);

    glUniformMatrix3fv(m_shBORDER1.proj, 1, GL_FALSE, glMatrix);
    glUniform4f(m_shBORDER1.getUniformLocation("color"), col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f);

    const auto TOPLEFT = Vector2D(round, round);
    const auto BOTTOMRIGHT = Vector2D(box->width - round, box->height - round);
    const auto FULLSIZE = Vector2D(box->width, box->height);

    glUniform2f(m_shBORDER1.getUniformLocation("topLeft"), (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(m_shBORDER1.getUniformLocation("bottomRight"), (float)BOTTOMRIGHT.x, (float)BOTTOMRIGHT.y);
    glUniform2f(m_shBORDER1.getUniformLocation("fullSize"), (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(m_shBORDER1.getUniformLocation("radius"), round);
    glUniform1f(m_shBORDER1.getUniformLocation("thick"), *PBORDERSIZE);
    glUniform1i(m_shBORDER1.getUniformLocation("primitiveMultisample"), *PMULTISAMPLE);

    glVertexAttribPointer(m_shBORDER1.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(m_shBORDER1.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_shBORDER1.posAttrib);
    glEnableVertexAttribArray(m_shBORDER1.texAttrib);

    if (pixman_region32_not_empty(m_RenderData.pDamage)) {
        PIXMAN_DAMAGE_FOREACH(m_RenderData.pDamage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(m_shBORDER1.posAttrib);
    glDisableVertexAttribArray(m_shBORDER1.texAttrib);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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

    begin(PMONITOR, &fakeDamage, true);

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

    m_bEndFrame = true;

    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, &now, !pWindow->m_bX11DoesntWantBorders, RENDER_PASS_ALL);

    g_pConfigManager->setInt("decoration:blur", BLURVAL);

    // render onto the window fb
    // we rendered onto the primary because it has a stencil, which we need for the borders etc
    const auto PFRAMEBUFFER = &m_mWindowFramebuffers[pWindow];

    glViewport(0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y);

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    PFRAMEBUFFER->bind();

    clear(CColor(0, 0, 0, 0));  // JIC

    wlr_box fullMonBox = {0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};
    
    renderTexture(m_RenderData.pCurrentMonData->primaryFB.m_cTex, &fullMonBox, 255.f, 0);
    m_bEndFrame = false;

    // restore original fb
    #ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iCurrentOutputFb);
    #else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iCurrentOutputFb);
    #endif
    end();

    pixman_region32_fini(&fakeDamage);

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

    begin(PMONITOR, &fakeDamage, true);

    const auto PFRAMEBUFFER = &m_mLayerFramebuffers[pLayer];

    glViewport(0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y);

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    PFRAMEBUFFER->bind();

    clear(CColor(0, 0, 0, 0));  // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    m_bEndFrame = true;

    // draw the layer
    g_pHyprRenderer->renderLayer(pLayer, PMONITOR, &now);

    m_bEndFrame = false;

    // TODO: WARN:
    // revise if any stencil-requiring rendering is done to the layers.

// restore original fb
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iCurrentOutputFb);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iCurrentOutputFb);
#endif

    end();

    pixman_region32_fini(&fakeDamage);

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

    wlr_box windowBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->scale * PWINDOW->m_vRealSize.vec().x / (PWINDOW->m_vOriginalClosedSize.x * PMONITOR->scale)), (PMONITOR->scale * PWINDOW->m_vRealSize.vec().y / (PWINDOW->m_vOriginalClosedSize.y * PMONITOR->scale)));

    // TODO: this is wrong on scaled.

    windowBox.width = PMONITOR->vecTransformedSize.x * scaleXY.x;
    windowBox.height = PMONITOR->vecTransformedSize.y * scaleXY.y;
    windowBox.x = ((PWINDOW->m_vRealPosition.vec().x - PMONITOR->vecPosition.x) * PMONITOR->scale) - ((PWINDOW->m_vOriginalClosedPos.x * PMONITOR->scale) * scaleXY.x);
    windowBox.y = ((PWINDOW->m_vRealPosition.vec().y - PMONITOR->vecPosition.y) * PMONITOR->scale) - ((PWINDOW->m_vOriginalClosedPos.y * PMONITOR->scale) * scaleXY.y);

    pixman_region32_t fakeDamage;
    pixman_region32_init_rect(&fakeDamage, 0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y);

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

    wlr_box windowBox = {0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    pixman_region32_t fakeDamage;
    pixman_region32_init_rect(&fakeDamage, 0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y);

    renderTextureInternalWithDamage(it->second.m_cTex, &windowBox, PLAYER->alpha.fl(), &fakeDamage, 0);

    pixman_region32_fini(&fakeDamage);
}

void CHyprOpenGLImpl::renderRoundedShadow(wlr_box* box, int round, int range, float a) {
    RASSERT(m_RenderData.pMonitor, "Tried to render shadow without begin()!");
    RASSERT((box->width > 0 && box->height > 0), "Tried to render shadow with width/height < 0!");
    RASSERT(m_pCurrentWindow, "Tried to render shadow without a window!");

    static auto *const PSHADOWPOWER = &g_pConfigManager->getConfigValuePtr("decoration:shadow_render_power")->intValue;

    const auto SHADOWPOWER = std::clamp((int)*PSHADOWPOWER, 1, 4);

    const auto col = m_pCurrentWindow->m_cRealShadowColor.col();

    float matrix[9];
    wlr_matrix_project_box(matrix, box, wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), 0, m_RenderData.pMonitor->output->transform_matrix);  // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);
    wlr_matrix_multiply(glMatrix, matrixFlip180, glMatrix);

    wlr_matrix_transpose(glMatrix, glMatrix);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_shSHADOW.program);

    glUniformMatrix3fv(m_shSHADOW.proj, 1, GL_FALSE, glMatrix);
    glUniform4f(m_shSHADOW.getUniformLocation("color"), col.r / 255.f, col.g / 255.f, col.b / 255.f, col.a / 255.f * a);

    const auto TOPLEFT = Vector2D(range + round, range + round);
    const auto BOTTOMRIGHT = Vector2D(box->width - (range + round), box->height - (range + round));
    const auto FULLSIZE = Vector2D(box->width, box->height);

    // Rounded corners
    glUniform2f(m_shSHADOW.getUniformLocation("topLeft"), (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(m_shSHADOW.getUniformLocation("bottomRight"), (float)BOTTOMRIGHT.x, (float)BOTTOMRIGHT.y);
    glUniform2f(m_shSHADOW.getUniformLocation("fullSize"), (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(m_shSHADOW.getUniformLocation("radius"), range + round);
    glUniform1f(m_shSHADOW.getUniformLocation("range"), range);
    glUniform1f(m_shSHADOW.getUniformLocation("shadowPower"), SHADOWPOWER);

    glVertexAttribPointer(m_shSHADOW.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(m_shSHADOW.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_shSHADOW.posAttrib);
    glEnableVertexAttribArray(m_shSHADOW.texAttrib);

    if (pixman_region32_not_empty(m_RenderData.pDamage)) {
        PIXMAN_DAMAGE_FOREACH(m_RenderData.pDamage) {
            const auto RECT = RECTSARR[i];
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(m_shSHADOW.posAttrib);
    glDisableVertexAttribArray(m_shSHADOW.texAttrib);

    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void CHyprOpenGLImpl::renderSplash(cairo_t *const CAIRO, cairo_surface_t *const CAIROSURFACE) {
    cairo_select_font_face(CAIRO, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    const auto FONTSIZE = (int)(m_RenderData.pMonitor->vecPixelSize.y / 76); 
    cairo_set_font_size(CAIRO, FONTSIZE);

    cairo_set_source_rgba(CAIRO, 1.f, 1.f, 1.f, 0.32f);

    cairo_text_extents_t textExtents;
    cairo_text_extents(CAIRO, g_pCompositor->m_szCurrentSplash.c_str(), &textExtents);

    cairo_move_to(CAIRO, m_RenderData.pMonitor->vecPixelSize.x / 2.f - textExtents.width / 2.f, m_RenderData.pMonitor->vecPixelSize.y - textExtents.height - 1);
    cairo_show_text(CAIRO, g_pCompositor->m_szCurrentSplash.c_str());

    cairo_surface_flush(CAIROSURFACE);
}

void CHyprOpenGLImpl::createBGTextureForMonitor(SMonitor* pMonitor) {
    RASSERT(m_RenderData.pMonitor, "Tried to createBGTex without begin()!");

    static auto *const PNOSPLASH = &g_pConfigManager->getConfigValuePtr("misc:disable_splash_rendering")->intValue;

    // release the last tex if exists
    const auto PTEX = &m_mMonitorBGTextures[pMonitor];
    PTEX->destroyTexture();

    PTEX->allocate();

    Debug::log(LOG, "Allocated texture for BGTex");

    // TODO: use relative paths to the installation
    // or configure the paths at build time

    // check if wallpapers exist
    if (!std::filesystem::exists("/usr/share/hyprland/wall_8K.png"))
        return; // the texture will be empty, oh well. We'll clear with a solid color anyways.

    // get the adequate tex
    std::string texPath = "/usr/share/hyprland/wall_";
    Vector2D textureSize;
    if (pMonitor->vecTransformedSize.x > 7000) {
        textureSize = Vector2D(7680, 4320);
        texPath += "8K.png";
    } else if (pMonitor->vecTransformedSize.x > 3000) {
        textureSize = Vector2D(3840, 2160);
        texPath += "4K.png";
    } else {
        textureSize = Vector2D(1920, 1080);
        texPath += "2K.png";
    }

    // create a new one with cairo
    const auto CAIROSURFACE = cairo_image_surface_create_from_png(texPath.c_str());

    const auto CAIRO = cairo_create(CAIROSURFACE);

    if (!*PNOSPLASH)
        renderSplash(CAIRO, CAIROSURFACE);

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

    static auto *const PRENDERTEX = &g_pConfigManager->getConfigValuePtr("misc:disable_hyprland_logo")->intValue;

    if (!*PRENDERTEX) {
        wlr_box box = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
        renderTexture(m_mMonitorBGTextures[m_RenderData.pMonitor], &box, 255, 0);
    }
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
