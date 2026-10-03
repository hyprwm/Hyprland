#include "GLRenderer.hpp"
#include "decorations/CHyprInnerGlowDecoration.hpp"
#include <aquamarine/output/Output.hpp>
#include "../config/ConfigValue.hpp"
#include "../pointer/cursor/CursorManager.hpp"
#include "../pointer/PointerManager.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../debug/Overlay.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../event/EventBus.hpp"
#include "../output/Monitor.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/SurfacePassElement.hpp"
#include "../debug/log/Logger.hpp"
#include "../protocols/types/ContentType.hpp"
#include "../state/MonitorState.hpp"
#include "../helpers/string/StringUtils.hpp"
#include "OpenGL.hpp"
#include "Renderer.hpp"
#include "./gl/GLElementRenderer.hpp"
#include "./gl/GLFramebuffer.hpp"
#include "./gl/GLTexture.hpp"
#include "./gl/blur/Factory.hpp"
#include "./gl/blur/Provider.hpp"

#include <cstdint>
#include <ranges>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;
using namespace Hyprutils::OS;
using enum NContentType::eContentType;
using namespace NColorManagement;
using namespace Render;
using namespace Render::GL;

extern "C" {
#include <xf86drm.h>
}

CHyprGLRenderer::CHyprGLRenderer() : IHyprRenderer(), m_elementRenderer(makeUnique<CGLElementRenderer>()) {
    // KMS can be display-only; classify the active GL renderer instead of the DRM driver.
    g_pHyprOpenGL->makeEGLCurrent();
    if (const auto* renderer = rc<const char*>(glGetString(GL_RENDERER))) {
        const std::string_view name{renderer};
        m_software = StringUtils::containsCaseInsensitive(name, "llvmpipe") || StringUtils::containsCaseInsensitive(name, "softpipe") ||
            StringUtils::containsCaseInsensitive(name, "Software Rasterizer");
    }

    refreshBlurProvider();
    m_preRenderListener = Event::bus()->m_events.render.pre.listen([this](PHLMONITOR monitor) { preRender(monitor); });
}

CHyprGLRenderer::~CHyprGLRenderer() {
    // The renderer is destroyed before OpenGL. Complete even submissions whose
    // readable callbacks will only be discarded during event-loop teardown.
    g_pHyprOpenGL->makeEGLCurrent();
    glFinish();
    m_context.m_usedAsyncBuffers.clear();
    m_pendingBufferUses.clear();
}

IHyprRenderer::eType CHyprGLRenderer::type() {
    return RT_GL;
}

void CHyprGLRenderer::initRender() {
    g_pHyprOpenGL->makeEGLCurrent();
}

bool CHyprGLRenderer::initRenderBuffer(CRenderContext& ctx, SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    try {
        ctx.m_currentRenderbuffer = getOrCreateRenderbuffer(buffer, fmt);
    } catch (std::exception& e) {
        LOG(Log::ERR, "getOrCreateRenderbuffer failed for {}", NFormatUtils::drmFormatName(fmt));
        return false;
    }

    return !!ctx.m_currentRenderbuffer;
}

bool CHyprGLRenderer::beginFullFakeRenderInternal(CRenderContext& ctx, PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    initRender();

    RASSERT(fb, "Cannot render FULL_FAKE without a provided fb!");
    bindFB(ctx, fb);
    if (simple)
        g_pHyprOpenGL->beginSimple(ctx, pMonitor, damage, nullptr, fb);
    else
        return g_pHyprOpenGL->begin(ctx, pMonitor, damage, fb);
    return true;
}

bool CHyprGLRenderer::beginRenderInternal(CRenderContext& ctx, PHLMONITOR pMonitor, CRegion& damage, bool simple) {

    ctx.m_currentRenderbuffer->bind();
    ctx.m_data.currentFB = ctx.m_currentRenderbuffer->getFB();
    if (simple)
        g_pHyprOpenGL->beginSimple(ctx, pMonitor, damage, ctx.m_currentRenderbuffer);
    else
        return g_pHyprOpenGL->begin(ctx, pMonitor, damage);

    return true;
}

