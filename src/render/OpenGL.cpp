#include "Shaders.hpp"
#include "OpenGL.hpp"
#include "../Compositor.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "Shaders.hpp"
#include <random>
#include "../config/ConfigValue.hpp"

inline void loadGLProc(void* pProc, const char* name) {
    void* proc = (void*)eglGetProcAddress(name);
    if (proc == NULL) {
        Debug::log(CRIT, "[Tracy GPU Profiling] eglGetProcAddress({}) failed", name);
        abort();
    }
    *(void**)pProc = proc;
}

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

    loadGLProc(&m_sProc.glEGLImageTargetRenderbufferStorageOES, "glEGLImageTargetRenderbufferStorageOES");
    loadGLProc(&m_sProc.eglDestroyImageKHR, "eglDestroyImageKHR");

    m_sExts.EXT_read_format_bgra = m_szExtensions.contains("GL_EXT_read_format_bgra");

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

bool CHyprOpenGLImpl::passRequiresIntrospection(CMonitor* pMonitor) {
    // passes requiring introspection are the ones that need to render blur.

    static auto PBLUR        = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PXRAY        = CConfigValue<Hyprlang::INT>("decoration:blur:xray");
    static auto POPTIM       = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLURSPECIAL = CConfigValue<Hyprlang::INT>("decoration:blur:special");

    if (m_RenderData.mouseZoomFactor != 1.0 || g_pHyprRenderer->m_bCrashingInProgress)
        return true;

    if (!pMonitor->mirrors.empty())
        return true;

    if (*PBLUR == 0)
        return false;

    if (m_RenderData.pCurrentMonData->blurFBShouldRender)
        return true;

    if (pMonitor->solitaryClient)
        return false;

    for (auto& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]) {
        const auto XRAYMODE = ls->xray == -1 ? *PXRAY : ls->xray;
        if (ls->forceBlur && !XRAYMODE)
            return true;
    }

    for (auto& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]) {
        const auto XRAYMODE = ls->xray == -1 ? *PXRAY : ls->xray;
        if (ls->forceBlur && !XRAYMODE)
            return true;
    }

    // these two block optimization
    for (auto& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]) {
        if (ls->forceBlur)
            return true;
    }

    for (auto& ls : pMonitor->m_aLayerSurfaceLayers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]) {
        if (ls->forceBlur)
            return true;
    }

    if (*PBLURSPECIAL) {
        for (auto& ws : g_pCompositor->m_vWorkspaces) {
            if (!ws->m_bIsSpecialWorkspace || ws->m_iMonitorID != pMonitor->ID)
                continue;

            if (ws->m_fAlpha.value() == 0)
                continue;

            return true;
        }
    }

    if (*PXRAY)
        return false;

    for (auto& w : g_pCompositor->m_vWindows) {
        if (!w->m_bIsMapped || w->isHidden() || (!w->m_bIsFloating && *POPTIM && !g_pCompositor->isWorkspaceSpecial(w->m_iWorkspaceID)))
            continue;

        if (!g_pHyprRenderer->shouldRenderWindow(w.get()))
            continue;

        if (w->m_sAdditionalConfigData.forceNoBlur.toUnderlying() == true || w->m_sAdditionalConfigData.xray.toUnderlying() == true)
            continue;

        if (w->opaque())
            continue;

        return true;
    }

    return false;
}

void CHyprOpenGLImpl::begin(CMonitor* pMonitor, const CRegion& damage_, CFramebuffer* fb, std::optional<CRegion> finalDamage) {
    m_RenderData.pMonitor = pMonitor;

    static auto PFORCEINTROSPECTION = CConfigValue<Hyprlang::INT>("opengl:force_introspection");

#ifndef GLES2

    const GLenum RESETSTATUS = glGetGraphicsResetStatus();
    if (RESETSTATUS != GL_NO_ERROR) {
        std::string errStr = "";
        switch (RESETSTATUS) {
            case GL_GUILTY_CONTEXT_RESET: errStr = "GL_GUILTY_CONTEXT_RESET"; break;
            case GL_INNOCENT_CONTEXT_RESET: errStr = "GL_INNOCENT_CONTEXT_RESET"; break;
            case GL_UNKNOWN_CONTEXT_RESET: errStr = "GL_UNKNOWN_CONTEXT_RESET"; break;
            default: errStr = "UNKNOWN??"; break;
        }
        RASSERT(false, "Aborting, glGetGraphicsResetStatus returned {}. Cannot continue until proper GPU reset handling is implemented.", errStr);
        return;
    }

#endif

    TRACY_GPU_ZONE("RenderBegin");

    glViewport(0, 0, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);

    matrixProjection(m_RenderData.projection, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, WL_OUTPUT_TRANSFORM_NORMAL);

    if (m_mMonitorRenderResources.contains(pMonitor) && m_mMonitorRenderResources.at(pMonitor).offloadFB.m_vSize != pMonitor->vecPixelSize)
        destroyMonitorResources(pMonitor);

    m_RenderData.pCurrentMonData = &m_mMonitorRenderResources[pMonitor];

    if (!m_RenderData.pCurrentMonData->m_bShadersInitialized)
        initShaders();

    // ensure a framebuffer for the monitor exists
    if (m_RenderData.pCurrentMonData->offloadFB.m_vSize != pMonitor->vecPixelSize) {
        m_RenderData.pCurrentMonData->stencilTex.allocate();

        m_RenderData.pCurrentMonData->offloadFB.m_pStencilTex    = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->mirrorFB.m_pStencilTex     = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->mirrorSwapFB.m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;
        m_RenderData.pCurrentMonData->offMainFB.m_pStencilTex    = &m_RenderData.pCurrentMonData->stencilTex;

        m_RenderData.pCurrentMonData->offloadFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, pMonitor->drmFormat);
        m_RenderData.pCurrentMonData->mirrorFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, pMonitor->drmFormat);
        m_RenderData.pCurrentMonData->mirrorSwapFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, pMonitor->drmFormat);
        m_RenderData.pCurrentMonData->offMainFB.alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, pMonitor->drmFormat);
    }

    if (m_RenderData.pCurrentMonData->monitorMirrorFB.isAllocated() && m_RenderData.pMonitor->mirrors.empty())
        m_RenderData.pCurrentMonData->monitorMirrorFB.release();

    m_RenderData.damage.set(damage_);
    m_RenderData.finalDamage.set(finalDamage.value_or(damage_));

    m_bFakeFrame = fb;

    if (m_bReloadScreenShader) {
        m_bReloadScreenShader = false;
        static auto PSHADER   = CConfigValue<std::string>("decoration:screen_shader");
        applyScreenShader(*PSHADER);
    }

    const auto PRBO                    = g_pHyprRenderer->getCurrentRBO();
    const bool FBPROPERSIZE            = !fb || fb->m_vSize == pMonitor->vecPixelSize;
    const bool USERFORCEDINTROSPECTION = *PFORCEINTROSPECTION == 1 ? true : (*PFORCEINTROSPECTION == 2 ? g_pHyprRenderer->isNvidia() : false); // 0 - no, 1 - yes, 2 - nvidia only

    if (USERFORCEDINTROSPECTION || m_RenderData.forceIntrospection || !FBPROPERSIZE || m_sFinalScreenShader.program > 0 ||
        (PRBO && pMonitor->vecPixelSize != PRBO->getFB()->m_vSize) || passRequiresIntrospection(pMonitor)) {
        // we have to offload
        // bind the offload Hypr Framebuffer
        m_RenderData.pCurrentMonData->offloadFB.bind();
        m_RenderData.currentFB  = &m_RenderData.pCurrentMonData->offloadFB;
        m_bOffloadedFramebuffer = true;
    } else {
        // we can render to the rbo / fbo (fake) directly
        const auto PFBO        = fb ? fb : PRBO->getFB();
        m_RenderData.currentFB = PFBO;
        if (PFBO->m_pStencilTex != &m_RenderData.pCurrentMonData->stencilTex) {
            PFBO->m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;
            PFBO->addStencil();
        }
        PFBO->bind();
        m_bOffloadedFramebuffer = false;
    }

    m_RenderData.mainFB = m_RenderData.currentFB;
    m_RenderData.outFB  = fb ? fb : PRBO->getFB();
}

