#include "GLRenderer.hpp"
#include "../Compositor.hpp"
#include "../helpers/math/Math.hpp"
#include <algorithm>
#include <aquamarine/output/Output.hpp>
#include <filesystem>
#include "../config/ConfigValue.hpp"
#include "../config/ConfigManager.hpp"
#include "../managers/CursorManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../managers/animation/AnimationManager.hpp"
#include "../layout/LayoutManager.hpp"
#include "../desktop/view/Window.hpp"
#include "../desktop/view/LayerSurface.hpp"
#include "../desktop/view/GlobalViewMethods.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../protocols/SessionLock.hpp"
#include "../protocols/LayerShell.hpp"
#include "../protocols/XDGShell.hpp"
#include "../protocols/PresentationTime.hpp"
#include "../protocols/core/DataDevice.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/DRMSyncobj.hpp"
#include "../protocols/LinuxDMABUF.hpp"
#include "../helpers/sync/SyncTimeline.hpp"
#include "../hyprerror/HyprError.hpp"
#include "../debug/HyprDebugOverlay.hpp"
#include "../debug/HyprNotificationOverlay.hpp"
#include "../i18n/Engine.hpp"
#include "helpers/CursorShapes.hpp"
#include "helpers/Monitor.hpp"
#include "pass/TexPassElement.hpp"
#include "pass/ClearPassElement.hpp"
#include "pass/RectPassElement.hpp"
#include "pass/RendererHintsPassElement.hpp"
#include "pass/SurfacePassElement.hpp"
#include "debug/log/Logger.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../protocols/types/ContentType.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "render/OpenGL.hpp"
#include "render/Renderer.hpp"
#include "render/gl/GLFramebuffer.hpp"
#include "render/gl/GLTexture.hpp"
#include "render/vulkan/Vulkan.hpp"
#include "decorations/CHyprDropShadowDecoration.hpp"

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;
using namespace Hyprutils::OS;
using enum NContentType::eContentType;
using namespace NColorManagement;

extern "C" {
#include <xf86drm.h>
}

CHyprGLRenderer::CHyprGLRenderer() : IHyprRenderer() {}

void CHyprGLRenderer::initRender() {
    g_pHyprOpenGL->makeEGLCurrent();
    g_pHyprRenderer->m_renderData.pMonitor = renderData().pMonitor;
}

bool CHyprGLRenderer::initRenderBuffer(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    try {
        g_pHyprOpenGL->m_renderData.m_currentRenderbuffer = getOrCreateRenderbuffer(m_currentBuffer, fmt);
    } catch (std::exception& e) {
        Log::logger->log(Log::ERR, "getOrCreateRenderbuffer failed for {}", NFormatUtils::drmFormatName(fmt));
        return false;
    }

    return g_pHyprOpenGL->m_renderData.m_currentRenderbuffer;
}

bool CHyprGLRenderer::beginFullFakeRenderInternal(PHLMONITOR pMonitor, CRegion& damage, SP<IFramebuffer> fb, bool simple) {
    initRender();

    RASSERT(fb, "Cannot render FULL_FAKE without a provided fb!");
    GLFB(fb)->bind();
    if (simple)
        g_pHyprOpenGL->beginSimple(pMonitor, damage, nullptr, fb);
    else
        g_pHyprOpenGL->begin(pMonitor, damage, fb);
    return true;
}

bool CHyprGLRenderer::beginRenderInternal(PHLMONITOR pMonitor, CRegion& damage, bool simple) {

    g_pHyprOpenGL->m_renderData.m_currentRenderbuffer->bind();
    if (simple)
        g_pHyprOpenGL->beginSimple(pMonitor, damage, g_pHyprOpenGL->m_renderData.m_currentRenderbuffer);
    else
        g_pHyprOpenGL->begin(pMonitor, damage);

    return true;
}

