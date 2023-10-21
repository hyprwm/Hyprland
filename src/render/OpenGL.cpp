#include "Shaders.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "Shaders.hpp"
#include <random>

CHyprOpenGLImpl::CHyprOpenGLImpl() {
    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL)),
            "Couldn't unset current EGL!");

    auto* const EXTENSIONS = (const char*)glGetString(GL_EXTENSIONS);

    RASSERT(EXTENSIONS, "Couldn't retrieve openGL extensions!");

    m_iDRMFD = g_pCompositor->m_iDRMFD;

    m_szExtensions = EXTENSIONS;

    Debug::log(LOG, "Creating the Hypr OpenGL Renderer!");
    Debug::log(LOG, "Using: {}", (char*)glGetString(GL_VERSION));
    Debug::log(LOG, "Vendor: {}", (char*)glGetString(GL_VENDOR));
    Debug::log(LOG, "Renderer: {}", (char*)glGetString(GL_RENDERER));
    Debug::log(LOG, "Supported extensions size: {}", std::count(m_szExtensions.begin(), m_szExtensions.end(), ' '));

#ifdef USE_TRACY_GPU

    loadGLProc(&glQueryCounter, "glQueryCounterEXT");
    loadGLProc(&glGetQueryObjectiv, "glGetQueryObjectivEXT");
    loadGLProc(&glGetQueryObjectui64v, "glGetQueryObjectui64vEXT");

#endif

    TRACY_GPU_CONTEXT;

#ifdef GLES2
    Debug::log(WARN, "!RENDERER: Using the legacy GLES2 renderer!");
#endif

    g_pHookSystem->hookDynamic("preRender", [&](void* self, SCallbackInfo& info, std::any data) { preRender(std::any_cast<CMonitor*>(data)); });

    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT), "Couldn't unset current EGL!");

    m_tGlobalTimer.reset();
}

GLuint CHyprOpenGLImpl::createProgram(const std::string& vert, const std::string& frag, bool dynamic) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert, dynamic);
    if (dynamic) {
        if (vertCompiled == 0)
            return 0;
    } else {
        RASSERT(vertCompiled, "Compiling shader failed. VERTEX NULL! Shader source:\n\n{}", vert.c_str());
    }

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag, dynamic);
    if (dynamic) {
        if (fragCompiled == 0)
            return 0;
    } else {
        RASSERT(fragCompiled, "Compiling shader failed. FRAGMENT NULL! Shader source:\n\n{}", frag.c_str());
    }

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
    if (dynamic) {
        if (ok == GL_FALSE)
            return 0;
    } else {
        RASSERT(ok != GL_FALSE, "createProgram() failed! GL_LINK_STATUS not OK!");
    }

    return prog;
}

GLuint CHyprOpenGLImpl::compileShader(const GLuint& type, std::string src, bool dynamic) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (dynamic) {
        if (ok == GL_FALSE)
            return 0;
    } else {
        RASSERT(ok != GL_FALSE, "compileShader() failed! GL_COMPILE_STATUS not OK!");
    }

    return shader;
}

void CHyprOpenGLImpl::begin(CMonitor* pMonitor, CRegion* pDamage, bool fake) {
    m_RenderData.pMonitor = pMonitor;

    TRACY_GPU_ZONE("RenderBegin");

    if (eglGetCurrentContext() != wlr_egl_get_context(g_pCompositor->m_sWLREGL)) {
        eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL));
    }

    glViewport(0, 0, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);

    matrixProjection(m_RenderData.projection, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, WL_OUTPUT_TRANSFORM_NORMAL);

    m_RenderData.pCurrentMonData = &m_mMonitorRenderResources[pMonitor];

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_iCurrentOutputFb);
    m_iWLROutputFb = m_iCurrentOutputFb;

    // ensure a framebuffer for the monitor exists
    if (!m_mMonitorRenderResources.contains(pMonitor) || m_RenderData.pCurrentMonData->primaryFB.m_vSize != pMonitor->vecPixelSize) {
        m_RenderData.pCurrentMonData->stencilTex.allocate();

        m_RenderData.pCurrentMonData->primaryFB.m_pStencilTex    = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->mirrorFB.m_pStencilTex     = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->mirrorSwapFB.m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->offMainFB.m_pStencilTex    = &m_RenderData.pCurrentMonData->stencilTex;

        m_RenderData.pCurrentMonData->primaryFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);
        m_RenderData.pCurrentMonData->mirrorFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);
        m_RenderData.pCurrentMonData->mirrorSwapFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);
        m_RenderData.pCurrentMonData->offMainFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);

        createBGTextureForMonitor(pMonitor);
    }

    if (m_RenderData.pCurrentMonData->monitorMirrorFB.isAllocated() && m_RenderData.pMonitor->mirrors.empty())
        m_RenderData.pCurrentMonData->monitorMirrorFB.release();

    if (!m_RenderData.pCurrentMonData->m_bShadersInitialized)
        initShaders();

    // bind the primary Hypr Framebuffer
    m_RenderData.pCurrentMonData->primaryFB.bind();

    m_RenderData.damage.set(*pDamage);

    m_bFakeFrame = fake;

    m_RenderData.currentFB = &m_RenderData.pCurrentMonData->primaryFB;

    if (m_bReloadScreenShader) {
        m_bReloadScreenShader = false;
        applyScreenShader(g_pConfigManager->getString("decoration:screen_shader"));
    }
}

void CHyprOpenGLImpl::end() {
    static auto* const PZOOMRIGID = &g_pConfigManager->getConfigValuePtr("misc:cursor_zoom_rigid")->intValue;

    TRACY_GPU_ZONE("RenderEnd");

    // end the render, copy the data to the WLR framebuffer
    if (!m_bFakeFrame) {
        m_RenderData.damage = m_RenderData.pMonitor->lastFrameDamage;

        if (!m_RenderData.pMonitor->mirrors.empty())
            saveBufferForMirror(); // save with original damage region

        glBindFramebuffer(GL_FRAMEBUFFER, m_iWLROutputFb);
        wlr_box monbox = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};

        if (m_RenderData.mouseZoomFactor != 1.f) {
            const auto ZOOMCENTER = m_RenderData.mouseZoomUseMouse ?
                (g_pInputManager->getMouseCoordsInternal() - m_RenderData.pMonitor->vecPosition) * m_RenderData.pMonitor->scale :
                m_RenderData.pMonitor->vecTransformedSize / 2.f;
            monbox.x -= ZOOMCENTER.x;
            monbox.y -= ZOOMCENTER.y;
            scaleBox(&monbox, m_RenderData.mouseZoomFactor);
            monbox.x += *PZOOMRIGID ? m_RenderData.pMonitor->vecTransformedSize.x / 2 : ZOOMCENTER.x;
            monbox.y += *PZOOMRIGID ? m_RenderData.pMonitor->vecTransformedSize.y / 2 : ZOOMCENTER.y;

            if (monbox.x > 0)
                monbox.x = 0;
            if (monbox.y > 0)
                monbox.y = 0;
            if (monbox.x + monbox.width < m_RenderData.pMonitor->vecTransformedSize.x)
                monbox.x = m_RenderData.pMonitor->vecTransformedSize.x - monbox.width;
            if (monbox.y + monbox.height < m_RenderData.pMonitor->vecTransformedSize.y)
                monbox.y = m_RenderData.pMonitor->vecTransformedSize.y - monbox.height;
        }

        m_bEndFrame         = true;
        m_bApplyFinalShader = true;
        if (m_RenderData.mouseZoomUseMouse)
            m_RenderData.useNearestNeighbor = true;

        blend(false);

        if (m_sFinalScreenShader.program < 1)
            renderTexturePrimitive(m_RenderData.pCurrentMonData->primaryFB.m_cTex, &monbox);
        else
            renderTexture(m_RenderData.pCurrentMonData->primaryFB.m_cTex, &monbox, 1.f);

        blend(true);

        m_RenderData.useNearestNeighbor = false;
        m_bApplyFinalShader             = false;
        m_bEndFrame                     = false;
    }

    // reset our data
    m_RenderData.pMonitor          = nullptr;
    m_iWLROutputFb                 = 0;
    m_RenderData.mouseZoomFactor   = 1.f;
    m_RenderData.mouseZoomUseMouse = true;
}

void CHyprOpenGLImpl::bindWlrOutputFb() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_iWLROutputFb);
}