void CHyprOpenGLImpl::end() {
    static auto PZOOMRIGID = CConfigValue<Hyprlang::INT>("misc:cursor_zoom_rigid");

    TRACY_GPU_ZONE("RenderEnd");

    if (!m_RenderData.pMonitor->mirrors.empty() && !m_bFakeFrame)
        saveBufferForMirror(); // save with original damage region

    // end the render, copy the data to the WLR framebuffer
    if (m_bOffloadedFramebuffer) {
        m_RenderData.damage = m_RenderData.finalDamage;

        m_RenderData.outFB->bind();

        CBox monbox = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};

        if (m_RenderData.mouseZoomFactor != 1.f) {
            const auto ZOOMCENTER = m_RenderData.mouseZoomUseMouse ?
                (g_pInputManager->getMouseCoordsInternal() - m_RenderData.pMonitor->vecPosition) * m_RenderData.pMonitor->scale :
                m_RenderData.pMonitor->vecTransformedSize / 2.f;

            monbox.translate(-ZOOMCENTER).scale(m_RenderData.mouseZoomFactor).translate(*PZOOMRIGID ? m_RenderData.pMonitor->vecTransformedSize / 2.0 : ZOOMCENTER);

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

        if (m_sFinalScreenShader.program < 1 && !g_pHyprRenderer->m_bCrashingInProgress)
            renderTexturePrimitive(m_RenderData.pCurrentMonData->offloadFB.m_cTex, &monbox);
        else
            renderTexture(m_RenderData.pCurrentMonData->offloadFB.m_cTex, &monbox, 1.f);

        blend(true);

        m_RenderData.useNearestNeighbor = false;
        m_bApplyFinalShader             = false;
        m_bEndFrame                     = false;
    }

    // reset our data
    m_RenderData.pMonitor           = nullptr;
    m_RenderData.mouseZoomFactor    = 1.f;
    m_RenderData.mouseZoomUseMouse  = true;
    m_RenderData.forceIntrospection = false;
    m_RenderData.currentFB          = nullptr;
    m_RenderData.mainFB             = nullptr;
    m_RenderData.outFB              = nullptr;

    // check for gl errors
    const GLenum ERR = glGetError();

#ifdef GLES2
    if (ERR == GL_CONTEXT_LOST_KHR) /* We don't have infra to recover from this */
#else
    if (ERR == GL_CONTEXT_LOST) /* We don't have infra to recover from this */
#endif
        RASSERT(false, "glGetError at Opengl::end() returned GL_CONTEXT_LOST. Cannot continue until proper GPU reset handling is implemented.");
}