void CHyprGLRenderer::endRender(const std::function<void()>& renderingDoneCallback) {
    const auto  PMONITOR           = g_pHyprRenderer->m_renderData.pMonitor;
    static auto PNVIDIAANTIFLICKER = CConfigValue<Hyprlang::INT>("opengl:nvidia_anti_flicker");

    g_pHyprOpenGL->m_renderData.damage = m_renderPass.render(g_pHyprOpenGL->m_renderData.damage);

    auto cleanup = CScopeGuard([this]() {
        if (g_pHyprOpenGL->m_renderData.m_currentRenderbuffer)
            g_pHyprOpenGL->m_renderData.m_currentRenderbuffer->unbind();
        g_pHyprOpenGL->m_renderData.m_currentRenderbuffer = nullptr;
        m_currentBuffer                                   = nullptr;
    });

    if (m_renderMode != RENDER_MODE_TO_BUFFER_READ_ONLY)
        g_pHyprOpenGL->end();
    else {
        g_pHyprRenderer->m_renderData.pMonitor.reset();
        g_pHyprRenderer->m_renderData.mouseZoomFactor = 1.f;
        g_pHyprOpenGL->m_renderData.mouseZoomUseMouse = true;
    }

    if (m_renderMode == RENDER_MODE_FULL_FAKE)
        return;

    if (m_renderMode == RENDER_MODE_NORMAL)
        PMONITOR->m_output->state->setBuffer(m_currentBuffer);

    if (!explicitSyncSupported()) {
        Log::logger->log(Log::TRACE, "renderer: Explicit sync unsupported, falling back to implicit in endRender");

        // nvidia doesn't have implicit sync, so we have to explicitly wait here, llvmpipe and other software renderer seems to bug out aswell.
        if ((isNvidia() && *PNVIDIAANTIFLICKER) || isSoftware())
            glFinish();
        else
            glFlush(); // mark an implicit sync point

        m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();

        return;
    }

    UP<CEGLSync> eglSync = CEGLSync::create();
    if LIKELY (eglSync && eglSync->isValid()) {
        for (auto const& buf : m_usedAsyncBuffers) {
            for (const auto& releaser : buf->m_syncReleasers) {
                releaser->addSyncFileFd(eglSync->fd());
            }
        }

        // release buffer refs with release points now, since syncReleaser handles actual buffer release based on EGLSync
        std::erase_if(m_usedAsyncBuffers, [](const auto& buf) { return !buf->m_syncReleasers.empty(); });

        // release buffer refs without release points when EGLSync sync_file/fence is signalled
        g_pEventLoopManager->doOnReadable(eglSync->fd().duplicate(), [renderingDoneCallback, prevbfs = std::move(m_usedAsyncBuffers)]() mutable {
            prevbfs.clear();
            if (renderingDoneCallback)
                renderingDoneCallback();
        });
        m_usedAsyncBuffers.clear();

        if (m_renderMode == RENDER_MODE_NORMAL) {
            PMONITOR->m_inFence = eglSync->takeFd();
            PMONITOR->m_output->state->setExplicitInFence(PMONITOR->m_inFence.get());
        }
    } else {
        Log::logger->log(Log::ERR, "renderer: Explicit sync failed, releasing resources");

        m_usedAsyncBuffers.clear(); // release all buffer refs and hope implicit sync works
        if (renderingDoneCallback)
            renderingDoneCallback();
    }
}

SP<ITexture> CHyprGLRenderer::createTexture(bool opaque) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(opaque);
}

SP<ITexture> CHyprGLRenderer::createTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(drmFormat, pixels, stride, size, keepDataCopy, opaque);
}

SP<ITexture> CHyprGLRenderer::createTexture(const Aquamarine::SDMABUFAttrs& attrs, void* image, bool opaque) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLTexture>(attrs, image, opaque);
}

SP<ITexture> CHyprGLRenderer::createTexture(const int width, const int height, unsigned char* const data) {
    g_pHyprOpenGL->makeEGLCurrent();
    SP<ITexture> tex = makeShared<CGLTexture>();

    tex->allocate();

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

    tex->allocate();
    tex->m_size = {cairo_image_surface_get_width(cairo), cairo_image_surface_get_height(cairo)};

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
    }

    glTexImage2D(GL_TEXTURE_2D, 0, glIFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, DATA);

    return tex;
}

void* CHyprGLRenderer::createImage(const SP<Aquamarine::IBuffer> buffer) {
    g_pHyprOpenGL->makeEGLCurrent();
    return g_pHyprOpenGL->createEGLImage(buffer->dmabuf());
}

bool CHyprGLRenderer::explicitSyncSupported() {
    return g_pHyprOpenGL->explicitSyncSupported();
}

std::vector<SDRMFormat> CHyprGLRenderer::getDRMFormats() {
    return g_pHyprOpenGL->getDRMFormats();
}

void CHyprGLRenderer::cleanWindowResources(Desktop::View::CWindow* window) {
    if (!g_pHyprOpenGL)
        return;

    g_pHyprOpenGL->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_windowFramebuffers, [&](const auto& other) { return other.first.expired() || other.first.get() == window; });
}