void CHyprOpenGLImpl::initShaders() {
    GLuint prog                                      = createProgram(QUADVERTSRC, QUADFRAGSRC);
    m_RenderData.pCurrentMonData->m_shQUAD.program   = prog;
    m_RenderData.pCurrentMonData->m_shQUAD.proj      = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shQUAD.color     = glGetUniformLocation(prog, "color");
    m_RenderData.pCurrentMonData->m_shQUAD.posAttrib = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shQUAD.topLeft   = glGetUniformLocation(prog, "topLeft");
    m_RenderData.pCurrentMonData->m_shQUAD.fullSize  = glGetUniformLocation(prog, "fullSize");
    m_RenderData.pCurrentMonData->m_shQUAD.radius    = glGetUniformLocation(prog, "radius");

    prog                                                     = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA);
    m_RenderData.pCurrentMonData->m_shRGBA.program           = prog;
    m_RenderData.pCurrentMonData->m_shRGBA.proj              = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shRGBA.tex               = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shRGBA.alpha             = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shRGBA.texAttrib         = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shRGBA.posAttrib         = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shRGBA.discardOpaque     = glGetUniformLocation(prog, "discardOpaque");
    m_RenderData.pCurrentMonData->m_shRGBA.discardAlpha      = glGetUniformLocation(prog, "discardAlpha");
    m_RenderData.pCurrentMonData->m_shRGBA.discardAlphaValue = glGetUniformLocation(prog, "discardAlphaValue");
    m_RenderData.pCurrentMonData->m_shRGBA.topLeft           = glGetUniformLocation(prog, "topLeft");
    m_RenderData.pCurrentMonData->m_shRGBA.fullSize          = glGetUniformLocation(prog, "fullSize");
    m_RenderData.pCurrentMonData->m_shRGBA.radius            = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shRGBA.applyTint         = glGetUniformLocation(prog, "applyTint");
    m_RenderData.pCurrentMonData->m_shRGBA.tint              = glGetUniformLocation(prog, "tint");

    prog                                                     = createProgram(TEXVERTSRC, TEXFRAGSRCRGBAPASSTHRU);
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.program   = prog;
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.proj      = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.tex       = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.posAttrib = glGetAttribLocation(prog, "pos");

    prog                                               = createProgram(TEXVERTSRC, FRAGGLITCH);
    m_RenderData.pCurrentMonData->m_shGLITCH.program   = prog;
    m_RenderData.pCurrentMonData->m_shGLITCH.proj      = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shGLITCH.tex       = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shGLITCH.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shGLITCH.posAttrib = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shGLITCH.distort   = glGetUniformLocation(prog, "distort");
    m_RenderData.pCurrentMonData->m_shGLITCH.time      = glGetUniformLocation(prog, "time");
    m_RenderData.pCurrentMonData->m_shGLITCH.fullSize  = glGetUniformLocation(prog, "screenSize");

    prog                                                     = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX);
    m_RenderData.pCurrentMonData->m_shRGBX.program           = prog;
    m_RenderData.pCurrentMonData->m_shRGBX.tex               = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shRGBX.proj              = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shRGBX.alpha             = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shRGBX.texAttrib         = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shRGBX.posAttrib         = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shRGBX.discardOpaque     = glGetUniformLocation(prog, "discardOpaque");
    m_RenderData.pCurrentMonData->m_shRGBX.discardAlpha      = glGetUniformLocation(prog, "discardAlpha");
    m_RenderData.pCurrentMonData->m_shRGBX.discardAlphaValue = glGetUniformLocation(prog, "discardAlphaValue");
    m_RenderData.pCurrentMonData->m_shRGBX.topLeft           = glGetUniformLocation(prog, "topLeft");
    m_RenderData.pCurrentMonData->m_shRGBX.fullSize          = glGetUniformLocation(prog, "fullSize");
    m_RenderData.pCurrentMonData->m_shRGBX.radius            = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shRGBX.applyTint         = glGetUniformLocation(prog, "applyTint");
    m_RenderData.pCurrentMonData->m_shRGBX.tint              = glGetUniformLocation(prog, "tint");

    prog                                                    = createProgram(TEXVERTSRC, TEXFRAGSRCEXT);
    m_RenderData.pCurrentMonData->m_shEXT.program           = prog;
    m_RenderData.pCurrentMonData->m_shEXT.tex               = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shEXT.proj              = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shEXT.alpha             = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shEXT.posAttrib         = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shEXT.texAttrib         = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shEXT.discardOpaque     = glGetUniformLocation(prog, "discardOpaque");
    m_RenderData.pCurrentMonData->m_shEXT.discardAlpha      = glGetUniformLocation(prog, "discardAlpha");
    m_RenderData.pCurrentMonData->m_shEXT.discardAlphaValue = glGetUniformLocation(prog, "discardAlphaValue");
    m_RenderData.pCurrentMonData->m_shEXT.topLeft           = glGetUniformLocation(prog, "topLeft");
    m_RenderData.pCurrentMonData->m_shEXT.fullSize          = glGetUniformLocation(prog, "fullSize");
    m_RenderData.pCurrentMonData->m_shEXT.radius            = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shEXT.applyTint         = glGetUniformLocation(prog, "applyTint");
    m_RenderData.pCurrentMonData->m_shEXT.tint              = glGetUniformLocation(prog, "tint");

    prog                                              = createProgram(TEXVERTSRC, FRAGBLUR1);
    m_RenderData.pCurrentMonData->m_shBLUR1.program   = prog;
    m_RenderData.pCurrentMonData->m_shBLUR1.tex       = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shBLUR1.alpha     = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shBLUR1.proj      = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBLUR1.posAttrib = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBLUR1.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBLUR1.radius    = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shBLUR1.halfpixel = glGetUniformLocation(prog, "halfpixel");

    prog                                              = createProgram(TEXVERTSRC, FRAGBLUR2);
    m_RenderData.pCurrentMonData->m_shBLUR2.program   = prog;
    m_RenderData.pCurrentMonData->m_shBLUR2.tex       = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shBLUR2.alpha     = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shBLUR2.proj      = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBLUR2.posAttrib = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBLUR2.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBLUR2.radius    = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shBLUR2.halfpixel = glGetUniformLocation(prog, "halfpixel");

    prog                                                    = createProgram(TEXVERTSRC, FRAGBLURFINISH);
    m_RenderData.pCurrentMonData->m_shBLURFINISH.program    = prog;
    m_RenderData.pCurrentMonData->m_shBLURFINISH.tex        = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.proj       = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib  = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib  = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.noise      = glGetUniformLocation(prog, "noise");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.contrast   = glGetUniformLocation(prog, "contrast");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.brightness = glGetUniformLocation(prog, "brightness");

    prog                                                 = createProgram(QUADVERTSRC, FRAGSHADOW);
    m_RenderData.pCurrentMonData->m_shSHADOW.program     = prog;
    m_RenderData.pCurrentMonData->m_shSHADOW.proj        = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shSHADOW.posAttrib   = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shSHADOW.texAttrib   = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shSHADOW.topLeft     = glGetUniformLocation(prog, "topLeft");
    m_RenderData.pCurrentMonData->m_shSHADOW.bottomRight = glGetUniformLocation(prog, "bottomRight");
    m_RenderData.pCurrentMonData->m_shSHADOW.fullSize    = glGetUniformLocation(prog, "fullSize");
    m_RenderData.pCurrentMonData->m_shSHADOW.radius      = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shSHADOW.range       = glGetUniformLocation(prog, "range");
    m_RenderData.pCurrentMonData->m_shSHADOW.shadowPower = glGetUniformLocation(prog, "shadowPower");
    m_RenderData.pCurrentMonData->m_shSHADOW.color       = glGetUniformLocation(prog, "color");

    prog                                                            = createProgram(QUADVERTSRC, FRAGBORDER1);
    m_RenderData.pCurrentMonData->m_shBORDER1.program               = prog;
    m_RenderData.pCurrentMonData->m_shBORDER1.proj                  = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBORDER1.thick                 = glGetUniformLocation(prog, "thick");
    m_RenderData.pCurrentMonData->m_shBORDER1.posAttrib             = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBORDER1.texAttrib             = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBORDER1.topLeft               = glGetUniformLocation(prog, "topLeft");
    m_RenderData.pCurrentMonData->m_shBORDER1.bottomRight           = glGetUniformLocation(prog, "bottomRight");
    m_RenderData.pCurrentMonData->m_shBORDER1.fullSize              = glGetUniformLocation(prog, "fullSize");
    m_RenderData.pCurrentMonData->m_shBORDER1.fullSizeUntransformed = glGetUniformLocation(prog, "fullSizeUntransformed");
    m_RenderData.pCurrentMonData->m_shBORDER1.radius                = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shBORDER1.radiusOuter           = glGetUniformLocation(prog, "radiusOuter");
    m_RenderData.pCurrentMonData->m_shBORDER1.gradient              = glGetUniformLocation(prog, "gradient");
    m_RenderData.pCurrentMonData->m_shBORDER1.gradientLength        = glGetUniformLocation(prog, "gradientLength");
    m_RenderData.pCurrentMonData->m_shBORDER1.angle                 = glGetUniformLocation(prog, "angle");
    m_RenderData.pCurrentMonData->m_shBORDER1.alpha                 = glGetUniformLocation(prog, "alpha");

    m_RenderData.pCurrentMonData->m_bShadersInitialized = true;

    Debug::log(LOG, "Shaders initialized successfully.");
}

void CHyprOpenGLImpl::applyScreenShader(const std::string& path) {

    m_sFinalScreenShader.destroy();

    if (path == "" || path == STRVAL_EMPTY)
        return;

    std::ifstream infile(absolutePath(path, g_pConfigManager->getConfigDir()));

    if (!infile.good()) {
        g_pConfigManager->addParseError("Screen shader parser: Screen shader path not found");
        return;
    }

    std::string fragmentShader((std::istreambuf_iterator<char>(infile)), (std::istreambuf_iterator<char>()));

    m_sFinalScreenShader.program = createProgram(TEXVERTSRC, fragmentShader, true);

    if (!m_sFinalScreenShader.program) {
        g_pConfigManager->addParseError("Screen shader parser: Screen shader parse failed");
        return;
    }

    m_sFinalScreenShader.proj   = glGetUniformLocation(m_sFinalScreenShader.program, "proj");
    m_sFinalScreenShader.tex    = glGetUniformLocation(m_sFinalScreenShader.program, "tex");
    m_sFinalScreenShader.time   = glGetUniformLocation(m_sFinalScreenShader.program, "time");
    m_sFinalScreenShader.output = glGetUniformLocation(m_sFinalScreenShader.program, "output");
    if (m_sFinalScreenShader.time != -1 && g_pConfigManager->getInt("debug:damage_tracking") != 0 && !g_pHyprRenderer->m_bCrashingInProgress) {
        // The screen shader uses the "time" uniform
        // Since the screen shader could change every frame, damage tracking *needs* to be disabled
        g_pConfigManager->addParseError("Screen shader: Screen shader uses uniform 'time', which requires debug:damage_tracking to be switched off.\n"
                                        "WARNING: Disabling damage tracking will *massively* increase GPU utilization!");
    }
    m_sFinalScreenShader.texAttrib = glGetAttribLocation(m_sFinalScreenShader.program, "texcoord");
    m_sFinalScreenShader.posAttrib = glGetAttribLocation(m_sFinalScreenShader.program, "pos");
}