void CHyprOpenGLImpl::setDamage(const CRegion& damage_, std::optional<CRegion> finalDamage) {
    m_RenderData.damage.set(damage_);
    m_RenderData.finalDamage.set(finalDamage.value_or(damage_));
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
    m_RenderData.pCurrentMonData->m_shRGBA.alphaMatte        = glGetUniformLocation(prog, "texMatte");
    m_RenderData.pCurrentMonData->m_shRGBA.alpha             = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shRGBA.texAttrib         = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shRGBA.matteTexAttrib    = glGetAttribLocation(prog, "texcoordMatte");
    m_RenderData.pCurrentMonData->m_shRGBA.posAttrib         = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shRGBA.discardOpaque     = glGetUniformLocation(prog, "discardOpaque");
    m_RenderData.pCurrentMonData->m_shRGBA.discardAlpha      = glGetUniformLocation(prog, "discardAlpha");
    m_RenderData.pCurrentMonData->m_shRGBA.discardAlphaValue = glGetUniformLocation(prog, "discardAlphaValue");
    m_RenderData.pCurrentMonData->m_shRGBA.topLeft           = glGetUniformLocation(prog, "topLeft");
    m_RenderData.pCurrentMonData->m_shRGBA.fullSize          = glGetUniformLocation(prog, "fullSize");
    m_RenderData.pCurrentMonData->m_shRGBA.radius            = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shRGBA.applyTint         = glGetUniformLocation(prog, "applyTint");
    m_RenderData.pCurrentMonData->m_shRGBA.tint              = glGetUniformLocation(prog, "tint");
    m_RenderData.pCurrentMonData->m_shRGBA.useAlphaMatte     = glGetUniformLocation(prog, "useAlphaMatte");

    prog                                                     = createProgram(TEXVERTSRC, TEXFRAGSRCRGBAPASSTHRU);
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.program   = prog;
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.proj      = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.tex       = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shPASSTHRURGBA.posAttrib = glGetAttribLocation(prog, "pos");

    prog                                               = createProgram(TEXVERTSRC, TEXFRAGSRCRGBAMATTE);
    m_RenderData.pCurrentMonData->m_shMATTE.program    = prog;
    m_RenderData.pCurrentMonData->m_shMATTE.proj       = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shMATTE.tex        = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shMATTE.alphaMatte = glGetUniformLocation(prog, "texMatte");
    m_RenderData.pCurrentMonData->m_shMATTE.texAttrib  = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shMATTE.posAttrib  = glGetAttribLocation(prog, "pos");

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

    prog                                                      = createProgram(TEXVERTSRC, FRAGBLUR1);
    m_RenderData.pCurrentMonData->m_shBLUR1.program           = prog;
    m_RenderData.pCurrentMonData->m_shBLUR1.tex               = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shBLUR1.alpha             = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shBLUR1.proj              = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBLUR1.posAttrib         = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBLUR1.texAttrib         = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBLUR1.radius            = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shBLUR1.halfpixel         = glGetUniformLocation(prog, "halfpixel");
    m_RenderData.pCurrentMonData->m_shBLUR1.passes            = glGetUniformLocation(prog, "passes");
    m_RenderData.pCurrentMonData->m_shBLUR1.vibrancy          = glGetUniformLocation(prog, "vibrancy");
    m_RenderData.pCurrentMonData->m_shBLUR1.vibrancy_darkness = glGetUniformLocation(prog, "vibrancy_darkness");

    prog                                              = createProgram(TEXVERTSRC, FRAGBLUR2);
    m_RenderData.pCurrentMonData->m_shBLUR2.program   = prog;
    m_RenderData.pCurrentMonData->m_shBLUR2.tex       = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shBLUR2.alpha     = glGetUniformLocation(prog, "alpha");
    m_RenderData.pCurrentMonData->m_shBLUR2.proj      = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBLUR2.posAttrib = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBLUR2.texAttrib = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBLUR2.radius    = glGetUniformLocation(prog, "radius");
    m_RenderData.pCurrentMonData->m_shBLUR2.halfpixel = glGetUniformLocation(prog, "halfpixel");

    prog                                                     = createProgram(TEXVERTSRC, FRAGBLURPREPARE);
    m_RenderData.pCurrentMonData->m_shBLURPREPARE.program    = prog;
    m_RenderData.pCurrentMonData->m_shBLURPREPARE.tex        = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shBLURPREPARE.proj       = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBLURPREPARE.posAttrib  = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBLURPREPARE.texAttrib  = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBLURPREPARE.contrast   = glGetUniformLocation(prog, "contrast");
    m_RenderData.pCurrentMonData->m_shBLURPREPARE.brightness = glGetUniformLocation(prog, "brightness");

    prog                                                    = createProgram(TEXVERTSRC, FRAGBLURFINISH);
    m_RenderData.pCurrentMonData->m_shBLURFINISH.program    = prog;
    m_RenderData.pCurrentMonData->m_shBLURFINISH.tex        = glGetUniformLocation(prog, "tex");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.proj       = glGetUniformLocation(prog, "proj");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.posAttrib  = glGetAttribLocation(prog, "pos");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.texAttrib  = glGetAttribLocation(prog, "texcoord");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.brightness = glGetUniformLocation(prog, "brightness");
    m_RenderData.pCurrentMonData->m_shBLURFINISH.noise      = glGetUniformLocation(prog, "noise");

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

    static auto PDT = CConfigValue<Hyprlang::INT>("debug:damage_tracking");

    m_sFinalScreenShader.destroy();

    if (path == "" || path == STRVAL_EMPTY)
        return;

    std::ifstream infile(absolutePath(path, g_pConfigManager->getConfigDir()));

    if (!infile.good()) {
        g_pConfigManager->addParseError("Screen shader parser: Screen shader path not found");
        return;
    }

    std::string fragmentShader((std::istreambuf_iterator<char>(infile)), (std::istreambuf_iterator<char>()));

    m_sFinalScreenShader.program = createProgram(fragmentShader.starts_with("#version 320 es") ? TEXVERTSRC320 : TEXVERTSRC, fragmentShader, true);

    if (!m_sFinalScreenShader.program) {
        g_pConfigManager->addParseError("Screen shader parser: Screen shader parse failed");
        return;
    }

    m_sFinalScreenShader.proj      = glGetUniformLocation(m_sFinalScreenShader.program, "proj");
    m_sFinalScreenShader.tex       = glGetUniformLocation(m_sFinalScreenShader.program, "tex");
    m_sFinalScreenShader.time      = glGetUniformLocation(m_sFinalScreenShader.program, "time");
    m_sFinalScreenShader.wl_output = glGetUniformLocation(m_sFinalScreenShader.program, "wl_output");
    m_sFinalScreenShader.fullSize  = glGetUniformLocation(m_sFinalScreenShader.program, "screen_size");
    if (m_sFinalScreenShader.time != -1 && *PDT != 0 && !g_pHyprRenderer->m_bCrashingInProgress) {
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

    scissor((CBox*)nullptr);
}

void CHyprOpenGLImpl::blend(bool enabled) {
    if (enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // everything is premultiplied
    } else
        glDisable(GL_BLEND);

    m_bBlend = enabled;
}

void CHyprOpenGLImpl::scissor(const CBox* pBox, bool transform) {
    RASSERT(m_RenderData.pMonitor, "Tried to scissor without begin()!");

    if (!pBox) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }

    CBox newBox = *pBox;

    if (transform) {
        int w, h;
        wlr_output_transformed_resolution(m_RenderData.pMonitor->output, &w, &h);

        const auto TR = wlr_output_transform_invert(m_RenderData.pMonitor->transform);
        newBox.transform(TR, w, h);
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

    CBox newBox = {pBox->x1, pBox->y1, pBox->x2 - pBox->x1, pBox->y2 - pBox->y1};

    scissor(&newBox, transform);
}

void CHyprOpenGLImpl::scissor(const int x, const int y, const int w, const int h, bool transform) {
    CBox box = {x, y, w, h};
    scissor(&box, transform);
}

void CHyprOpenGLImpl::renderRect(CBox* box, const CColor& col, int round) {
    if (!m_RenderData.damage.empty())
        renderRectWithDamage(box, col, &m_RenderData.damage, round);
}

void CHyprOpenGLImpl::renderRectWithBlur(CBox* box, const CColor& col, int round, float blurA, bool xray) {
    if (m_RenderData.damage.empty())
        return;

    CRegion damage{m_RenderData.damage};
    damage.intersect(*box);

    CFramebuffer* POUTFB = xray ? &m_RenderData.pCurrentMonData->blurFB : blurMainFramebufferWithDamage(blurA, &damage);

    m_RenderData.currentFB->bind();

    // make a stencil for rounded corners to work with blur
    scissor((CBox*)nullptr); // allow the entire window and stencil to render
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
    CBox MONITORBOX             = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
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
    scissor((CBox*)nullptr);

    renderRectWithDamage(box, col, &m_RenderData.damage, round);
}

void CHyprOpenGLImpl::renderRectWithDamage(CBox* box, const CColor& col, CRegion* damage, int round) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderRectWithDamage");

    CBox newBox = *box;
    m_RenderData.renderModif.applyToBox(newBox);

    box = &newBox;

    float matrix[9];
    wlr_matrix_project_box(matrix, box->pWlr(), wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), newBox.rot,
                           m_RenderData.pMonitor->projMatrix.data()); // TODO: write own, don't use WLR here

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

    CBox transformedBox = *box;
    transformedBox.transform(wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
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

void CHyprOpenGLImpl::renderTexture(wlr_texture* tex, CBox* pBox, float alpha, int round, bool allowCustomUV) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTexture(CTexture(tex), pBox, alpha, round, false, allowCustomUV);
}

void CHyprOpenGLImpl::renderTexture(const CTexture& tex, CBox* pBox, float alpha, int round, bool discardActive, bool allowCustomUV) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");

    renderTextureInternalWithDamage(tex, pBox, alpha, &m_RenderData.damage, round, discardActive, false, allowCustomUV, true);

    scissor((CBox*)nullptr);
}

void CHyprOpenGLImpl::renderTextureInternalWithDamage(const CTexture& tex, CBox* pBox, float alpha, CRegion* damage, int round, bool discardActive, bool noAA, bool allowCustomUV,
                                                      bool allowDim) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    TRACY_GPU_ZONE("RenderTextureInternalWithDamage");

    alpha = std::clamp(alpha, 0.f, 1.f);

    if (damage->empty())
        return;

    CBox newBox = *pBox;
    m_RenderData.renderModif.applyToBox(newBox);

    static auto PDIMINACTIVE = CConfigValue<Hyprlang::INT>("decoration:dim_inactive");
    static auto PDT          = CConfigValue<Hyprlang::INT>("debug:damage_tracking");

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform);
    float      matrix[9];
    wlr_matrix_project_box(matrix, newBox.pWlr(), TRANSFORM, newBox.rot, m_RenderData.pMonitor->projMatrix.data());

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

    if ((usingFinalShader && *PDT == 0) || CRASHING) {
        glUniform1f(shader->time, m_tGlobalTimer.getSeconds());
    } else if (usingFinalShader && shader->time != -1) {
        // Don't let time be unitialised
        glUniform1f(shader->time, 0.f);
    }

    if (usingFinalShader && shader->wl_output != -1)
        glUniform1i(shader->wl_output, m_RenderData.pMonitor->ID);
    if (usingFinalShader && shader->fullSize != -1)
        glUniform2f(shader->fullSize, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y);

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

    CBox transformedBox = newBox;
    transformedBox.transform(wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
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
            const auto DIM = m_pCurrentWindow->m_fDimPercent.value();
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

void CHyprOpenGLImpl::renderTexturePrimitive(const CTexture& tex, CBox* pBox) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    TRACY_GPU_ZONE("RenderTexturePrimitive");

    if (m_RenderData.damage.empty())
        return;

    CBox newBox = *pBox;
    m_RenderData.renderModif.applyToBox(newBox);

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform);
    float      matrix[9];
    wlr_matrix_project_box(matrix, newBox.pWlr(), TRANSFORM, newBox.rot, m_RenderData.pMonitor->projMatrix.data());

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

    scissor((CBox*)nullptr);

    glDisableVertexAttribArray(shader->posAttrib);
    glDisableVertexAttribArray(shader->texAttrib);

    glBindTexture(tex.m_iTarget, 0);
}