void CHyprGLRenderer::cleanPopupResources(Desktop::View::CPopup* popup) {
    g_pHyprOpenGL->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_popupFramebuffers, [&](const auto& other) { return other.first.expired() || other.first.get() == popup; });
}

void CHyprGLRenderer::cleanLsResources(Desktop::View::CLayerSurface* ls) {
    g_pHyprOpenGL->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_layerFramebuffers, [&](const auto& other) { return other.first.expired() || other.first.get() == ls; });
}

SP<IFramebuffer> CHyprGLRenderer::createFB() {
    return makeShared<CGLFramebuffer>();
}

void CHyprGLRenderer::draw(CBorderPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;
    if (m_data.hasGrad2)
        g_pHyprOpenGL->renderBorder(
            m_data.box, m_data.grad1, m_data.grad2, m_data.lerp,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
    else
        g_pHyprOpenGL->renderBorder(
            m_data.box, m_data.grad1,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
};

void CHyprGLRenderer::draw(CClearPassElement* element, const CRegion& damage) {
    g_pHyprOpenGL->clear(element->m_data.color);
};

void CHyprGLRenderer::draw(CFramebufferElement* element, const CRegion& damage) {
    const auto       m_data = element->m_data;
    SP<IFramebuffer> fb     = nullptr;

    if (m_data.main) {
        switch (m_data.framebufferID) {
            case FB_MONITOR_RENDER_MAIN: fb = g_pHyprOpenGL->m_renderData.mainFB; break;
            case FB_MONITOR_RENDER_CURRENT: fb = g_pHyprOpenGL->m_renderData.currentFB; break;
            case FB_MONITOR_RENDER_OUT: fb = g_pHyprOpenGL->m_renderData.outFB; break;
            default: fb = nullptr;
        }

        if (!fb) {
            Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: main but null");
            return;
        }

    } else {
        switch (m_data.framebufferID) {
            case FB_MONITOR_RENDER_EXTRA_OFFLOAD: fb = g_pHyprOpenGL->m_renderData.pCurrentMonData->offloadFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR: fb = g_pHyprOpenGL->m_renderData.pCurrentMonData->mirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP: fb = g_pHyprOpenGL->m_renderData.pCurrentMonData->mirrorSwapFB; break;
            case FB_MONITOR_RENDER_EXTRA_OFF_MAIN: fb = g_pHyprOpenGL->m_renderData.pCurrentMonData->offMainFB; break;
            case FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR: fb = g_pHyprOpenGL->m_renderData.pCurrentMonData->monitorMirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_BLUR: fb = g_pHyprOpenGL->m_renderData.pCurrentMonData->blurFB; break;
            default: fb = nullptr;
        }

        if (!fb) {
            Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: not main but null");
            return;
        }
    }

    GLFB(fb)->bind();
};

void CHyprGLRenderer::draw(CPreBlurElement* element, const CRegion& damage) {
    g_pHyprOpenGL->preBlurForCurrentMonitor();
};

void CHyprGLRenderer::draw(CRectPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;

    if (m_data.box.w <= 0 || m_data.box.h <= 0)
        return;

    if (!m_data.clipBox.empty())
        g_pHyprOpenGL->m_renderData.clipBox = m_data.clipBox;

    if (m_data.color.a == 1.F || !m_data.blur)
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color, {.damage = &damage, .round = m_data.round, .roundingPower = m_data.roundingPower});
    else
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color,
                                  {.round = m_data.round, .roundingPower = m_data.roundingPower, .blur = true, .blurA = m_data.blurA, .xray = m_data.xray});

    g_pHyprOpenGL->m_renderData.clipBox = {};
};

void CHyprGLRenderer::draw(CRendererHintsPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;
    if (m_data.renderModif.has_value())
        g_pHyprOpenGL->m_renderData.renderModif = *m_data.renderModif;
};

void CHyprGLRenderer::draw(CShadowPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;
    m_data.deco->render(g_pHyprRenderer->m_renderData.pMonitor.lock(), m_data.a);
};