void CHyprOpenGLImpl::clear(const CColor& color) {
    RASSERT(m_RenderData.pMonitor, "Tried to render without begin()!");

    TRACY_GPU_ZONE("RenderClear");

    glClearColor(color.r, color.g, color.b, color.a);

    if (!m_RenderData.damage.empty()) {
        for (auto& RECT : m_RenderData.damage.getRects()) {
            scissor(&RECT);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    scissor((wlr_box*)nullptr);
}

void CHyprOpenGLImpl::blend(bool enabled) {
    if (enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // everything is premultiplied
    } else
        glDisable(GL_BLEND);

    m_bBlend = enabled;
}

void CHyprOpenGLImpl::scissor(const wlr_box* pBox, bool transform) {
    RASSERT(m_RenderData.pMonitor, "Tried to scissor without begin()!");

    if (!pBox) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    wlr_box newBox = *pBox;

    if (transform) {
        int w, h;
        wlr_output_transformed_resolution(m_RenderData.pMonitor->output, &w, &h);

        const auto TR = wlr_output_transform_invert(m_RenderData.pMonitor->transform);
        wlr_box_transform(&newBox, &newBox, TR, w, h);
    }

    glScissor(newBox.x, newBox.y, newBox.width, newBox.height);
    glEnable(GL_SCISSOR_TEST);
}

void CHyprOpenGLImpl::scissor(const pixman_box32* pBox, bool transform) {
    RASSERT(m_RenderData.pMonitor, "Tried to scissor without begin()!");

    if (!pBox) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    wlr_box newBox = {pBox->x1, pBox->y1, pBox->x2 - pBox->x1, pBox->y2 - pBox->y1};

    scissor(&newBox, transform);
}

void CHyprOpenGLImpl::scissor(const int x, const int y, const int w, const int h, bool transform) {
    wlr_box box = {x, y, w, h};
    scissor(&box, transform);
}

void CHyprOpenGLImpl::renderRect(wlr_box* box, const CColor& col, int round) {
    if (!m_RenderData.damage.empty())
        renderRectWithDamage(box, col, &m_RenderData.damage, round);
}

void CHyprOpenGLImpl::renderRectWithBlur(wlr_box* box, const CColor& col, int round, float blurA) {
    if (m_RenderData.damage.empty())
        return;

    CRegion damage{m_RenderData.damage};
    damage.intersect(box);

    CFramebuffer* POUTFB = blurMainFramebufferWithDamage(blurA, &damage);

    // bind primary
    m_RenderData.pCurrentMonData->primaryFB.bind();

    // make a stencil for rounded corners to work with blur
    scissor((wlr_box*)nullptr); // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    renderRect(box, CColor(0, 0, 0, 0), round);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_EQUAL, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    scissor(box);
    wlr_box MONITORBOX          = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    m_bEndFrame                 = true; // fix transformed
    const auto SAVEDRENDERMODIF = m_RenderData.renderModif;
    m_RenderData.renderModif    = {}; // fix shit
    renderTextureInternalWithDamage(POUTFB->m_cTex, &MONITORBOX, blurA, &damage, 0, false, false, false);
    m_bEndFrame              = false;
    m_RenderData.renderModif = SAVEDRENDERMODIF;

    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    glDisable(GL_STENCIL_TEST);
    glStencilMask(-1);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    scissor((wlr_box*)nullptr);

    renderRectWithDamage(box, col, &m_RenderData.damage, round);
}

void CHyprOpenGLImpl::renderRectWithDamage(wlr_box* box, const CColor& col, CRegion* damage, int round) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderRectWithDamage");

    wlr_box newBox = *box;
    scaleBox(&newBox, m_RenderData.renderModif.scale);
    newBox.x += m_RenderData.renderModif.translate.x;
    newBox.y += m_RenderData.renderModif.translate.y;

    box = &newBox;

    float matrix[9];
    wlr_matrix_project_box(matrix, box, wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), 0,
                           m_RenderData.pMonitor->output->transform_matrix); // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    glUseProgram(m_RenderData.pCurrentMonData->m_shQUAD.program);

#ifndef GLES2
    glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shQUAD.proj, 1, GL_TRUE, glMatrix);
#else
    wlr_matrix_transpose(glMatrix, glMatrix);
    glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shQUAD.proj, 1, GL_FALSE, glMatrix);