void CHyprOpenGLImpl::renderTextureMatte(const CTexture& tex, CBox* pBox, CFramebuffer& matte) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture without begin()!");
    RASSERT((tex.m_iTexID > 0), "Attempted to draw NULL texture!");

    TRACY_GPU_ZONE("RenderTextureMatte");

    if (m_RenderData.damage.empty())
        return;

    CBox newBox = *pBox;
    m_RenderData.renderModif.applyToBox(newBox);

    // get transform
    const auto TRANSFORM = wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform);
    float      matrix[9];
    wlr_matrix_project_box(matrix, newBox.pWlr(), TRANSFORM, newBox.rot, m_RenderData.pMonitor->projMatrix.data());

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    CShader* shader = &m_RenderData.pCurrentMonData->m_shMATTE;

    glUseProgram(shader->program);

#ifndef GLES2
    glUniformMatrix3fv(shader->proj, 1, GL_TRUE, glMatrix);
#else
    wlr_matrix_transpose(glMatrix, glMatrix);
    glUniformMatrix3fv(shader->proj, 1, GL_FALSE, glMatrix);
#endif
    glUniform1i(shader->tex, 0);
    glUniform1i(shader->alphaMatte, 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex.m_iTarget, tex.m_iTexID);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(matte.m_cTex.m_iTarget, matte.m_cTex.m_iTexID);

    glVertexAttribPointer(shader->posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
    glVertexAttribPointer(shader->texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

    glEnableVertexAttribArray(shader->posAttrib);
    glEnableVertexAttribArray(shader->texAttrib);

    for (auto& RECT : m_RenderData.damage.getRects()) {
        scissor(&RECT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    scissor((CBox*)nullptr);

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
    CBox       MONITORBOX = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    wlr_matrix_project_box(matrix, MONITORBOX.pWlr(), TRANSFORM, 0, m_RenderData.pMonitor->projMatrix.data());

    float glMatrix[9];
    wlr_matrix_multiply(glMatrix, m_RenderData.projection, matrix);

    // get the config settings
    static auto PBLURSIZE             = CConfigValue<Hyprlang::INT>("decoration:blur:size");
    static auto PBLURPASSES           = CConfigValue<Hyprlang::INT>("decoration:blur:passes");
    static auto PBLURVIBRANCY         = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy");
    static auto PBLURVIBRANCYDARKNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:vibrancy_darkness");

    // prep damage
    CRegion damage{*originalDamage};
    wlr_region_transform(damage.pixman(), damage.pixman(), wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
                         m_RenderData.pMonitor->vecTransformedSize.y);
    wlr_region_expand(damage.pixman(), damage.pixman(), *PBLURPASSES > 10 ? pow(2, 15) : std::clamp(*PBLURSIZE, (int64_t)1, (int64_t)40) * pow(2, *PBLURPASSES));

    // helper
    const auto    PMIRRORFB     = &m_RenderData.pCurrentMonData->mirrorFB;
    const auto    PMIRRORSWAPFB = &m_RenderData.pCurrentMonData->mirrorSwapFB;

    CFramebuffer* currentRenderToFB = PMIRRORFB;

    // Begin with base color adjustments - global brightness and contrast
    // TODO: make this a part of the first pass maybe to save on a drawcall?
    {
        static auto PBLURCONTRAST   = CConfigValue<Hyprlang::FLOAT>("decoration:blur:contrast");
        static auto PBLURBRIGHTNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:brightness");

        PMIRRORSWAPFB->bind();

        glActiveTexture(GL_TEXTURE0);

        glBindTexture(m_RenderData.currentFB->m_cTex.m_iTarget, m_RenderData.currentFB->m_cTex.m_iTexID);

        glTexParameteri(m_RenderData.currentFB->m_cTex.m_iTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glUseProgram(m_RenderData.pCurrentMonData->m_shBLURPREPARE.program);

#ifndef GLES2
        glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBLURPREPARE.proj, 1, GL_TRUE, glMatrix);
#else
        wlr_matrix_transpose(glMatrix, glMatrix);
        glUniformMatrix3fv(m_RenderData.pCurrentMonData->m_shBLURPREPARE.proj, 1, GL_FALSE, glMatrix);
#endif
        glUniform1f(m_RenderData.pCurrentMonData->m_shBLURPREPARE.contrast, *PBLURCONTRAST);
        glUniform1f(m_RenderData.pCurrentMonData->m_shBLURPREPARE.brightness, *PBLURBRIGHTNESS);
        glUniform1i(m_RenderData.pCurrentMonData->m_shBLURPREPARE.tex, 0);

        glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBLURPREPARE.posAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);
        glVertexAttribPointer(m_RenderData.pCurrentMonData->m_shBLURPREPARE.texAttrib, 2, GL_FLOAT, GL_FALSE, 0, fullVerts);

        glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURPREPARE.posAttrib);
        glEnableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURPREPARE.texAttrib);

        if (!damage.empty()) {
            for (auto& RECT : damage.getRects()) {
                scissor(&RECT, false /* this region is already transformed */);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            }
        }

        glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURPREPARE.posAttrib);
        glDisableVertexAttribArray(m_RenderData.pCurrentMonData->m_shBLURPREPARE.texAttrib);

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
        if (pShader == &m_RenderData.pCurrentMonData->m_shBLUR1) {
            glUniform2f(m_RenderData.pCurrentMonData->m_shBLUR1.halfpixel, 0.5f / (m_RenderData.pMonitor->vecPixelSize.x / 2.f),
                        0.5f / (m_RenderData.pMonitor->vecPixelSize.y / 2.f));
            glUniform1i(m_RenderData.pCurrentMonData->m_shBLUR1.passes, *PBLURPASSES);
            glUniform1f(m_RenderData.pCurrentMonData->m_shBLUR1.vibrancy, *PBLURVIBRANCY);
            glUniform1f(m_RenderData.pCurrentMonData->m_shBLUR1.vibrancy_darkness, *PBLURVIBRANCYDARKNESS);
        } else
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

    // finalize the image
    {
        static auto PBLURNOISE      = CConfigValue<Hyprlang::FLOAT>("decoration:blur:noise");
        static auto PBLURBRIGHTNESS = CConfigValue<Hyprlang::FLOAT>("decoration:blur:brightness");

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
    m_mMonitorRenderResources[pMonitor].blurFBDirty = true;
}

void CHyprOpenGLImpl::preRender(CMonitor* pMonitor) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Hyprlang::INT>("decoration:blur:xray");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    if (!*PBLURNEWOPTIMIZE || !m_mMonitorRenderResources[pMonitor].blurFBDirty || !*PBLUR)
        return;

    // ignore if solitary present, nothing to blur
    if (pMonitor->solitaryClient)
        return;

    // check if we need to update the blur fb
    // if there are no windows that would benefit from it,
    // we will ignore that the blur FB is dirty.

    auto windowShouldBeBlurred = [&](CWindow* pWindow) -> bool {
        if (!pWindow)
            return false;

        if (pWindow->m_sAdditionalConfigData.forceNoBlur)
            return false;

        if (pWindow->m_pWLSurface.small() && !pWindow->m_pWLSurface.m_bFillIgnoreSmall)
            return true;

        const auto  PSURFACE = pWindow->m_pWLSurface.wlr();

        const auto  PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
        const float A          = pWindow->m_fAlpha.value() * pWindow->m_fActiveInactiveAlpha.value() * PWORKSPACE->m_fAlpha.value();

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

                if (ls->layerSurface->surface->opaque && ls->alpha.value() >= 1.f)
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
    CBox       wholeMonitor = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    const auto POUTFB       = blurMainFramebufferWithDamage(1, &fakeDamage);

    // render onto blurFB
    m_RenderData.pCurrentMonData->blurFB.alloc(m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y, m_RenderData.pMonitor->drmFormat);
    m_RenderData.pCurrentMonData->blurFB.bind();

    clear(CColor(0, 0, 0, 0));

    m_bEndFrame = true; // fix transformed
    renderTextureInternalWithDamage(POUTFB->m_cTex, &wholeMonitor, 1, &fakeDamage, 0, false, true, false);
    m_bEndFrame = false;

    m_RenderData.currentFB->bind();

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
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLUR            = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");

    return !(!m_RenderData.pCurrentMonData->blurFBDirty || !*PBLURNEWOPTIMIZE || !*PBLUR || !m_RenderData.pCurrentMonData->blurFBShouldRender);
}

bool CHyprOpenGLImpl::shouldUseNewBlurOptimizations(SLayerSurface* pLayer, CWindow* pWindow) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Hyprlang::INT>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Hyprlang::INT>("decoration:blur:xray");

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

void CHyprOpenGLImpl::renderTextureWithBlur(const CTexture& tex, CBox* pBox, float a, wlr_surface* pSurface, int round, bool blockBlurOptimization, float blurA) {
    RASSERT(m_RenderData.pMonitor, "Tried to render texture with blur without begin()!");

    static auto PBLURENABLED     = CConfigValue<Hyprlang::INT>("decoration:blur:enabled");
    static auto PNOBLUROVERSIZED = CConfigValue<Hyprlang::INT>("decoration:no_blur_on_oversized");

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
    if (a >= 1.f && std::round(pSurface->current.width * m_RenderData.pMonitor->scale) == pBox->w &&
        std::round(pSurface->current.height * m_RenderData.pMonitor->scale) == pBox->h) {
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

    m_RenderData.currentFB->bind();

    // make a stencil for rounded corners to work with blur
    scissor((CBox*)nullptr); // allow the entire window and stencil to render
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
    CBox MONITORBOX = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    // render our great blurred FB
    static auto PBLURIGNOREOPACITY = CConfigValue<Hyprlang::INT>("decoration:blur:ignore_opacity");
    m_bEndFrame                    = true; // fix transformed
    const auto SAVEDRENDERMODIF    = m_RenderData.renderModif;
    m_RenderData.renderModif       = {}; // fix shit
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
    scissor((CBox*)nullptr);
}

void pushVert2D(float x, float y, float* arr, int& counter, CBox* box) {
    // 0-1 space god damnit
    arr[counter * 2 + 0] = x / box->width;
    arr[counter * 2 + 1] = y / box->height;
    counter++;
}

void CHyprOpenGLImpl::renderBorder(CBox* box, const CGradientValueData& grad, int round, int borderSize, float a, int outerRound) {
    RASSERT((box->width > 0 && box->height > 0), "Tried to render rect with width/height < 0!");
    RASSERT(m_RenderData.pMonitor, "Tried to render rect without begin()!");

    TRACY_GPU_ZONE("RenderBorder");

    if (m_RenderData.damage.empty() || (m_pCurrentWindow && m_pCurrentWindow->m_sAdditionalConfigData.forceNoBorder))
        return;

    CBox newBox = *box;
    m_RenderData.renderModif.applyToBox(newBox);

    box = &newBox;

    if (borderSize < 1)
        return;

    int scaledBorderSize = std::round(borderSize * m_RenderData.pMonitor->scale);

    // adjust box
    box->x -= scaledBorderSize;
    box->y -= scaledBorderSize;
    box->width += 2 * scaledBorderSize;
    box->height += 2 * scaledBorderSize;

    round += round == 0 ? 0 : scaledBorderSize;

    float matrix[9];
    wlr_matrix_project_box(matrix, box->pWlr(), wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), newBox.rot,
                           m_RenderData.pMonitor->projMatrix.data()); // TODO: write own, don't use WLR here

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

    CBox transformedBox = *box;
    transformedBox.transform(wlr_output_transform_invert(m_RenderData.pMonitor->transform), m_RenderData.pMonitor->vecTransformedSize.x,
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

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    g_pHyprRenderer->makeEGLCurrent();

    pFramebuffer->m_pStencilTex = &m_RenderData.pCurrentMonData->stencilTex;

    pFramebuffer->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->drmFormat);

    g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, pFramebuffer);

    clear(CColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // this is a hack but it works :P
    // we need to disable blur or else we will get a black background, as the shader
    // will try to copy the bg to apply blur.
    // this isn't entirely correct, but like, oh well.
    // small todo: maybe make this correct? :P
    static auto* const PBLUR   = (Hyprlang::INT* const*)(g_pConfigManager->getConfigValuePtr("decoration:blur:enabled"));
    const auto         BLURVAL = **PBLUR;
    **PBLUR                    = 0;

    // TODO: how can we make this the size of the window? setting it to window's size makes the entire screen render with the wrong res forever more. odd.
    glViewport(0, 0, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y);

    m_RenderData.currentFB = pFramebuffer;

    clear(CColor(0, 0, 0, 0)); // JIC

    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, &now, false, RENDER_PASS_ALL, true);

    **PBLUR = BLURVAL;

    g_pHyprRenderer->endRender();
}