void CHyprGLRenderer::draw(CSurfacePassElement* element, const CRegion& damage) {
    const auto m_data                              = element->m_data;
    g_pHyprOpenGL->m_renderData.currentWindow      = m_data.pWindow;
    g_pHyprOpenGL->m_renderData.surface            = m_data.surface;
    g_pHyprOpenGL->m_renderData.currentLS          = m_data.pLS;
    g_pHyprOpenGL->m_renderData.clipBox            = m_data.clipBox;
    g_pHyprOpenGL->m_renderData.discardMode        = m_data.discardMode;
    g_pHyprOpenGL->m_renderData.discardOpacity     = m_data.discardOpacity;
    g_pHyprOpenGL->m_renderData.useNearestNeighbor = m_data.useNearestNeighbor;
    g_pHyprOpenGL->pushMonitorTransformEnabled(m_data.flipEndFrame);

    CScopeGuard x = {[]() {
        g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        g_pHyprOpenGL->m_renderData.useNearestNeighbor            = false;
        g_pHyprOpenGL->m_renderData.clipBox                       = {};
        g_pHyprOpenGL->m_renderData.clipRegion                    = {};
        g_pHyprOpenGL->m_renderData.discardMode                   = 0;
        g_pHyprOpenGL->m_renderData.discardOpacity                = 0;
        g_pHyprOpenGL->m_renderData.useNearestNeighbor            = false;
        g_pHyprOpenGL->popMonitorTransformEnabled();
        g_pHyprOpenGL->m_renderData.currentWindow.reset();
        g_pHyprOpenGL->m_renderData.surface.reset();
        g_pHyprOpenGL->m_renderData.currentLS.reset();
    }};

    if (!m_data.texture)
        return;

    const auto& TEXTURE = m_data.texture;

    // this is bad, probably has been logged elsewhere. Means the texture failed
    // uploading to the GPU.
    if (!TEXTURE->ok())
        return;

    const auto INTERACTIVERESIZEINPROGRESS = m_data.pWindow && g_layoutManager->dragController()->target() && g_layoutManager->dragController()->mode() == MBIND_RESIZE;
    TRACY_GPU_ZONE("RenderSurface");

    auto        PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface);

    const float ALPHA         = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier : 1.F);
    const float OVERALL_ALPHA = PSURFACE ? PSURFACE->m_overallOpacity : 1.F;
    const bool  BLUR          = m_data.blur && (!TEXTURE->m_opaque || ALPHA < 1.F || OVERALL_ALPHA < 1.F);

    auto        windowBox = element->getTexBox();

    const auto  PROJSIZEUNSCALED = windowBox.size();

    windowBox.scale(m_data.pMonitor->m_scale);
    windowBox.round();

    if (windowBox.width <= 1 || windowBox.height <= 1) {
        element->discard();
        return;
    }

    const bool MISALIGNEDFSV1 = std::floor(m_data.pMonitor->m_scale) != m_data.pMonitor->m_scale /* Fractional */ && m_data.surface->m_current.scale == 1 /* fs protocol */ &&
        windowBox.size() != m_data.surface->m_current.bufferSize /* misaligned */ && DELTALESSTHAN(windowBox.width, m_data.surface->m_current.bufferSize.x, 3) &&
        DELTALESSTHAN(windowBox.height, m_data.surface->m_current.bufferSize.y, 3) /* off by one-or-two */ &&
        (!m_data.pWindow || (!m_data.pWindow->m_realSize->isBeingAnimated() && !INTERACTIVERESIZEINPROGRESS)) /* not window or not animated/resizing */ &&
        (!m_data.pLS || (!m_data.pLS->m_realSize->isBeingAnimated())); /* not LS or not animated */

    g_pHyprRenderer->calculateUVForSurface(m_data.pWindow, m_data.surface, m_data.pMonitor->m_self.lock(), m_data.mainSurface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);

    auto cancelRender                      = false;
    g_pHyprOpenGL->m_renderData.clipRegion = element->visibleRegion(cancelRender);
    if (cancelRender)
        return;

    // check for fractional scale surfaces misaligning the buffer size
    // in those cases it's better to just force nearest neighbor
    // as long as the window is not animated. During those it'd look weird.
    // UV will fixup it as well
    if (MISALIGNEDFSV1)
        g_pHyprOpenGL->m_renderData.useNearestNeighbor = true;

    float rounding      = m_data.rounding;
    float roundingPower = m_data.roundingPower;

    rounding -= 1; // to fix a border issue

    if (m_data.dontRound) {
        rounding      = 0;
        roundingPower = 2.0f;
    }

    const bool WINDOWOPAQUE    = m_data.pWindow && m_data.pWindow->wlSurface()->resource() == m_data.surface ? m_data.pWindow->opaque() : false;
    const bool CANDISABLEBLEND = ALPHA >= 1.f && OVERALL_ALPHA >= 1.f && rounding == 0 && WINDOWOPAQUE;

    if (CANDISABLEBLEND)
        g_pHyprOpenGL->blend(false);
    else
        g_pHyprOpenGL->blend(true);

    // FIXME: This is wrong and will bug the blur out as shit if the first surface
    // is a subsurface that does NOT cover the entire frame. In such cases, we probably should fall back
    // to what we do for misaligned surfaces (blur the entire thing and then render shit without blur)
    if (m_data.surfaceCounter == 0 && !m_data.popup) {
        if (BLUR)
            g_pHyprOpenGL->renderTexture(TEXTURE, windowBox,
                                         {
                                             .surface               = m_data.surface,
                                             .a                     = ALPHA,
                                             .blur                  = true,
                                             .blurA                 = m_data.fadeAlpha,
                                             .overallA              = OVERALL_ALPHA,
                                             .round                 = rounding,
                                             .roundingPower         = roundingPower,
                                             .allowCustomUV         = true,
                                             .blockBlurOptimization = m_data.blockBlurOptimization,
                                         });
        else
            g_pHyprOpenGL->renderTexture(TEXTURE, windowBox,
                                         {.a = ALPHA * OVERALL_ALPHA, .round = rounding, .roundingPower = roundingPower, .discardActive = false, .allowCustomUV = true});
    } else {
        if (BLUR && m_data.popup)
            g_pHyprOpenGL->renderTexture(TEXTURE, windowBox,
                                         {
                                             .surface               = m_data.surface,
                                             .a                     = ALPHA,
                                             .blur                  = true,
                                             .blurA                 = m_data.fadeAlpha,
                                             .overallA              = OVERALL_ALPHA,
                                             .round                 = rounding,
                                             .roundingPower         = roundingPower,
                                             .allowCustomUV         = true,
                                             .blockBlurOptimization = true,
                                         });
        else
            g_pHyprOpenGL->renderTexture(TEXTURE, windowBox,
                                         {.a = ALPHA * OVERALL_ALPHA, .round = rounding, .roundingPower = roundingPower, .discardActive = false, .allowCustomUV = true});
    }

    g_pHyprOpenGL->blend(true);
};