SRenderResult CHyprGLRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    if (!m_context.active()) {
        LOG(Log::ERR, "Cannot end rendering without an active render context");
        return {};
    }

    bool              finished = false;
    const CScopeGuard cleanup([&] {
        if (!finished)
            abortRender();
    });

    const auto        PMONITOR           = m_context.m_data.pMonitor;
    const auto        mode               = m_context.m_mode;
    static auto       PNVIDIAANTIFLICKER = CConfigValue<Config::INTEGER>("opengl:nvidia_anti_flicker");

    m_context.m_data.damage = m_context.m_pass.render(m_context, m_context.m_data.damage);

    if (mode != RENDER_MODE_TO_BUFFER_READ_ONLY)
        g_pHyprOpenGL->end(m_context);

    SRenderResult result{
        .finalDamage = m_context.m_data.damage,
    };

    if (mode == RENDER_MODE_NORMAL)
        PMONITOR->m_output->state->setBuffer(m_context.m_currentBuffer);

    // All draws use the same EGL context, so this submission also covers earlier
    // snapshots and aborted draws. Detach their locks before resetting the session.
    mergeSurfaceBufferUses(m_pendingBufferUses, m_context.m_usedAsyncBuffers);
    auto completedBuffers = mode == RENDER_MODE_FULL_FAKE ? std::vector<SSurfaceBufferUse>{} : std::exchange(m_pendingBufferUses, {});

    // Callbacks may begin another render. Release this session before invoking them.
    finishRender();
    finished = true;

    if (mode == RENDER_MODE_FULL_FAKE)
        return result;

    if (!explicitSyncSupported()) {
        LOG(Log::TRACE, "renderer: Explicit sync unsupported, falling back to implicit in endRender");

        // nvidia doesn't have implicit sync, so we have to explicitly wait here, llvmpipe and other software renderer seems to bug out as well.
        if ((isNvidia() && *PNVIDIAANTIFLICKER) || isSoftware())
            glFinish();
        else
            glFlush(); // mark an implicit sync point

        completedBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();

        return result;
    }

    auto eglSync = createSyncFDManager();
    if LIKELY (eglSync && eglSync->isValid()) {
        auto       completionFD      = eglSync->fd().duplicate();
        const bool asyncReleaseReady = completionFD.isValid() && attachSurfaceBufferReleaseFences(completedBuffers, eglSync->fd());
        if (mode == RENDER_MODE_NORMAL) {
            PMONITOR->m_inFence = eglSync->takeFd();
            PMONITOR->m_output->state->setExplicitInFence(PMONITOR->m_inFence.get());
        }

        // This may run inline. Finish all monitor bookkeeping before handing off.
        eglSync.reset();
        if (!asyncReleaseReady) {
            LOG(Log::ERR, "renderer: Failed to prepare release fences, waiting for GPU completion");
            // A missing completion FD or release fence cannot protect explicit-sync clients.
            glFinish();
            completedBuffers.clear();
            if (renderingDoneCallback)
                renderingDoneCallback();
            return result;
        }

        releaseSurfaceBuffersOnReadable(std::move(completionFD), std::move(completedBuffers), renderingDoneCallback);
    } else {
        LOG(Log::ERR, "renderer: Explicit sync failed, waiting for GPU completion");

        // Without a fence, explicit-sync clients need GPU completion before release.
        glFinish();

        if (mode == RENDER_MODE_NORMAL && PMONITOR) {
            PMONITOR->m_inFence.reset();
            PMONITOR->m_output->state->resetExplicitFences();
        }

        eglSync.reset();
        completedBuffers.clear();
        if (renderingDoneCallback)
            renderingDoneCallback();
    }

    return result;
}

void CHyprGLRenderer::abortRender() {
    // Immediate draws may already have sampled these buffers. A later submission
    // (or shutdown) must synchronize them, without reporting a completed frame.
    mergeSurfaceBufferUses(m_pendingBufferUses, m_context.m_usedAsyncBuffers);
    IHyprRenderer::abortRender();
}

void CHyprGLRenderer::renderOffToMain(CRenderContext& ctx, SP<IFramebuffer> off) {
    g_pHyprOpenGL->renderOffToMain(ctx, off);
}

SP<IRenderbuffer> CHyprGLRenderer::getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLRenderbuffer>(buffer, fmt);
}

UP<ISyncFDManager> CHyprGLRenderer::createSyncFDManager() {
    return CEGLSync::create();
}

SP<ITexture> CHyprGLRenderer::createStencilTexture(const int width, const int height) {
    g_pHyprOpenGL->makeEGLCurrent();
    auto tex = makeShared<CGLTexture>();
    tex->allocate({width, height});

    return tex;
}

SP<ITexture> CHyprGLRenderer::createTexture(bool opaque) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(opaque);
}

SP<ITexture> CHyprGLRenderer::createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(drmFormat, pixels, stride, size, keepDataCopy, opaque);
}

SP<ITexture> CHyprGLRenderer::createTexture(const Aquamarine::SDMABUFAttrs& attrs, bool opaque) {
    g_pHyprOpenGL->makeEGLCurrent();
    const auto image = g_pHyprOpenGL->createEGLImage(attrs);
    if (!image)
        return nullptr;
    return makeShared<CGLTexture>(attrs, image, opaque);
}

