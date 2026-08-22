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
    refreshBlurProvider();
    m_preRenderListener = Event::bus()->m_events.render.pre.listen([this](PHLMONITOR monitor) { preRender(monitor); });
}

CHyprGLRenderer::~CHyprGLRenderer() = default;

IHyprRenderer::eType CHyprGLRenderer::type() {
    return RT_GL;
}

void CHyprGLRenderer::initRender() {
    g_pHyprOpenGL->makeEGLCurrent();
    g_pHyprRenderer->m_renderData.pMonitor = renderData().pMonitor;
}

bool CHyprGLRenderer::initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    try {
        m_currentRenderbuffer = getOrCreateRenderbuffer(m_currentBuffer, fmt);
    } catch (std::exception& e) {
        Log::logger->log(Log::ERR, "getOrCreateRenderbuffer failed for {}", NFormatUtils::drmFormatName(fmt));
        return false;
    }

    return !!m_currentRenderbuffer;
}

bool CHyprGLRenderer::beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    initRender();

    RASSERT(fb, "Cannot render FULL_FAKE without a provided fb!");
    bindFB(fb);
    if (simple)
        g_pHyprOpenGL->beginSimple(pMonitor, damage, nullptr, fb);
    else
        g_pHyprOpenGL->begin(pMonitor, damage, fb);
    return true;
}

bool CHyprGLRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple) {

    m_currentRenderbuffer->bind();
    if (simple)
        g_pHyprOpenGL->beginSimple(pMonitor, damage, m_currentRenderbuffer);
    else
        g_pHyprOpenGL->begin(pMonitor, damage);

    return true;
}

void CHyprGLRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    const auto  PMONITOR           = g_pHyprRenderer->m_renderData.pMonitor;
    static auto PNVIDIAANTIFLICKER = CConfigValue<Config::INTEGER>("opengl:nvidia_anti_flicker");

    g_pHyprRenderer->m_renderData.damage = m_renderPass.render(g_pHyprRenderer->m_renderData.damage);

    auto cleanup = CScopeGuard([this]() {
        if (m_currentRenderbuffer)
            m_currentRenderbuffer->unbind();
        m_currentRenderbuffer = nullptr;
        m_currentBuffer       = nullptr;
    });

    if (m_renderMode != RENDER_MODE_TO_BUFFER_READ_ONLY)
        g_pHyprOpenGL->end();
    else {
        g_pHyprRenderer->m_renderData.pMonitor.reset();
        g_pHyprRenderer->m_renderData.mouseZoomFactor   = 1.f;
        g_pHyprRenderer->m_renderData.mouseZoomUseMouse = true;
    }

    if (m_renderMode == RENDER_MODE_FULL_FAKE)
        return;

    if (m_renderMode == RENDER_MODE_NORMAL)
        PMONITOR->m_output->state->setBuffer(m_currentBuffer);

    if (!explicitSyncSupported()) {
        Log::logger->log(Log::TRACE, "renderer: Explicit sync unsupported, falling back to implicit in endRender");

        // nvidia doesn't have implicit sync, so we have to explicitly wait here, llvmpipe and other software renderer seems to bug out as well.
        if ((isNvidia() && *PNVIDIAANTIFLICKER) || isSoftware())
            glFinish();
        else
            glFlush(); // mark an implicit sync point

        PMONITOR->m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();

        return;
    }

    auto eglSync = createSyncFDManager();
    if LIKELY (eglSync && eglSync->isValid()) {
        for (auto& buf : PMONITOR->m_usedAsyncBuffers) {
            if (buf.first.expired()) // surface is gone.
                continue;

            for (const auto& releaser : buf.second->m_syncReleasers) {
                releaser->addSyncFileFd(eglSync->fd());
            }
        }

        // release buffer refs with release points now, since syncReleaser handles actual buffer release based on EGLSync
        std::erase_if(PMONITOR->m_usedAsyncBuffers, [](const auto& buf) { return buf.first.expired() || !buf.second->m_syncReleasers.empty(); });

        // release buffer refs without release points when EGLSync sync_file/fence is signalled
        g_pEventLoopManager->doOnReadable(eglSync->fd().duplicate(), [renderingDoneCallback, prevbfs = std::move(PMONITOR->m_usedAsyncBuffers)]() mutable {
            prevbfs.clear();
            if (renderingDoneCallback)
                renderingDoneCallback();
        });
        PMONITOR->m_usedAsyncBuffers.clear();

        if (m_renderMode == RENDER_MODE_NORMAL) {
            PMONITOR->m_inFence = eglSync->takeFd();
            PMONITOR->m_output->state->setExplicitInFence(PMONITOR->m_inFence.get());
        }
    } else {
        Log::logger->log(Log::ERR, "renderer: Explicit sync failed, falling back to implicit sync");

        // Establish an implicit synchronization point without blocking the render loop.
        glFlush();

        if (m_renderMode == RENDER_MODE_NORMAL && PMONITOR) {
            PMONITOR->m_inFence.reset();
            PMONITOR->m_output->state->resetExplicitFences();
        }

        PMONITOR->m_usedAsyncBuffers.clear();
        if (renderingDoneCallback)
            renderingDoneCallback();
    }
}

void CHyprGLRenderer::renderOffToMain(SP<IFramebuffer> off) {
    g_pHyprOpenGL->renderOffToMain(off);
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
    g_pHyprOpenGL->scissor(nullptr);
}

void CHyprGLRenderer::blend(bool enabled) {
    g_pHyprOpenGL->blend(enabled);
}

void CHyprGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) {
    g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, color, a);
}

void CHyprGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,
                                 float lerp, float a) {
    g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, grad1, grad2, lerp, a);
}

void CHyprGLRenderer::drawGlow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& color, float a) {
    g_pHyprOpenGL->renderInnerGlow(box, round, roundingPower, range, color, 0, a);
}

void CHyprGLRenderer::drawGlow(const CBox& box, int round, float roundingPower, int range, const Config::CGradientValueData& grad1, const Config::CGradientValueData& grad2,
                               float lerp, float a) {
    g_pHyprOpenGL->renderInnerGlow(box, round, roundingPower, range, grad1, grad2, lerp, 0, a);
}

SP<IFramebuffer> CHyprGLRenderer::blurFramebuffer(SP<IFramebuffer> source, float strength, const CRegion& originalDamage, const SBlurContext& context) {
    RASSERT(m_blur, "Cannot blur without a blur provider");
    return m_blur->blur(source, strength, originalDamage, context);
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

bool CHyprGLRenderer::blurProviderIsAnimated() const {
    return m_blur && m_blur->isAnimated();
}

bool CHyprGLRenderer::blurProviderRequiresLiveBlur() const {
    return m_blur && m_blur->requiresLiveBlur();
}

void CHyprGLRenderer::preRender(PHLMONITOR pMonitor) {
    static auto PBLURNEWOPTIMIZE = CConfigValue<Config::INTEGER>("decoration:blur:new_optimizations");
    static auto PBLURXRAY        = CConfigValue<Config::INTEGER>("decoration:blur:xray");
    static auto PBLUR            = CConfigValue<Config::INTEGER>("decoration:blur:enabled");

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