#endif

    // premultiply the color as well as we don't work with straight alpha
    glUniform4f(m_RenderData.pCurrentMonData->m_shQUAD.color, col.r * col.a, col.g * col.a, col.b * col.a, col.a);

    wlr_box transformedBox;
    wlr_box_transform(&transformedBox, box, wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
                      m_RenderData.pMonitor->vecTransformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    // Rounded corners
    glUniform2f(m_RenderData.pCurrentMonData->m_shQUAD.topLeft, (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(m_RenderData.pCurrentMonData->m_shQUAD.fullSize, (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(m_RenderData.pCurrentMonData->m_shQUAD.radius, round);

    glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shQUAD.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shQUAD.posAttrib);

    if (m_RenderData.clipBox.width != 0 && m_RenderData.clipBox.height != 0) {
        CRegion damageClip{m_RenderData.clipBox.x, m_RenderData.clipBox.y, m_RenderData.clipBox.width, m_RenderData.clipBox.height};
        damageClip.intersect(*damage);

        if (!damageClip.empty()) {
            for (auto& RECT : damageClip.getRects()) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
    } else {
        for (auto& RECT : damage->getRects()) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shQUAD.posAttrib);
}

void CHyprOpenGLImpl::renderTexture(wlr_texture* tex, wlr_box* pBox, float alpha, int round, bool allowCustomUV) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTexture(CTexture(tex), pBox, alpha, round, false, allowCustomUV);
}

void CHyprOpenGLImpl::renderTexture(const CTexture& tex, wlr_box* pBox, float alpha, int round, bool discardActive, bool allowCustomUV) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTextureInternalWithDamage(tex, pBox, alpha, &m_RenderData.damage, round, discardActive, false, allowCustomUV, true);

    scissor((wlr_box*)nullptr);
}

void CHyprOpenGLImpl::renderTextureInternalWithDamage(const CTexture& tex, wlr_box* pBox, float alpha, CRegion* damage, int round, bool discardActive, bool noAA,
                                                      bool allowCustomUV, bool allowDim) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    TRACY_GPU_ZONE("RenderTextureInternalWithDamage");

    alpha = std::clamp(alpha, 0.f, 1.f);

    if (m_RenderData.damage.empty())
        return;

    wlr_box newBox = *pBox;
    scaleBox(&newBox, m_RenderData.renderModif.scale);
    newBox.x += m_RenderData.renderModif.translate.x;
    newBox.y += m_RenderData.renderModif.translate.y;

    static auto* const PDIMINACTIVE = &g_pConfigManager->getConfigValuePtr("decoration:dim_inactive")->intValue;

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform);
    float      matrix[9];
    wlr_matrix_project_box(matrix, &newBox, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    CShader*   shader = nullptr;

    bool       usingFinalShader = false;

    const bool CRASHING = m_bApplyFinalShader && g_pHyprRenderer->m_bCrashingInProgress;

    if (CRASHING) {
        shader           = &m_RenderData.pCurrentMonData->m_shGLITCH;
        usingFinalShader = true;
    } else if (m_bApplyFinalShader && m_sFinalScreenShader.program) {
        shader           = &m_sFinalScreenShader;
        usingFinalShader = true;
    } else {
        if (m_bApplyFinalShader) {
            shader           = &m_RenderData.pCurrentMonData->m_shPASSTHRURGBA;
            usingFinalShader = true;
        } else {
            switch (tex.m_iType) {
                case TEXTURE_RGBA: shader = &m_RenderData.pCurrentMonData->m_shRGBA; break;
                case TEXTURE_RGBX: shader = &m_RenderData.pCurrentMonData->m_shRGBX; break;
                case TEXTURE_EXTERNAL: shader = &m_RenderData.pCurrentMonData->m_shEXT; break;
                default: RASSERT(false, "tex.m_iTarget unsupported!");
            }
        }
    }

    if (m_pCurrentWindow && m_pCurrentWindow->m_sAdditionalConfigData.forceRGBX)
        shader = &m_RenderData.pCurrentMonData->m_shRGBX;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex.m_iTarget, tex.m_iTexID);

    if (m_RenderData.useNearestNeighbor) {
        glTexParameteri(tex.m_iTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(tex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    } else {
        glTexParameteri(tex.m_iTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(tex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    glUseProgram(shader->program);

#ifndef GLES2
    glUniformMatrix3fv(shader->proj, 1, GL_TRUE, glMatrix);
#else
    wlr_matrix_transpose(glMatrix, glMatrix);
    glUniformMatrix3fv(shader->proj, 1, GL_FALSE, glMatrix);
#endif
    glUniform1i(shader->tex, 0);

    if ((usingFinalShader && g_pConfigManager->getInt("debug:damage_tracking") == 0) || CRASHING) {
        glUniform1f(shader->time, m_tGlobalTimer.getSeconds());
    } else if (usingFinalShader && shader->time != -1) {
        // Don't let time be unitialised
        glUniform1f(shader->time, 0.f);
    }

    if (usingFinalShader && shader->output != -1)
        glUniform1i(shader->output, m_RenderData.pMonitor->ID);

    if (CRASHING) {
        glUniform1f(shader->distort, g_pHyprRenderer->m_fCrashingDistort);
        glUniform2f(shader->fullSize, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y);
    }

    if (!usingFinalShader) {
        glUniform1f(shader->alpha, alpha);

        if (discardActive) {
            glUniform1i(shader->discardOpaque, !!(m_RenderData.discardMode & DISCARD_OPAQUE));
            glUniform1i(shader->discardAlpha, !!(m_RenderData.discardMode & DISCARD_ALPHA));
            glUniform1f(shader->discardAlphaValue, m_RenderData.discardOpacity);
        } else {
            glUniform1i(shader->discardOpaque, 0);
            glUniform1i(shader->discardAlpha, 0);
        }
    }

    wlr_box transformedBox;
    wlr_box_transform(&transformedBox, &newBox, wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
                      m_RenderData.pMonitor->vecTransformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    if (!usingFinalShader) {
        // Rounded corners
        glUniform2f(shader->topLeft, TOPLEFT.x, TOPLEFT.y);
        glUniform2f(shader->fullSize, FULLSIZE.x, FULLSIZE.y);
        glUniform1f(shader->radius, round);

        if (allowDim && m_pCurrentWindow && *PDIMINACTIVE) {
            glUniform1i(shader->applyTint, 1);
            const auto DIM = m_pCurrentWindow->m_fDimPercent.fl();
            glUniform3f(shader->tint, 1.f - DIM, 1.f - DIM, 1.f - DIM);
        } else {
            glUniform1i(shader->applyTint, 0);
        }
    }

    const float verts[] = {
        m_RenderData.primarySurfaceUVBottomRight.x, m_RenderData.primarySurfaceUVTopLeft.y,     // top right
        m_RenderData.primarySurfaceUVTopLeft.x,     m_RenderData.primarySurfaceUVTopLeft.y,     // top left
        m_RenderData.primarySurfaceUVBottomRight.x, m_RenderData.primarySurfaceUVBottomRight.y, // bottom right
        m_RenderData.primarySurfaceUVTopLeft.x,     m_RenderData.primarySurfaceUVBottomRight.y, // bottom left
    };

    glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    if (allowCustomUV && m_RenderData.primarySurfaceUVTopLeft != Vector2D(-1, -1)) {
        glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
    } else {
        glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    }

    glEnableVertexAttribArray(shader->posAttrib);
    glEnableVertexAttribArray(shader->texAttrib);

    if (m_RenderData.clipBox.width != 0 && m_RenderData.clipBox.height != 0) {
        CRegion damageClip{m_RenderData.clipBox.x, m_RenderData.clipBox.y, m_RenderData.clipBox.width, m_RenderData.clipBox.height};
        damageClip.intersect(*damage);

        if (!damageClip.empty()) {
            for (auto& RECT : damageClip.getRects()) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
    } else {
        for (auto& RECT : damage->getRects()) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(shader->posAttrib);
    glDisableVertexAttribArray(shader->texAttrib);

    glBindTexture(tex.m_iTarget, 0);
}

void CHyprOpenGLImpl::renderTexturePrimitive(const CTexture& tex, wlr_box* pBox) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    TRACY_GPU_ZONE("RenderTexturePrimitive");

    if (m_RenderData.damage.empty())
        return;

    wlr_box newBox = *pBox;
    scaleBox(&newBox, m_RenderData.renderModif.scale);
    newBox.x += m_RenderData.renderModif.translate.x;
    newBox.y += m_RenderData.renderModif.translate.y;

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform);
    float      matrix[9];
    wlr_matrix_project_box(matrix, &newBox, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    CShader* shader = &m_RenderData.pCurrentMonData->m_shPASSTHRURGBA;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex.m_iTarget, tex.m_iTexID);

    glUseProgram(shader->program);

#ifndef GLES2
    glUniformMatrix3fv(shader->proj, 1, GL_TRUE, glMatrix);
#else
    wlr_matrix_transpose(glMatrix, glMatrix);
    glUniformMatrix3fv(shader->proj, 1, GL_FALSE, glMatrix);
#endif
    glUniform1i(shader->tex, 0);

    glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(shader->posAttrib);
    glEnableVertexAttribArray(shader->texAttrib);

    for (auto& RECT : m_RenderData.damage.getRects()) {
        scissor(&RECT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    scissor((wlr_box*)nullptr);

    glDisableVertexAttribArray(shader->posAttrib);
    glDisableVertexAttribArray(shader->texAttrib);

    glBindTexture(tex.m_iTarget, 0);
}

// This probably isn't the fastest
// but it works... well, I guess?
//
// Dual (or more) kawase blur
CFramebuffer* CHyprOpenGLImpl::blurMainFramebufferWithDamage(float a, CRegion* originalDamage) {

    TRACY_GPU_ZONE("RenderBlurMainFramebufferWithDamage");

    const auto BLENDBEFORE = m_bBlend;
    blend(false);
    glDisable(GL_STENCIL_TEST);

    // get transforms for the full monitor
    const auto TRANSFORM = wlr_output_transform_invert(m_RenderData.pMonitor->transform);
    float      matrix[9];
    wlr_box    MONITORBOX = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    wlr_matrix_project_box(matrix, &MONITORBOX, TRANSFORM, 0, m_RenderData.pMonitor->output->transform_matrix);

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    // get the config settings
    static auto* const PBLURSIZE   = &g_pConfigManager->getConfigValuePtr("decoration:blur:size")->intValue;
    static auto* const PBLURPASSES = &g_pConfigManager->getConfigValuePtr("decoration:blur:passes")->intValue;

    // prep damage
    CRegion damage{*originalDamage};
    wlr_region_transform(damage.pixman(), damage.pixman(), wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
                         m_RenderData.pMonitor->vecTransformedSize.y);
    wlr_region_expand(damage.pixman(), damage.pixman(), *PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES));

    // helper
    const auto    PMIRRORFB     = &m_RenderData.pCurrentMonData->mirrorFB;
    const auto    PMIRRORSWAPFB = &m_RenderData.pCurrentMonData->mirrorSwapFB;

    CFramebuffer* currentRenderToFB = PMIRRORFB;

    // begin with color adjustments
    // TODO: make this a part of the first pass maybe to save on a drawcall?
    {
        static auto* const PBLURCONTRAST   = &g_pConfigManager->getConfigValuePtr("decoration:blur:contrast")->floatValue;
        static auto* const PBLURBRIGHTNESS = &g_pConfigManager->getConfigValuePtr("decoration:blur:brightness")->floatValue;

        PMIRRORSWAPFB->bind();

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(m_RenderData.pCurrentMonData->primaryFB.m_cTex.m_iTarget, m_RenderData.pCurrentMonData->primaryFB.m_cTex.m_iTexID);

        glTexParameteri(m_RenderData.pCurrentMonData->primaryFB.m_cTex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glUseProgram(m_RenderData.pCurrentMonData->m_shBLURFINISH.program);

#ifndef GLES2
        glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBLURFINISH.proj, 1, GL_TRUE, glMatrix);
#else
        wlr_matrix_transpose(glMatrix, glMatrix);
        glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBLURFINISH.proj, 1, GL_FALSE, glMatrix);
#endif
        glUniform1f(m_RenderData.pCurrentMonData->m_shBLURFINISH.contrast, *PBLURCONTRAST);
        glUniform1f(m_RenderData.pCurrentMonData->m_shBLURFINISH.brightness, *PBLURBRIGHTNESS);

        glUniform1i(m_RenderData.pCurrentMonData->m_shBLURFINISH.tex, 0);

        glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
        glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

        glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib);
        glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib);

        if (!damage.empty()) {
            for (auto& RECT : damage.getRects()) {
                scissor(&RECT, false /* this region is already transformed */);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }

        glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib);
        glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib);

        currentRenderToFB = PMIRRORSWAPFB;
    }

    // declare the draw func
    auto drawPass = [&](CShader* pShader, CRegion* pDamage) {
        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(currentRenderToFB->m_cTex.m_iTarget, currentRenderToFB->m_cTex.m_iTexID);

        glTexParameteri(currentRenderToFB->m_cTex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glUseProgram(pShader->program);

        // prep two shaders
#ifndef GLES2
        glUniformMatrix3fv(pShader->proj, 1, GL_TRUE, glMatrix);
#else
        wlr_matrix_transpose(glMatrix, glMatrix);
        glUniformMatrix3fv(pShader->proj, 1, GL_FALSE, glMatrix);
#endif
        glUniform1f(pShader->radius, *PBLURSIZE * a); // this makes the blursize change with a
        if (pShader == &m_RenderData.pCurrentMonData->m_shBLUR1)
            glUniform2f(m_RenderData.pCurrentMonData->m_shBLUR1.halfpixel, 0.5f / (m_RenderData.pMonitor->vecPixelSize.x / 2.f),
                        0.5f / (m_RenderData.pMonitor->vecPixelSize.y / 2.f));
        else
            glUniform2f(m_RenderData.pCurrentMonData->m_shBLUR2.halfpixel, 0.5f / (m_RenderData.pMonitor->vecPixelSize.x * 2.f),
                        0.5f / (m_RenderData.pMonitor->vecPixelSize.y * 2.f));
        glUniform1i(pShader->tex, 0);

        glVertexAttribPointer(pShader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
        glVertexAttribPointer(pShader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

        glEnableVertexAttribArray(pShader->posAttrib);
        glEnableVertexAttribArray(pShader->texAttrib);

        if (!pDamage->empty()) {
            for (auto& RECT : pDamage->getRects()) {
                scissor(&RECT, false /* this region is already transformed */);
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
    // first draw is swap -> mirr
    PMIRRORFB->bind();
    glBindTexture(PMIRRORSWAPFB->m_cTex.m_iTarget, PMIRRORSWAPFB->m_cTex.m_iTexID);

    // damage region will be scaled, make a temp
    CRegion tempDamage{damage};

    // and draw
    for (int i = 1; i <= *PBLURPASSES; ++i) {
        wlr_region_scale(tempDamage.pixman(), damage.pixman(), 1.f / (1 << i));
        drawPass(&m_RenderData.pCurrentMonData->m_shBLUR1, &tempDamage); // down
    }

    for (int i = *PBLURPASSES - 1; i >= 0; --i) {
        wlr_region_scale(tempDamage.pixman(), damage.pixman(), 1.f / (1 << i)); // when upsampling we make the region twice as big
        drawPass(&m_RenderData.pCurrentMonData->m_shBLUR2, &tempDamage);        // up
    }

    // finalize with noise
    {
        static auto* const PBLURNOISE = &g_pConfigManager->getConfigValuePtr("decoration:blur:noise")->floatValue;

        if (currentRenderToFB == PMIRRORFB)
            PMIRRORSWAPFB->bind();
        else
            PMIRRORFB->bind();

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(currentRenderToFB->m_cTex.m_iTarget, currentRenderToFB->m_cTex.m_iTexID);

        glTexParameteri(currentRenderToFB->m_cTex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glUseProgram(m_RenderData.pCurrentMonData->m_shBLURFINISH.program);

#ifndef GLES2
        glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBLURFINISH.proj, 1, GL_TRUE, glMatrix);
#else
        wlr_matrix_transpose(glMatrix, glMatrix);
        glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBLURFINISH.proj, 1, GL_FALSE, glMatrix);
#endif
        glUniform1f(m_RenderData.pCurrentMonData->m_shBLURFINISH.noise, *PBLURNOISE);

        glUniform1i(m_RenderData.pCurrentMonData->m_shBLURFINISH.tex, 0);

        glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
        glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

        glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib);
        glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib);

        if (!damage.empty()) {
            for (auto& RECT : damage.getRects()) {
                scissor(&RECT, false /* this region is already transformed */);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }

        glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib);
        glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib);

        if (currentRenderToFB != PMIRRORFB)
            currentRenderToFB = PMIRRORFB;
        else
            currentRenderToFB = PMIRRORSWAPFB;
    }

    // finish
    glBindTexture(PMIRRORFB->m_cTex.m_iTarget, 0);

    blend(BLENDBEFORE);

    return currentRenderToFB;
}

void CHyprOpenGLImpl::markBlurDirtyForMonitor(CMonitor* pMonitor) {
    const auto PWORKSPACE  = g_pCompositor->getWorkspaceByID(pMonitor->activeWorkspace);
    const auto PFULLWINDOW = g_pCompositor->getFullscreenWindowOnWorkspace(pMonitor->activeWorkspace);

    if (PWORKSPACE->m_bHasFullscreenWindow && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL && PFULLWINDOW && !PFULLWINDOW->m_vRealSize.isBeingAnimated() &&
        PFULLWINDOW->opaque())
        return;

    m_mMonitorRenderResources[pMonitor].blurFBDirty = true;
}

void CHyprOpenGLImpl::preRender(CMonitor* pMonitor) {
    static auto* const PBLURNEWOPTIMIZE = &g_pConfigManager->getConfigValuePtr("decoration:blur:new_optimizations")->intValue;
    static auto* const PBLURXRAY        = &g_pConfigManager->getConfigValuePtr("decoration:blur:xray")->intValue;
    static auto* const PBLUR            = &g_pConfigManager->getConfigValuePtr("decoration:blur:enabled")->intValue;

    if (!*PBLURNEWOPTIMIZE || !m_mMonitorRenderResources[pMonitor].blurFBDirty || !*PBLUR)
        return;

    // check if we need to update the blur fb
    // if there are no windows that would benefit from it,
    // we will ignore that the blur FB is dirty.

    auto windowShouldBeBlurred = [&](CWindow* pWindow) -> bool {
        if (!pWindow)
            return false;

        if (pWindow->m_sAdditionalConfigData.forceNoBlur)
            return false;

        const auto  PSURFACE = pWindow->m_pWLSurface.wlr();

        const auto  PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
        const float A          = pWindow->m_fAlpha.fl() * pWindow->m_fActiveInactiveAlpha.fl() * PWORKSPACE->m_fAlpha.fl();

        if (A >= 1.f) {
            if (PSURFACE->opaque)
                return false;

            CRegion        inverseOpaque;

            pixman_box32_t surfbox = {0, 0, PSURFACE->current.width, PSURFACE->current.height};
            CRegion        opaqueRegion{&PSURFACE->current.opaque};
            inverseOpaque.set(opaqueRegion).invert(&surfbox).intersect(0, 0, PSURFACE->current.width, PSURFACE->current.height);

            if (inverseOpaque.empty())
                return false;
        }

        return true;
    };

    bool hasWindows = false;
    for (auto& w : g_pCompositor->m_vWindows) {
        if (w->m_iWorkspaceID == pMonitor->activeWorkspace && !w->isHidden() && w->m_bIsMapped && (!w->m_bIsFloating || *PBLURXRAY)) {

            // check if window is valid
            if (!windowShouldBeBlurred(w.get()))
                continue;

            hasWindows = true;
            break;
        }
    }

    for (auto& m : g_pCompositor->m_vMonitors) {
        for (auto& lsl : m->m_aLayerSurfaceLayers) {
            for (auto& ls : lsl) {
                if (!ls->layerSurface || ls->xray != 1)
                    continue;

                if (ls->layerSurface->surface->opaque && ls->alpha.fl() >= 1.f)
                    continue;

                hasWindows = true;
                break;
            }
        }
    }

    if (!hasWindows)
        return;

    g_pHyprRenderer->damageMonitor(pMonitor);
    m_mMonitorRenderResources[pMonitor].blurFBShouldRender = true;
}

void CHyprOpenGLImpl::preBlurForCurrentMonitor() {

    TRACY_GPU_ZONE("RenderPreBlurForCurrentMonitor");

    const auto SAVEDRENDERMODIF = m_RenderData.renderModif;
    m_RenderData.renderModif    = {}; // fix shit

    // make the fake dmg
    CRegion    fakeDamage{0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    wlr_box    wholeMonitor = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    const auto POUTFB       = blurMainFramebufferWithDamage(1, &fakeDamage);

    // render onto blurFB
    m_RenderData.pCurrentMonData->blurFB.alloc(m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y);
    m_RenderData.pCurrentMonData->blurFB.bind();

    clear(CColor(0, 0, 0, 0));

    m_bEndFrame = true; // fix transformed
    renderTextureInternalWithDamage(POUTFB->m_cTex, &wholeMonitor, 1, &fakeDamage, 0, false, true, false);
    m_bEndFrame = false;

    m_RenderData.pCurrentMonData->primaryFB.bind();

    m_RenderData.pCurrentMonData->blurFBDirty = false;

    m_RenderData.renderModif = SAVEDRENDERMODIF;

    m_mMonitorRenderResources[m_RenderData.pMonitor].blurFBShouldRender = false;
}

void CHyprOpenGLImpl::preWindowPass() {
    if (!preBlurQueued())
        return;

    // blur the main FB, it will be rendered onto the mirror
    preBlurForCurrentMonitor();
}

bool CHyprOpenGLImpl::preBlurQueued() {
    static auto* const PBLURNEWOPTIMIZE = &g_pConfigManager->getConfigValuePtr("decoration:blur:new_optimizations")->intValue;
    static auto* const PBLUR            = &g_pConfigManager->getConfigValuePtr("decoration:blur:enabled")->intValue;

    return !(!m_RenderData.pCurrentMonData->blurFBDirty || !*PBLURNEWOPTIMIZE || !*PBLUR || !m_RenderData.pCurrentMonData->blurFBShouldRender);
}

bool CHyprOpenGLImpl::shouldUseNewBlurOptimizations(SLayerSurface* pLayer, CWindow* pWindow) {
    static auto* const PBLURNEWOPTIMIZE = &g_pConfigManager->getConfigValuePtr("decoration:blur:new_optimizations")->intValue;
    static auto* const PBLURXRAY        = &g_pConfigManager->getConfigValuePtr("decoration:blur:xray")->intValue;

    if (!m_RenderData.pCurrentMonData->blurFB.m_cTex.m_iTexID)
        return false;

    if (pWindow && pWindow->m_sAdditionalConfigData.xray.toUnderlying() == 0)
        return false;

    if (pLayer && pLayer->xray == 0)
        return false;

    if ((*PBLURNEWOPTIMIZE && pWindow && !pWindow->m_bIsFloating && !g_pCompositor->isWorkspaceSpecial(pWindow->m_iWorkspaceID)) || *PBLURXRAY)
        return true;

    if ((pLayer && pLayer->xray == 1) || (pWindow && pWindow->m_sAdditionalConfigData.xray.toUnderlying() == 1))
        return true;

    return false;
}

void CHyprOpenGLImpl::renderTextureWithBlur(const CTexture& tex, wlr_box* pBox, float a, wlr_surface* pSurface, int round, bool blockBlurOptimization, float blurA) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture with blur without begin()!");

    static auto* const PBLURENABLED     = &g_pConfigManager->getConfigValuePtr("decoration:blur:enabled")->intValue;
    static auto* const PNOBLUROVERSIZED = &g_pConfigManager->getConfigValuePtr("decoration:no_blur_on_oversized")->intValue;

    TRACY_GPU_ZONE("RenderTextureWithBlur");

    // make a damage region for this window
    CRegion texDamage{m_RenderData.damage};
    texDamage.intersect(pBox->x, pBox->y, pBox->width, pBox->height);

    if (texDamage.empty())
        return;

    if (*PBLURENABLED == 0 || (*PNOBLUROVERSIZED && m_RenderData.primarySurfaceUVTopLeft != Vector2D(-1, -1)) ||
        (m_pCurrentWindow && (m_pCurrentWindow->m_sAdditionalConfigData.forceNoBlur || m_pCurrentWindow->m_sAdditionalConfigData.forceRGBX))) {
        renderTexture(tex, pBox, a, round, false, true);
        return;
    }

    // amazing hack: the surface has an opaque region!
    CRegion inverseOpaque;
    if (a >= 1.f) {
        pixman_box32_t surfbox = {0, 0, pSurface->current.width * pSurface->current.scale, pSurface->current.height * pSurface->current.scale};
        inverseOpaque          = &pSurface->current.opaque;
        inverseOpaque.invert(&surfbox).intersect(0, 0, pSurface->current.width * pSurface->current.scale, pSurface->current.height * pSurface->current.scale);

        if (inverseOpaque.empty()) {
            renderTexture(tex, pBox, a, round, false, true);
            return;
        }
    } else {
        inverseOpaque = {0, 0, pBox->width, pBox->height};
    }

    wlr_region_scale(inverseOpaque.pixman(), inverseOpaque.pixman(), m_RenderData.pMonitor->scale);

    //   vvv TODO: layered blur fbs?
    const bool    USENEWOPTIMIZE = shouldUseNewBlurOptimizations(m_pCurrentLayer, m_pCurrentWindow) && !blockBlurOptimization;

    CFramebuffer* POUTFB = nullptr;
    if (!USENEWOPTIMIZE) {
        inverseOpaque.translate({pBox->x, pBox->y}).intersect(texDamage);

        POUTFB = blurMainFramebufferWithDamage(a, &inverseOpaque);
    } else {
        POUTFB = &m_RenderData.pCurrentMonData->blurFB;
    }

    // bind primary
    m_RenderData.pCurrentMonData->primaryFB.bind();

    // make a stencil for rounded corners to work with blur
    scissor((wlr_box*)nullptr); // allow the entire window and stencil to render
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_STENCIL_TEST);

    glStencilFunc(GL_ALWAYS, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    if (USENEWOPTIMIZE && !(m_RenderData.discardMode & DISCARD_ALPHA))
        renderRect(pBox, CColor(0, 0, 0, 0), round);
    else
        renderTexture(tex, pBox, a, round, true, true); // discard opaque
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glStencilFunc(GL_EQUAL, 1, -1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    // stencil done. Render everything.
    wlr_box MONITORBOX = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    // render our great blurred FB
    static auto* const PBLURIGNOREOPACITY = &g_pConfigManager->getConfigValuePtr("decoration:blur:ignore_opacity")->intValue;
    m_bEndFrame                           = true; // fix transformed
    const auto SAVEDRENDERMODIF           = m_RenderData.renderModif;
    m_RenderData.renderModif              = {}; // fix shit
    renderTextureInternalWithDamage(POUTFB->m_cTex, &MONITORBOX, *PBLURIGNOREOPACITY ? blurA : a * blurA, &texDamage, 0, false, false, false);
    m_bEndFrame              = false;
    m_RenderData.renderModif = SAVEDRENDERMODIF;

    // render the window, but clear stencil
    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    // draw window
    glDisable(GL_STENCIL_TEST);
    renderTextureInternalWithDamage(tex, pBox, a, &texDamage, round, false, false, true, true);

    glStencilMask(-1);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    scissor((wlr_box*)nullptr);
}

void pushVert2D(float x, float y, float* arr, int& counter, wlr_box* box) {
    // 0-1 space god damnit
    arr[counter * 2 + 0] = x / box->width;
    arr[counter * 2 + 1] = y / box->height;
    counter++;
}

void CHyprOpenGLImpl::renderBorder(wlr_box* box, const CGradientValueData& grad, int round, int borderSize, float a, int outerRound) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder");

    if (m_RenderData.damage.empty() || (m_pCurrentWindow && m_pCurrentWindow->m_sAdditionalConfigData.forceNoBorder))
        return;

    wlr_box newBox = *box;
    scaleBox(&newBox, m_RenderData.renderModif.scale);
    newBox.x += m_RenderData.renderModif.translate.x;
    newBox.y += m_RenderData.renderModif.translate.y;

    box = &newBox;

    if (borderSize < 1)
        return;

    int scaledBorderSize = borderSize * m_RenderData.pMonitor->scale * m_RenderData.renderModif.scale;

    // adjust box
    box->x -= scaledBorderSize;
    box->y -= scaledBorderSize;
    box->width += 2 * scaledBorderSize;
    box->height += 2 * scaledBorderSize;

    round += round == 0 ? 0 : scaledBorderSize;

    float matrix[9];
    wlr_matrix_project_box(matrix, box, wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), 0,
                           m_RenderData.pMonitor->output->transform_matrix); // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    const auto BLEND = m_bBlend;
    blend(true);

    glUseProgram(m_RenderData.pCurrentMonData->m_shBORDER1.program);

#ifndef GLES2
    glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBORDER1.proj, 1, GL_TRUE, glMatrix);
#else
    wlr_matrix_transpose(glMatrix, glMatrix);
    glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBORDER1.proj, 1, GL_FALSE, glMatrix);
#endif

    static_assert(sizeof(CColor) == 4 * sizeof(float)); // otherwise the line below this will fail

    glUniform4fv(m_RenderData.pCurrentMonData->m_shBORDER1.gradient, grad.m_vColors.size(), (float*)grad.m_vColors.data());
    glUniform1i(m_RenderData.pCurrentMonData->m_shBORDER1.gradientLength, grad.m_vColors.size());
    glUniform1f(m_RenderData.pCurrentMonData->m_shBORDER1.angle, (int)(grad.m_fAngle / (PI / 180.0)) % 360 * (PI / 180.0));
    glUniform1f(m_RenderData.pCurrentMonData->m_shBORDER1.alpha, a);

    wlr_box transformedBox;
    wlr_box_transform(&transformedBox, box, wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
                      m_RenderData.pMonitor->vecTransformedSize.y);

    const auto TOPLEFT  = Vector2D(transformedBox.x, transformedBox.y);
    const auto FULLSIZE = Vector2D(transformedBox.width, transformedBox.height);

    glUniform2f(m_RenderData.pCurrentMonData->m_shBORDER1.topLeft, (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(m_RenderData.pCurrentMonData->m_shBORDER1.fullSize, (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform2f(m_RenderData.pCurrentMonData->m_shBORDER1.fullSizeUntransformed, (float)box->width, (float)box->height);
    glUniform1f(m_RenderData.pCurrentMonData->m_shBORDER1.radius, round);
    glUniform1f(m_RenderData.pCurrentMonData->m_shBORDER1.radiusOuter, outerRound == -1 ? round : outerRound);
    glUniform1f(m_RenderData.pCurrentMonData->m_shBORDER1.thick, scaledBorderSize);

    glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBORDER1.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBORDER1.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBORDER1.posAttrib);
    glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBORDER1.texAttrib);

    if (m_RenderData.clipBox.width != 0 && m_RenderData.clipBox.height != 0) {
        CRegion damageClip{m_RenderData.clipBox.x, m_RenderData.clipBox.y, m_RenderData.clipBox.width, m_RenderData.clipBox.height};
        damageClip.intersect(m_RenderData.damage);

        if (!damageClip.empty()) {
            for (auto& RECT : damageClip.getRects()) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
    } else {
        for (auto& RECT : m_RenderData.damage.getRects()) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBORDER1.posAttrib);
    glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBORDER1.texAttrib);

    blend(BLEND);
}

void CHyprOpenGLImpl::makeRawWindowSnapshot(CWindow* pWindow, CFramebuffer* pFramebuffer) {
    // we trust the window is valid.
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    wlr_output_attach_render(PMONITOR->output, nullptr);

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    begin(PMONITOR, &fakeDamage, true);

    clear(CColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // this is a hack but it works :P
    // we need to disable blur or else we will get a black background, as the shader
    // will try to copy the bg to apply blur.
    // this isn't entirely correct, but like, oh well.
    // small todo: maybe make this correct? :P
    const auto BLURVAL = g_pConfigManager->getInt("decoration:blur:enabled");
    g_pConfigManager->setInt("decoration:blur:enabled", 0);

    // TODO: how can we make this the size of the window? setting it to window's size makes the entire screen render with the wrong res forever more. odd.
    glViewport(0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    pFramebuffer->m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;

    pFramebuffer->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    pFramebuffer->bind();

    clear(CColor(0, 0, 0, 0)); // JIC

    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, &now, false, RENDER_PASS_ALL, true);

    g_pConfigManager->setInt("decoration:blur:enabled", BLURVAL);

// restore original fb
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iCurrentOutputFb);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iCurrentOutputFb);
#endif
    end();

    wlr_output_rollback(PMONITOR->output);
}

void CHyprOpenGLImpl::makeWindowSnapshot(CWindow* pWindow) {
    // we trust the window is valid.
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    wlr_output_attach_render(PMONITOR->output, nullptr);

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    begin(PMONITOR, &fakeDamage, true);

    g_pHyprRenderer->m_bRenderingSnapshot = true;

    clear(CColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // this is a hack but it works :P
    // we need to disable blur or else we will get a black background, as the shader
    // will try to copy the bg to apply blur.
    // this isn't entirely correct, but like, oh well.
    // small todo: maybe make this correct? :P
    const auto BLURVAL = g_pConfigManager->getInt("decoration:blur:enabled");
    g_pConfigManager->setInt("decoration:blur:enabled", 0);

    glViewport(0, 0, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y);

    const auto PFRAMEBUFFER = &m_mWindowFramebuffers[pWindow];

    PFRAMEBUFFER->m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    PFRAMEBUFFER->bind();

    clear(CColor(0, 0, 0, 0)); // JIC

    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, &now, !pWindow->m_bX11DoesntWantBorders, RENDER_PASS_ALL);

    g_pConfigManager->setInt("decoration:blur:enabled", BLURVAL);

// restore original fb
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iCurrentOutputFb);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iCurrentOutputFb);
#endif
    end();

    g_pHyprRenderer->m_bRenderingSnapshot = false;

    wlr_output_rollback(PMONITOR->output);
}

void CHyprOpenGLImpl::makeLayerSnapshot(SLayerSurface* pLayer) {
    // we trust the window is valid.
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pLayer->monitorID);

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    wlr_output_attach_render(PMONITOR->output, nullptr);

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    begin(PMONITOR, &fakeDamage, true);

    g_pHyprRenderer->m_bRenderingSnapshot = true;

    const auto PFRAMEBUFFER = &m_mLayerFramebuffers[pLayer];

    glViewport(0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y);

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    PFRAMEBUFFER->bind();

    clear(CColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const auto BLURLSSTATUS = pLayer->forceBlur;
    pLayer->forceBlur       = false;

    // draw the layer
    g_pHyprRenderer->renderLayer(pLayer, PMONITOR, &now);

    pLayer->forceBlur = BLURLSSTATUS;

    // TODO: WARN:
    // revise if any stencil-requiring rendering is done to the layers.

// restore original fb
#ifndef GLES2
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_iCurrentOutputFb);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, m_iCurrentOutputFb);
#endif
    end();

    g_pHyprRenderer->m_bRenderingSnapshot = false;

    wlr_output_rollback(PMONITOR->output);
}