void CHyprOpenGLImpl::makeWindowSnapshot(CWindow* pWindow) {
    // we trust the window is valid.
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    if (!g_pHyprRenderer->shouldRenderWindow(pWindow))
        return; // ignore, window is not being rendered

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    g_pHyprRenderer->makeEGLCurrent();

    const auto PFRAMEBUFFER = &m_mWindowFramebuffers[pWindow];

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->drmFormat);

    g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, PFRAMEBUFFER);

    g_pHyprRenderer->m_bRenderingSnapshot = true;

    clear(CColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // this is a hack but it works :P
    // we need to disable blur or else we will get a black background, as the shader
    // will try to copy the bg to apply blur.
    // this isn't entirely correct, but like, oh well.
    // small todo: maybe make this correct? :P
    static auto* const PBLUR   = (Hyprlang::INT* const*)(g_pConfigManager->getConfigValuePtr("decoration:blur:enabled"));
    const auto         BLURVAL = **PBLUR;
    **PBLUR                    = 0;

    clear(CColor(0, 0, 0, 0)); // JIC

    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, &now, !pWindow->m_bX11DoesntWantBorders, RENDER_PASS_ALL);

    **PBLUR = BLURVAL;

    g_pHyprRenderer->endRender();

    g_pHyprRenderer->m_bRenderingSnapshot = false;
}