SP<ITexture> CHyprGLRenderer::createTexture(const int width, const int height, unsigned char* const data) {
    g_pHyprOpenGL->makeEGLCurrent();
    SP<ITexture> tex = makeShared<CGLTexture>();

    tex->allocate({width, height}, DRM_FORMAT_ARGB8888); // FIXME assume DRM_FORMAT_ARGB8888

    tex->m_size = {width, height};
    // copy the data to an OpenGL texture we have
    const GLint glFormat = GL_RGBA;
    const GLint glType   = GL_UNSIGNED_BYTE;

    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);

    glTexImage2D(GL_TEXTURE_2D, 0, glFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, data);
    tex->unbind();

    return tex;
}

SP<ITexture> CHyprGLRenderer::createTexture(cairo_surface_t* cairo) {
    g_pHyprOpenGL->makeEGLCurrent();
    const auto CAIROFORMAT = cairo_image_surface_get_format(cairo);
    auto       tex         = makeShared<CGLTexture>();

    tex->allocate({cairo_image_surface_get_width(cairo), cairo_image_surface_get_height(cairo)});

    const GLint glIFormat = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_RGB32F : GL_RGBA;
    const GLint glFormat  = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_RGB : GL_RGBA;
    const GLint glType    = CAIROFORMAT == CAIRO_FORMAT_RGB96F ? GL_FLOAT : GL_UNSIGNED_BYTE;

    const auto  DATA = cairo_image_surface_get_data(cairo);
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    if (CAIROFORMAT != CAIRO_FORMAT_RGB96F) {
        tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
        tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);
        tex->m_drmFormat = DRM_FORMAT_ARGB8888;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, glIFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, DATA);

    return tex;
}

SP<ITexture> CHyprGLRenderer::createTexture(std::span<const float> lut3D, size_t N) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(lut3D, N);
}

bool CHyprGLRenderer::explicitSyncSupported() {
    return g_pHyprOpenGL->explicitSyncSupported();
}

bool CHyprGLRenderer::fp16Supported() {
    return g_pHyprOpenGL->fp16Supported();
}

std::vector<SDRMFormat> CHyprGLRenderer::getDRMFormats() {
    return g_pHyprOpenGL->getDRMFormats();
}

std::vector<uint64_t> CHyprGLRenderer::getDRMFormatModifiers(DRMFormat format) {
    return g_pHyprOpenGL->getDRMFormatModifiers(format);
}

SP<IFramebuffer> CHyprGLRenderer::createFB(const std::string& name) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLFramebuffer>(name);
}

void CHyprGLRenderer::disableScissor() {
    g_pHyprOpenGL->disableScissor();
}

void CHyprGLRenderer::blend(bool enabled) {
    g_pHyprOpenGL->blend(enabled);
}

void CHyprGLRenderer::drawShadow(CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a,
                                 const Render::SWindowRenderPresentation& presentation) {
    g_pHyprOpenGL->renderRoundedShadow(ctx, box, round, roundingPower, range, color, a, presentation);
}

void CHyprGLRenderer::drawShadow(CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                                 const Config::CGradientValueData& grad2, float lerp, float a, const Render::SWindowRenderPresentation& presentation) {
    g_pHyprOpenGL->renderRoundedShadow(ctx, box, round, roundingPower, range, grad1, grad2, lerp, a, presentation);
}

void CHyprGLRenderer::drawGlow(CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) {
    g_pHyprOpenGL->renderInnerGlow(ctx, box, round, roundingPower, range, color, 0, a);
}

void CHyprGLRenderer::drawGlow(CRenderContext& ctx, const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1,
                               const Config::CGradientValueData& grad2, float lerp, float a) {
    g_pHyprOpenGL->renderInnerGlow(ctx, box, round, roundingPower, range, grad1, grad2, lerp, 0, a);
}

SP<IFramebuffer> CHyprGLRenderer::blurFramebuffer(CRenderContext& ctx, SP<IFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context) {
    RASSERT(m_blur, "Cannot blur without a blur provider");
    return m_blur->blur(ctx, source, strength, originalDamage, context);
}

void CHyprGLRenderer::refreshBlurProvider() {
    static auto PBLURTYPE = CConfigValue<Config::INTEGER>("decoration:blur:variant");

    const auto  type = sc<eBlurType>(*PBLURTYPE);
    if (m_blur && m_blur->type() == type)
        return;

    m_blur = createBlurProvider(type, *g_pHyprOpenGL);
}