void CHyprOpenGLImpl::renderSnapshot(CWindow** pWindow) {
    RASSERT(m_RenderData.pMonitor, "Tried to render snapshot rect without begin()!");
    const auto         PWINDOW = *pWindow;

    static auto* const PDIMAROUND = &g_pConfigManager->getConfigValuePtr("decoration:dim_around")->floatValue;

    auto               it = m_mWindowFramebuffers.begin();
    for (; it != m_mWindowFramebuffers.end(); it++) {
        if (it->first == PWINDOW) {
            break;
        }
    }

    if (it == m_mWindowFramebuffers.end() || !it->second.m_cTex.m_iTexID)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    wlr_box    windowBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->scale * PWINDOW->m_vRealSize.vec().x / (PWINDOW->m_vOriginalClosedSize.x * PMONITOR->scale)),
                                (PMONITOR->scale * PWINDOW->m_vRealSize.vec().y / (PWINDOW->m_vOriginalClosedSize.y * PMONITOR->scale)));

    windowBox.width  = PMONITOR->vecTransformedSize.x * scaleXY.x;
    windowBox.height = PMONITOR->vecTransformedSize.y * scaleXY.y;
    windowBox.x      = ((PWINDOW->m_vRealPosition.vec().x - PMONITOR->vecPosition.x) * PMONITOR->scale) - ((PWINDOW->m_vOriginalClosedPos.x * PMONITOR->scale) * scaleXY.x);
    windowBox.y      = ((PWINDOW->m_vRealPosition.vec().y - PMONITOR->vecPosition.y) * PMONITOR->scale) - ((PWINDOW->m_vOriginalClosedPos.y * PMONITOR->scale) * scaleXY.y);

    CRegion fakeDamage{0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    if (*PDIMAROUND && (*pWindow)->m_sAdditionalConfigData.dimAround) {
        wlr_box monbox = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y};
        g_pHyprOpenGL->renderRect(&monbox, CColor(0, 0, 0, *PDIMAROUND * PWINDOW->m_fAlpha.fl()));
        g_pHyprRenderer->damageMonitor(PMONITOR);
    }

    m_bEndFrame = true;

    renderTextureInternalWithDamage(it->second.m_cTex, &windowBox, PWINDOW->m_fAlpha.fl(), &fakeDamage, 0);

    m_bEndFrame = false;
}