void CHyprOpenGLImpl::makeLayerSnapshot(SLayerSurface* pLayer) {
    // we trust the window is valid.
    const auto PMONITOR = g_pCompositor->getMonitorFromID(pLayer->monitorID);

    if (!PMONITOR || !PMONITOR->output || PMONITOR->vecPixelSize.x <= 0 || PMONITOR->vecPixelSize.y <= 0)
        return;

    // we need to "damage" the entire monitor
    // so that we render the entire window
    // this is temporary, doesnt mess with the actual wlr damage
    CRegion fakeDamage{0, 0, (int)PMONITOR->vecTransformedSize.x, (int)PMONITOR->vecTransformedSize.y};

    g_pHyprRenderer->makeEGLCurrent();

    const auto PFRAMEBUFFER = &m_mLayerFramebuffers[pLayer];

    PFRAMEBUFFER->alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->drmFormat);

    g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, PFRAMEBUFFER);

    g_pHyprRenderer->m_bRenderingSnapshot = true;

    clear(CColor(0, 0, 0, 0)); // JIC

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const auto BLURLSSTATUS = pLayer->forceBlur;
    pLayer->forceBlur       = false;

    // draw the layer
    g_pHyprRenderer->renderLayer(pLayer, PMONITOR, &now);

    pLayer->forceBlur = BLURLSSTATUS;

    g_pHyprRenderer->endRender();

    g_pHyprRenderer->m_bRenderingSnapshot = false;
}

void CHyprOpenGLImpl::renderSnapshot(CWindow** pWindow) {
    RASSERT(m_RenderData.pMonitor, "Tried to render snapshot rect without begin()!");
    const auto  PWINDOW = *pWindow;

    static auto PDIMAROUND = CConfigValue<Hyprlang::FLOAT>("decoration:dim_around");

    auto        it = m_mWindowFramebuffers.begin();
    for (; it != m_mWindowFramebuffers.end(); it++) {
        if (it->first == PWINDOW) {
            break;
        }
    }

    if (it == m_mWindowFramebuffers.end() || !it->second.m_cTex.m_iTexID)
        return;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);

    CBox       windowBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->scale * PWINDOW->m_vRealSize.value().x / (PWINDOW->m_vOriginalClosedSize.x * PMONITOR->scale)),
                                (PMONITOR->scale * PWINDOW->m_vRealSize.value().y / (PWINDOW->m_vOriginalClosedSize.y * PMONITOR->scale)));

    windowBox.width  = PMONITOR->vecTransformedSize.x * scaleXY.x;
    windowBox.height = PMONITOR->vecTransformedSize.y * scaleXY.y;
    windowBox.x      = ((PWINDOW->m_vRealPosition.value().x - PMONITOR->vecPosition.x) * PMONITOR->scale) - ((PWINDOW->m_vOriginalClosedPos.x * PMONITOR->scale) * scaleXY.x);
    windowBox.y      = ((PWINDOW->m_vRealPosition.value().y - PMONITOR->vecPosition.y) * PMONITOR->scale) - ((PWINDOW->m_vOriginalClosedPos.y * PMONITOR->scale) * scaleXY.y);

    CRegion fakeDamage{0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    if (*PDIMAROUND && (*pWindow)->m_sAdditionalConfigData.dimAround) {
        CBox monbox = {0, 0, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.x, g_pHyprOpenGL->m_RenderData.pMonitor->vecPixelSize.y};
        g_pHyprOpenGL->renderRect(&monbox, CColor(0, 0, 0, *PDIMAROUND * PWINDOW->m_fAlpha.value()));
        g_pHyprRenderer->damageMonitor(PMONITOR);
    }

    m_bEndFrame = true;

    renderTextureInternalWithDamage(it->second.m_cTex, &windowBox, PWINDOW->m_fAlpha.value(), &fakeDamage, 0);

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

    CBox       layerBox;
    // some mafs to figure out the correct box
    // the originalClosedPos is relative to the monitor's pos
    Vector2D scaleXY = Vector2D((PMONITOR->scale * PLAYER->realSize.value().x / (PLAYER->geometry.w * PMONITOR->scale)),
                                (PMONITOR->scale * PLAYER->realSize.value().y / (PLAYER->geometry.h * PMONITOR->scale)));

    layerBox.width  = PMONITOR->vecTransformedSize.x * scaleXY.x;
    layerBox.height = PMONITOR->vecTransformedSize.y * scaleXY.y;
    layerBox.x = ((PLAYER->realPosition.value().x - PMONITOR->vecPosition.x) * PMONITOR->scale) - (((PLAYER->geometry.x - PMONITOR->vecPosition.x) * PMONITOR->scale) * scaleXY.x);
    layerBox.y = ((PLAYER->realPosition.value().y - PMONITOR->vecPosition.y) * PMONITOR->scale) - (((PLAYER->geometry.y - PMONITOR->vecPosition.y) * PMONITOR->scale) * scaleXY.y);

    CRegion fakeDamage{0, 0, PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y};

    m_bEndFrame = true;

    renderTextureInternalWithDamage(it->second.m_cTex, &layerBox, PLAYER->alpha.value(), &fakeDamage, 0);

    m_bEndFrame = false;
}

void CHyprOpenGLImpl::renderRoundedShadow(CBox* box, int round, int range, const CColor& color, float a) {
    RASSERT(m_RenderData.pMonitor, "Tried to render shadow without begin()!");
    RASSERT((box->width > 0 && box->height > 0), "Tried to render shadow with width/height < 0!");
    RASSERT(m_pCurrentWindow, "Tried to render shadow without a window!");

    if (m_RenderData.damage.empty())
        return;

    TRACY_GPU_ZONE("RenderShadow");

    CBox newBox = *box;
    m_RenderData.renderModif.applyToBox(newBox);

    box = &newBox;

    static auto PSHADOWPOWER = CConfigValue<Hyprlang::INT>("decoration:shadow_render_power");

    const auto  SHADOWPOWER = std::clamp((int)*PSHADOWPOWER, 1, 4);

    const auto  col = color;

    float       matrix[9];
    wlr_matrix_project_box(matrix, box->pWlr(), wlr_output_transform_invert(!m_bEndFrame ? WL_OUTPUT_TRANSFORM_NORMAL : m_RenderData.pMonitor->transform), newBox.rot,
                           m_RenderData.pMonitor->projMatrix.data()); // TODO: write own, don't use WLR here

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
        m_RenderData.pCurrentMonData->monitorMirrorFB.alloc(m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y, m_RenderData.pMonitor->drmFormat);

    m_RenderData.pCurrentMonData->monitorMirrorFB.bind();

    CBox monbox = {0, 0, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y};

    blend(false);

    renderTexture(m_RenderData.currentFB->m_cTex, &monbox, 1.f, 0, false, false);

    blend(true);

    m_RenderData.currentFB->bind();
}