void CHyprGLRenderer::draw(CTexPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;
    g_pHyprOpenGL->pushMonitorTransformEnabled(m_data.flipEndFrame);

    CScopeGuard x = {[m_data]() {
        //
        g_pHyprOpenGL->popMonitorTransformEnabled();
        g_pHyprOpenGL->m_renderData.clipBox = {};
        if (m_data.replaceProjection)
            g_pHyprOpenGL->m_renderData.monitorProjection = g_pHyprRenderer->m_renderData.pMonitor->m_projMatrix;
        if (m_data.ignoreAlpha.has_value())
            g_pHyprOpenGL->m_renderData.discardMode = 0;
    }};

    if (!m_data.clipBox.empty())
        g_pHyprOpenGL->m_renderData.clipBox = m_data.clipBox;

    if (m_data.replaceProjection)
        g_pHyprOpenGL->m_renderData.monitorProjection = *m_data.replaceProjection;

    if (m_data.ignoreAlpha.has_value()) {
        g_pHyprOpenGL->m_renderData.discardMode    = DISCARD_ALPHA;
        g_pHyprOpenGL->m_renderData.discardOpacity = *m_data.ignoreAlpha;
    }

    if (m_data.blur) {
        g_pHyprOpenGL->renderTexture(m_data.tex, m_data.box,
                                     {
                                         .a                     = m_data.a,
                                         .blur                  = true,
                                         .blurA                 = m_data.blurA,
                                         .overallA              = 1.F,
                                         .round                 = m_data.round,
                                         .roundingPower         = m_data.roundingPower,
                                         .blockBlurOptimization = m_data.blockBlurOptimization.value_or(false),
                                     });
    } else {
        g_pHyprOpenGL->renderTexture(m_data.tex, m_data.box,
                                     {.damage = m_data.damage.empty() ? &damage : &m_data.damage, .a = m_data.a, .round = m_data.round, .roundingPower = m_data.roundingPower});
    }
};

void CHyprGLRenderer::draw(CTextureMatteElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;
    if (m_data.disableTransformAndModify) {
        g_pHyprOpenGL->pushMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTextureMatte(m_data.tex, m_data.box, m_data.fb);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->popMonitorTransformEnabled();
    } else
        g_pHyprOpenGL->renderTextureMatte(m_data.tex, m_data.box, m_data.fb);
};

SP<ITexture> CHyprGLRenderer::getBlurTexture(PHLMONITORREF pMonitor) {
    return g_pHyprOpenGL->m_monitorRenderResources[pMonitor].blurFB->getTexture();
}