void CHyprOpenGLImpl::renderSnapshot(SLayerSurface** pLayer) {
    RASSERT(m_RenderData.pMonitor, "Tried to render snapshot rect without begin()!");
    const auto PLAYER = *pLayer;

    auto       it = m_mLayerFramebuffers.begin();
    for (; it != m_mLayerFramebuffers.end(); it++) {
        if (it->first == PLAYER) {
            break;
        }
    }

    if (it == m_mLayerFramebuffers.end() || !it->second.m_cTex.m_iTexID)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PLAYER->monitorID);

    wlr_box    monbox = {0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    CRegion    fakeDamage{0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    m_bEndFrame = true;

    renderTextureInternalWithDamage(it->second.m_cTex, &monbox, PLAYER->alpha.fl(), &fakeDamage, 0);

    m_bEndFrame = false;
}

void CHyprOpenGLImpl::renderRoundedShadow(wlr_box* box, int round, int range, float a) {
    RASSERT(m_RenderData.pMonitor, "Tried to render shadow without begin()!");
    RASSERT((box->width > 0 && box->height > 0), "Tried to render shadow with width/height < 0!");
    RASSERT(m_pCurrentWindow, "Tried to render shadow without a window!");

    if (m_RenderData.damage.empty())
        return;

    TRACY_GPU_ZONE("RenderShadow");

    wlr_box newBox = *box;
    scaleBox(&newBox, m_RenderData.renderModif.scale);
    newBox.x += m_RenderData.renderModif.translate.x;
    newBox.y += m_RenderData.renderModif.translate.y;

    box = &newBox;

    static auto* const PSHADOWPOWER = &g_pConfigManager->getConfigValuePtr("decoration:shadow_render_power")->intValue;

    const auto         SHADOWPOWER = std::clamp((int)*PSHADOWPOWER, 1, 4);

    const auto         col = m_pCurrentWindow->m_cRealShadowColor.col();

    float              matrix[9];
    wlr_matrix_project_box(matrix, box, wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), 0,
                           m_RenderData.pMonitor->output->transform_matrix); // TODO: write own, don't use WLR here

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    glEnable(GL_BLEND);

    glUseProgram(m_RenderData.pCurrentMonData->m_shSHADOW.program);

#ifndef GLES2
    glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shSHADOW.proj, 1, GL_TRUE, glMatrix);