void CHyprOpenGLImpl::renderMirrored() {
    CBox       monbox = {0, 0, m_RenderData.pMonitor->vecPixelSize.x, m_RenderData.pMonitor->vecPixelSize.y};

    const auto PFB = &m_mMonitorRenderResources[m_RenderData.pMonitor->pMirrorOf].monitorMirrorFB;

    if (!PFB->isAllocated() || PFB->m_cTex.m_iTexID <= 0)
        return;

    renderTexture(PFB->m_cTex, &monbox, 1.f, 0, false, false);
}

void CHyprOpenGLImpl::renderSplash(cairo_t* const CAIRO, cairo_surface_t* const CAIROSURFACE, double offsetY, const Vector2D& size) {
    static auto PSPLASHCOLOR = CConfigValue<Hyprlang::INT>("misc:col.splash");

    static auto PSPLASHFONT = CConfigValue<std::string>("misc:splash_font_family");

    cairo_select_font_face(CAIRO, (*PSPLASHFONT).c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    const auto FONTSIZE = (int)(size.y / 76);
    cairo_set_font_size(CAIRO, FONTSIZE);

    const auto COLOR = CColor(*PSPLASHCOLOR);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    cairo_text_extents_t textExtents;
    cairo_text_extents(CAIRO, g_pCompositor->m_szCurrentSplash.c_str(), &textExtents);

    cairo_move_to(CAIRO, (size.x - textExtents.width) / 2.0, size.y - textExtents.height + offsetY);

    cairo_show_text(CAIRO, g_pCompositor->m_szCurrentSplash.c_str());

    cairo_surface_flush(CAIROSURFACE);
}

void CHyprOpenGLImpl::createBGTextureForMonitor(CMonitor* pMonitor) {
    RASSERT(m_RenderData.pMonitor, "Tried to createBGTex without begin()!");

    static auto        PRENDERTEX      = CConfigValue<Hyprlang::INT>("misc:disable_hyprland_logo");
    static auto        PNOSPLASH       = CConfigValue<Hyprlang::INT>("misc:disable_splash_rendering");
    static auto        PFORCEWALLPAPER = CConfigValue<Hyprlang::INT>("misc:force_default_wallpaper");

    const auto         FORCEWALLPAPER = std::clamp(*PFORCEWALLPAPER, static_cast<int64_t>(-1L), static_cast<int64_t>(2L));

    static std::string texPath = "";

    if (*PRENDERTEX)
        return;

    // release the last tex if exists
    const auto PFB = &m_mMonitorBGFBs[pMonitor];
    PFB->release();

    PFB->alloc(pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y, pMonitor->drmFormat);
    Debug::log(LOG, "Allocated texture for BGTex");

    // TODO: use relative paths to the installation
    // or configure the paths at build time
    if (texPath.empty()) {
        texPath = "/usr/share/hyprland/wall";

        // get the adequate tex
        if (FORCEWALLPAPER == -1) {
            std::mt19937_64                 engine(time(nullptr));
            std::uniform_int_distribution<> distribution(0, 2);

            texPath += std::to_string(distribution(engine));
        } else
            texPath += std::to_string(std::clamp(*PFORCEWALLPAPER, (int64_t)0, (int64_t)2));

        texPath += ".png";

        // check if wallpapers exist
        if (!std::filesystem::exists(texPath)) {
            // try local
            texPath = texPath.substr(0, 5) + "local/" + texPath.substr(5);

            if (!std::filesystem::exists(texPath))
                return; // the texture will be empty, oh well. We'll clear with a solid color anyways.
        }
    }

    // create a new one with cairo
    CTexture   tex;

    const auto CAIROISURFACE = cairo_image_surface_create_from_png(texPath.c_str());
    const auto CAIROFORMAT   = cairo_image_surface_get_format(CAIROISURFACE);

    tex.allocate();
    const Vector2D IMAGESIZE = {cairo_image_surface_get_width(CAIROISURFACE), cairo_image_surface_get_height(CAIROISURFACE)};

    // calc the target box
    const double MONRATIO = m_RenderData.pMonitor->vecTransformedSize.x / m_RenderData.pMonitor->vecTransformedSize.y;
    const double WPRATIO  = IMAGESIZE.x / IMAGESIZE.y;

    Vector2D     origin;
    double       scale;

    if (MONRATIO > WPRATIO) {
        scale = m_RenderData.pMonitor->vecTransformedSize.x / IMAGESIZE.x;

        origin.y = (m_RenderData.pMonitor->vecTransformedSize.y - IMAGESIZE.y * scale) / 2.0;
    } else {
        scale = m_RenderData.pMonitor->vecTransformedSize.y / IMAGESIZE.y;

        origin.x = (m_RenderData.pMonitor->vecTransformedSize.x - IMAGESIZE.x * scale) / 2.0;
    }

    const Vector2D scaledSize = IMAGESIZE * scale;

    const auto     CAIROSURFACE = cairo_image_surface_create(CAIROFORMAT, scaledSize.x, scaledSize.y);
    const auto     CAIRO        = cairo_create(CAIROSURFACE);

    cairo_set_antialias(CAIRO, CAIRO_ANTIALIAS_GOOD);
    cairo_scale(CAIRO, scale, scale);
    cairo_rectangle(CAIRO, 0, 0, 100, 100);
    cairo_set_source_surface(CAIRO, CAIROISURFACE, 0, 0);
    cairo_paint(CAIRO);

    if (!*PNOSPLASH)
        renderSplash(CAIRO, CAIROSURFACE, origin.y * WPRATIO / MONRATIO * scale, IMAGESIZE);

    cairo_surface_flush(CAIROSURFACE);

    CBox box    = {origin.x, origin.y, IMAGESIZE.x * scale, IMAGESIZE.y * scale};
    tex.m_vSize = IMAGESIZE * scale;

    // copy the data to an OpenGL texture we have
    const GLint glIFormat = CAIROFORMAT == CAIRO_FORMAT_RGB96F ?
#ifdef GLES2
        GL_RGB32F_EXT :
#else
        GL_RGB32F :
#endif
        GL_RGBA;
    const GLint glFormat = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_RGB : GL_RGBA;
    const GLint glType   = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_FLOAT : GL_UNSIGNED_BYTE;

    const auto  DATA = cairo_image_surface_get_data(CAIROSURFACE);
    glBindTexture(GL_TEXTURE_2D, tex.m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#ifndef GLES2
    if (CAIROFORMAT != CAIRO_FORMAT_RGB96F) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    }
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, glIFormat, tex.m_vSize.x, tex.m_vSize.y, 0, glFormat, glType, DATA);

    cairo_surface_destroy(CAIROSURFACE);
    cairo_surface_destroy(CAIROISURFACE);
    cairo_destroy(CAIRO);

    // render the texture to our fb
    PFB->bind();
    CRegion fakeDamage{0, 0, INT16_MAX, INT16_MAX};
    renderTextureInternalWithDamage(tex, &box, 1.0, &fakeDamage);

    // bind back
    if (m_RenderData.currentFB)
        m_RenderData.currentFB->bind();

    Debug::log(LOG, "Background created for monitor {}", pMonitor->szName);
}