void CHyprGLRenderer::expandBlurDamage(CRegion& damage, float multiplier) const {
    RASSERT(m_blur, "Cannot expand blur damage without a blur provider");
    m_blur->expandDamage(damage, multiplier);
}

bool CHyprGLRenderer::blurProviderIsAnimated(CRenderContext& ctx) const {
    return m_blur && m_blur->isAnimated(ctx);
}

bool CHyprGLRenderer::blurProviderRequiresLiveBlur() const {
    return m_blur && m_blur->requiresLiveBlur();
}

void CHyprGLRenderer::preRender(PHLMONITOR pMonitor) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Config::INTEGER>("decoration:blur:xray");
    static auto PBLUR            = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

    // Resource changes can invalidate the blur framebuffer, so resolve them before checking its state.
    (void)pMonitor->resources();

    if (!*PBLURNEWOPTIMIZE || !pMonitor->m_blurFBDirty || !*PBLUR)
        return;

    if (!pMonitor->m_solitaryClient.expired())
        return;

    auto windowShouldBeBlurred = [](PHLWINDOW pWindow) -> bool {
        if (!pWindow || pWindow->m_ruleApplicator->noBlur().valueOrDefault())
            return false;

        if (pWindow->wlSurface()->small() && !pWindow->wlSurface()->m_fillIgnoreSmall)
            return true;

        const auto  PSURFACE   = pWindow->wlSurface()->resource();
        const auto  PWORKSPACE = pWindow->m_workspace;
        const float A          = pWindow->presentation().alphaValue(Desktop::View::WINDOW_ALPHA_FADE) * pWindow->presentation().alphaValue(Desktop::View::WINDOW_ALPHA_FULLSCREEN) *
            pWindow->presentation().alphaValue(Desktop::View::WINDOW_ALPHA_LAYOUT) * pWindow->presentation().alphaValue(Desktop::View::WINDOW_ALPHA_ACTIVE) *
            PWORKSPACE->m_alpha->value();

        if (A < 1.F)
            return true;

        pixman_box32_t surfbox = {0, 0, PSURFACE->m_current.size.x, PSURFACE->m_current.size.y};
        CRegion        inverseOpaque;
        CRegion        opaqueRegion{PSURFACE->m_current.opaque};
        inverseOpaque.set(opaqueRegion).invert(&surfbox).intersect(0, 0, PSURFACE->m_current.size.x, PSURFACE->m_current.size.y);
        return !inverseOpaque.empty();
    };

    bool hasWindows = false;
    for (const auto& w : Desktop::windowState()->windows()) {
        const auto& XRAY_RULE           = w->m_ruleApplicator->xray();
        const bool  XRAY                = XRAY_RULE.hasValue() ? XRAY_RULE.valueOrDefault() : *PBLURXRAY;
        const bool  ON_ACTIVE_WORKSPACE = w->m_workspace && (w->m_workspace == pMonitor->m_activeWorkspace || w->m_workspace == pMonitor->m_activeSpecialWorkspace);
        if (!ON_ACTIVE_WORKSPACE || !w->mapped() || !w->acceptsInput() || !w->alphaNonZero() || ((w->isFloating() || w->onSpecialWorkspace()) && !XRAY) ||
            !windowShouldBeBlurred(w))
            continue;

        hasWindows = true;
        break;
    }

    if (!hasWindows) {
        for (const auto& m : State::monitorState()->monitors()) {
            for (const auto& layer : m->m_layerSurfaceLayers) {
                if (std::ranges::any_of(layer, [](const auto& ls) { return ls->m_layerSurface && ls->m_ruleApplicator->xray().valueOrDefault() == 1; })) {
                    hasWindows = true;
                    break;
                }
            }

            if (hasWindows)
                break;
        }
    }

    if (!hasWindows)
        return;

    g_pHyprRenderer->damageMonitor(pMonitor);
    pMonitor->m_blurFBShouldRender = true;
}

void CHyprGLRenderer::setViewport(int x, int y, int width, int height) {
    g_pHyprOpenGL->setViewport(x, y, width, height);
}

bool CHyprGLRenderer::reloadShaders(const std::string& path) {
    return g_pHyprOpenGL->initShaders(path);
}

SP<ITexture> CHyprGLRenderer::getBlurTexture(PHLMONITORREF pMonitor) {
    return pMonitor->resources()->m_blurFB->getTexture();
}

void CHyprGLRenderer::unsetEGL() {
    if (!g_pHyprOpenGL)
        return;

    eglMakeCurrent(g_pHyprOpenGL->m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

WP<IElementRenderer> CHyprGLRenderer::elementRenderer() {
    return m_elementRenderer;
}