#else
    wlr_matrix_transpose(glMatrix, glMatrix);
    glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shSHADOW.proj, 1, GL_FALSE, glMatrix);
#endif
    glUniform4f(m_RenderData.pCurrentMonData->m_shSHADOW.color, col.r, col.g, col.b, col.a * a);

    const auto TOPLEFT     = Vector2D(range + round, range + round);
    const auto BOTTOMRIGHT = Vector2D(box->width - (range + round), box->height - (range + round));
    const auto FULLSIZE    = Vector2D(box->width, box->height);

    // Rounded corners
    glUniform2f(m_RenderData.pCurrentMonData->m_shSHADOW.topLeft, (float)TOPLEFT.x, (float)TOPLEFT.y);
    glUniform2f(m_RenderData.pCurrentMonData->m_shSHADOW.bottomRight, (float)BOTTOMRIGHT.x, (float)BOTTOMRIGHT.y);
    glUniform2f(m_RenderData.pCurrentMonData->m_shSHADOW.fullSize, (float)FULLSIZE.x, (float)FULLSIZE.y);
    glUniform1f(m_RenderData.pCurrentMonData->m_shSHADOW.radius, range + round);
    glUniform1f(m_RenderData.pCurrentMonData->m_shSHADOW.range, range);
    glUniform1f(m_RenderData.pCurrentMonData->m_shSHADOW.shadowPower, SHADOWPOWER);

    glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shSHADOW.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shSHADOW.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shSHADOW.posAttrib);
    glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shSHADOW.texAttrib);

    if (m_RenderData.clipBox.width != 0 && m_RenderData.clipBox.height != 0) {
        CRegion damageClip{m_RenderData.clipBox.x, m_RenderData.clipBox.y, m_RenderData.clipBox.width, m_RenderData.clipBox.height};
        damageClip.intersect(m_RenderData.damage);

        if (!damageClip.empty()) {
            for (auto& RECT : damageClip.getRects()) {
                scissor(&RECT);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }
    } else {
        for (auto& RECT : m_RenderData.damage.getRects()) {
            scissor(&RECT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    }

    glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shSHADOW.posAttrib);
    glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shSHADOW.texAttrib);
}