void CHyprOpenGLImpl::clearWithTex() {
    RASSERT(m_RenderData.pMonitor, "Tried to render BGtex without begin()!");

    TRACY_GPU_ZONE("RenderClearWithTex");

    auto TEXIT = m_mMonitorBGFBs.find(m_RenderData.pMonitor);

    if (TEXIT == m_mMonitorBGFBs.end()) {
        createBGTextureForMonitor(m_RenderData.pMonitor);
        TEXIT = m_mMonitorBGFBs.find(m_RenderData.pMonitor);
    }

    if (TEXIT != m_mMonitorBGFBs.end()) {
        CBox monbox = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
        m_bEndFrame = true;
        renderTexture(TEXIT->second.m_cTex, &monbox, 1);
        m_bEndFrame = false;
    }
}

void CHyprOpenGLImpl::destroyMonitorResources(CMonitor* pMonitor) {
    g_pHyprRenderer->makeEGLCurrent();

    auto RESIT = g_pHyprOpenGL->m_mMonitorRenderResources.find(pMonitor);
    if (RESIT != g_pHyprOpenGL->m_mMonitorRenderResources.end()) {
        RESIT->second.mirrorFB.release();
        RESIT->second.offloadFB.release();
        RESIT->second.mirrorSwapFB.release();
        RESIT->second.monitorMirrorFB.release();
        RESIT->second.blurFB.release();
        RESIT->second.offMainFB.release();
        RESIT->second.stencilTex.destroyTexture();
        g_pHyprOpenGL->m_mMonitorRenderResources.erase(RESIT);
    }

    auto TEXIT = g_pHyprOpenGL->m_mMonitorBGFBs.find(pMonitor);
    if (TEXIT != g_pHyprOpenGL->m_mMonitorBGFBs.end()) {
        TEXIT->second.release();
        g_pHyprOpenGL->m_mMonitorBGFBs.erase(TEXIT);
    }

    Debug::log(LOG, "Monitor {} -> destroyed all render data", pMonitor->szName);
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
    CBox monbox = {0, 0, m_RenderData.pMonitor->vecTransformedSize.x, m_RenderData.pMonitor->vecTransformedSize.y};
    renderTexturePrimitive(off->m_cTex, &monbox);
}

void CHyprOpenGLImpl::bindBackOnMain() {
    m_RenderData.mainFB->bind();
    m_RenderData.currentFB = m_RenderData.mainFB;
}

void CHyprOpenGLImpl::setMonitorTransformEnabled(bool enabled) {
    m_bEndFrame = enabled;
}

inline const SGLPixelFormat GLES2_FORMATS[] = {
    {
        .drmFormat = DRM_FORMAT_ARGB8888,
        .glFormat  = GL_BGRA_EXT,
        .glType    = GL_UNSIGNED_BYTE,
        .withAlpha = true,
    },
    {
        .drmFormat = DRM_FORMAT_XRGB8888,
        .glFormat  = GL_BGRA_EXT,
        .glType    = GL_UNSIGNED_BYTE,
        .withAlpha = false,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR8888,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_BYTE,
        .withAlpha = false,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR8888,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_BYTE,
        .withAlpha = true,
    },
    {
        .drmFormat = DRM_FORMAT_BGR888,
        .glFormat  = GL_RGB,
        .glType    = GL_UNSIGNED_BYTE,
        .withAlpha = false,
    },
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    {
        .drmFormat = DRM_FORMAT_RGBX4444,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_SHORT_4_4_4_4,
        .withAlpha = false,
    },
    {
        .drmFormat = DRM_FORMAT_RGBA4444,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_SHORT_4_4_4_4,
        .withAlpha = true,
    },
    {
        .drmFormat = DRM_FORMAT_RGBX5551,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_SHORT_5_5_5_1,
        .withAlpha = false,
    },
    {
        .drmFormat = DRM_FORMAT_RGBA5551,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_SHORT_5_5_5_1,
        .withAlpha = true,
    },
    {
        .drmFormat = DRM_FORMAT_RGB565,
        .glFormat  = GL_RGB,
        .glType    = GL_UNSIGNED_SHORT_5_6_5,
        .withAlpha = false,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR2101010,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
        .withAlpha = false,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR2101010,
        .glFormat  = GL_RGBA,
        .glType    = GL_UNSIGNED_INT_2_10_10_10_REV_EXT,
        .withAlpha = true,
    },
    {
        .drmFormat = DRM_FORMAT_XBGR16161616F,
        .glFormat  = GL_RGBA,
        .glType    = GL_HALF_FLOAT_OES,
        .withAlpha = false,
    },
    {
        .drmFormat = DRM_FORMAT_ABGR16161616F,
        .glFormat  = GL_RGBA,
        .glType    = GL_HALF_FLOAT_OES,
        .withAlpha = true,
    },
    {
        .drmFormat        = DRM_FORMAT_XBGR16161616,
        .glInternalFormat = GL_RGBA16_EXT,
        .glFormat         = GL_RGBA,
        .glType           = GL_UNSIGNED_SHORT,
        .withAlpha        = false,
    },
    {
        .drmFormat        = DRM_FORMAT_ABGR16161616,
        .glInternalFormat = GL_RGBA16_EXT,
        .glFormat         = GL_RGBA,
        .glType           = GL_UNSIGNED_SHORT,
        .withAlpha        = true,
    },
#endif
};

uint32_t CHyprOpenGLImpl::getPreferredReadFormat(CMonitor* pMonitor) {
    GLint glf = -1, glt = -1, as = -1;
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &glf);
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &glt);
    glGetIntegerv(GL_ALPHA_BITS, &as);

    if (glf == 0 || glt == 0) {
        glf = drmFormatToGL(pMonitor->drmFormat);
        glt = glFormatToType(glf);
    }

    for (auto& fmt : GLES2_FORMATS) {
        if (fmt.glFormat == glf && fmt.glType == glt && fmt.withAlpha == (as > 0))
            return fmt.drmFormat;
    }

    if (m_sExts.EXT_read_format_bgra)
        return DRM_FORMAT_XRGB8888;

    return DRM_FORMAT_XBGR8888;
}

const SGLPixelFormat* CHyprOpenGLImpl::getPixelFormatFromDRM(uint32_t drmFormat) {
    for (auto& fmt : GLES2_FORMATS) {
        if (fmt.drmFormat == drmFormat)
            return &fmt;
    }

    return nullptr;
}

void SRenderModifData::applyToBox(CBox& box) {
    for (auto& [type, val] : modifs) {
        try {
            switch (type) {
                case RMOD_TYPE_SCALE: box.scale(std::any_cast<float>(val)); break;
                case RMOD_TYPE_SCALECENTER: box.scaleFromCenter(std::any_cast<float>(val)); break;
                case RMOD_TYPE_TRANSLATE: box.translate(std::any_cast<Vector2D>(val)); break;
                case RMOD_TYPE_ROTATE: box.rot += std::any_cast<float>(val); break;
                case RMOD_TYPE_ROTATECENTER: {
                    const auto   THETA = std::any_cast<float>(val);
                    const double COS   = std::cos(THETA);
                    const double SIN   = std::sin(THETA);
                    box.rot += THETA;
                    const auto OLDPOS = box.pos();
                    box.x             = OLDPOS.x * COS - OLDPOS.y * SIN;
                    box.y             = OLDPOS.y * COS + OLDPOS.x * SIN;
                }
            }
        } catch (std::bad_any_cast& e) { Debug::log(ERR, "BUG THIS OR PLUGIN ERROR: caught a bad_any_cast in SRenderModifData::applyToBox!"); }
    }
}