void CHyprOpenGLImpl::saveBufferForMirror() {

    if (!m_RenderData.pCurrentMonData->monitorMirrorFB.isAllocated())
        m_RenderData.pCurrentMonData->monitorMirrorFB.alloc(m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y);

    m_RenderData.pCurrentMonData->monitorMirrorFB.bind();

    wlr_box monbox = {0, 0, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y};

    blend(false);

    renderTexture(m_RenderData.pCurrentMonData->primaryFB.m_cTex, &monbox, 1.f, 0, false, false);

    blend(true);

    m_RenderData.pCurrentMonData->primaryFB.bind();
}

void CHyprOpenGLImpl::renderMirrored() {
    wlr_box    monbox = {0, 0, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y};

    const auto PFB = &m_mMonitorRenderResources[m_RenderData.pMonitor->pMirrorOf].monitorMirrorFB;

    if (!PFB->isAllocated() || PFB->m_cTex.m_iTexID <= 0)
        return;

    renderTexture(PFB->m_cTex, &monbox, 1.f, 0, false, false);
}

void CHyprOpenGLImpl::renderSplash(cairo_t* const CAIRO, cairo_surface_t* const CAIROSURFACE, double offsetY) {
    cairo_select_font_face(CAIRO, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    const auto FONTSIZE = (int)(m_RenderData.pMonitor->vecPixelSize.y / 76);
    cairo_set_font_size(CAIRO, FONTSIZE);

    cairo_set_source_rgba(CAIRO, 1.0, 1.0, 1.0, 0.32);

    cairo_text_extents_t textExtents;
    cairo_text_extents(CAIRO, g_pCompositor->m_szCurrentSplash.c_str(), &textExtents);

    cairo_move_to(CAIRO, (m_RenderData.pMonitor->vecPixelSize.x - textExtents.width) / 2.0, m_RenderData.pMonitor->vecPixelSize.y - textExtents.height + offsetY);

    cairo_show_text(CAIRO, g_pCompositor->m_szCurrentSplash.c_str());

    cairo_surface_flush(CAIROSURFACE);
}

void CHyprOpenGLImpl::createBGTextureForMonitor(CMonitor* pMonitor) {
    RASSERT(m_RenderData.pMonitor, "Tried to createBGTex without begin()!");

    static auto* const PNOSPLASH       = &g_pConfigManager->getConfigValuePtr("misc:disable_splash_rendering")->intValue;
    static auto* const PFORCEHYPRCHAN  = &g_pConfigManager->getConfigValuePtr("misc:force_hypr_chan")->intValue;
    static auto* const PFORCEWALLPAPER = &g_pConfigManager->getConfigValuePtr("misc:force_default_wallpaper")->intValue;

    const auto         FORCEWALLPAPER = std::clamp(*PFORCEWALLPAPER, static_cast<int64_t>(-1L), static_cast<int64_t>(2L));

    // release the last tex if exists
    const auto PTEX = &m_mMonitorBGTextures[pMonitor];
    PTEX->destroyTexture();

    PTEX->allocate();
    Debug::log(LOG, "Allocated texture for BGTex");

    // TODO: use relative paths to the installation
    // or configure the paths at build time
    std::string texPath    = "/usr/share/hyprland/wall_";
    std::string prefixes[] = {"", "anime_", "anime2_"};

    // get the adequate tex
    if (FORCEWALLPAPER == -1) {
        std::random_device              dev;
        std::mt19937                    engine(dev());
        std::uniform_int_distribution<> distribution(0, 2);
        std::uniform_int_distribution<> distribution_anime(1, 2);

        if (PFORCEHYPRCHAN)
            texPath += prefixes[distribution_anime(engine)];
        else
            texPath += prefixes[distribution(engine)];
    } else
        texPath += prefixes[FORCEWALLPAPER];

    Vector2D textureSize;
    if (pMonitor->vecTransformedSize.x > 3850) {
        textureSize = Vector2D(7680, 4320);
        texPath += "8K.png";
    } else if (pMonitor->vecTransformedSize.x > 1930) {
        textureSize = Vector2D(3840, 2160);
        texPath += "4K.png";
    } else {
        textureSize = Vector2D(1920, 1080);
        texPath += "2K.png";
    }

    // check if wallpapers exist
    if (!std::filesystem::exists(texPath)) {
        // try local
        texPath = texPath.substr(0, 5) + "local/" + texPath.substr(5);

        if (!std::filesystem::exists(texPath))
            return; // the texture will be empty, oh well. We'll clear with a solid color anyways.
    }

    PTEX->m_vSize = textureSize;

    // calc the target box
    const double MONRATIO = m_RenderData.pMonitor->vecTransformedSize.x / m_RenderData.pMonitor->vecTransformedSize.y;
    const double WPRATIO  = 1.77;

    Vector2D     origin;
    double       scale;

    if (MONRATIO > WPRATIO) {
        scale = m_RenderData.pMonitor->vecTransformedSize.x / PTEX->m_vSize.x;

        origin.y = (m_RenderData.pMonitor->vecTransformedSize.y - PTEX->m_vSize.y * scale) / 2.0;
    } else {
        scale = m_RenderData.pMonitor->vecTransformedSize.y / PTEX->m_vSize.y;

        origin.x = (m_RenderData.pMonitor->vecTransformedSize.x - PTEX->m_vSize.x * scale) / 2.0;
    }

    wlr_box box = {origin.x, origin.y, PTEX->m_vSize.x * scale, PTEX->m_vSize.y * scale};

    m_mMonitorRenderResources[pMonitor].backgroundTexBox = box;

    // create a new one with cairo
    const auto CAIROSURFACE = cairo_image_surface_create_from_png(texPath.c_str());
    const auto CAIRO        = cairo_create(CAIROSURFACE);

    // scale it to fit the current monitor
    cairo_scale(CAIRO, textureSize.x / pMonitor->vecTransformedSize.x, textureSize.y / pMonitor->vecTransformedSize.y);

    // render splash on wallpaper
    if (!*PNOSPLASH)
        renderSplash(CAIRO, CAIROSURFACE, origin.y * WPRATIO / MONRATIO);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    glBindTexture(GL_TEXTURE_2D, PTEX->m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureSize.x, textureSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    cairo_surface_destroy(CAIROSURFACE);
    cairo_destroy(CAIRO);

    Debug::log(LOG, "Background created for monitor {}", pMonitor->szName);
}

void CHyprOpenGLImpl::clearWithTex() {
    RASSERT(m_RenderData.pMonitor, "Tried to render BGtex without begin()!");

    TRACY_GPU_ZONE("RenderClearWithTex");

    auto TEXIT = m_mMonitorBGTextures.find(m_RenderData.pMonitor);

    if (TEXIT == m_mMonitorBGTextures.end()) {
        createBGTextureForMonitor(m_RenderData.pMonitor);
        TEXIT = m_mMonitorBGTextures.find(m_RenderData.pMonitor);
    }

    if (TEXIT != m_mMonitorBGTextures.end())
        renderTexturePrimitive(TEXIT->second, &m_mMonitorRenderResources[m_RenderData.pMonitor].backgroundTexBox);
}

void CHyprOpenGLImpl::destroyMonitorResources(CMonitor* pMonitor) {
    wlr_output_attach_render(pMonitor->output, nullptr);

    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].mirrorFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].primaryFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].mirrorSwapFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].monitorMirrorFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].blurFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].offMainFB.release();
    g_pHyprOpenGL->m_mMonitorRenderResources[pMonitor].stencilTex.destroyTexture();
    g_pHyprOpenGL->m_mMonitorBGTextures[pMonitor].destroyTexture();
    g_pHyprOpenGL->m_mMonitorRenderResources.erase(pMonitor);
    g_pHyprOpenGL->m_mMonitorBGTextures.erase(pMonitor);

    Debug::log(LOG, "Monitor {} -> destroyed all render data", pMonitor->szName);

    wlr_output_rollback(pMonitor->output);
}

void CHyprOpenGLImpl::saveMatrix() {
    memcpy(m_RenderData.savedProjection, m_RenderData.projection, 9 * sizeof(float));
}

void CHyprOpenGLImpl::setMatrixScaleTranslate(const Vector2D& translate, const float& scale) {
    wlr_matrix_scale(m_RenderData.projection, scale, scale);
    wlr_matrix_translate(m_RenderData.projection, translate.x, translate.y);
}

void CHyprOpenGLImpl::restoreMatrix() {
    memcpy(m_RenderData.projection, m_RenderData.savedProjection, 9 * sizeof(float));
}

void CHyprOpenGLImpl::bindOffMain() {
    m_RenderData.pCurrentMonData->offMainFB.bind();
    clear(CColor(0, 0, 0, 0));
    m_RenderData.currentFB = &m_RenderData.pCurrentMonData->offMainFB;
}

void CHyprOpenGLImpl::renderOffToMain(CFramebuffer* off) {
    wlr_box monbox = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    renderTexturePrimitive(off->m_cTex, &monbox);
}

void CHyprOpenGLImpl::bindBackOnMain() {
    m_RenderData.pCurrentMonData->primaryFB.bind();
    m_RenderData.currentFB = &m_RenderData.pCurrentMonData->primaryFB;
}
