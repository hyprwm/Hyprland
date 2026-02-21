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
    fb->bind();
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

    g_pHyprRenderer->m_renderData.damage = m_renderPass.render(g_pHyprRenderer->m_renderData.damage);

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
        g_pHyprRenderer->m_renderData.mouseZoomFactor   = 1.f;
        g_pHyprRenderer->m_renderData.mouseZoomUseMouse = true;
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

void CHyprGLRenderer::renderOffToMain(IFramebuffer* off) {
    g_pHyprOpenGL->renderOffToMain(off);
}

SP<IRenderbuffer> CHyprGLRenderer::getOrCreateRenderbufferInternal(SP<Aquamarine::IBuffer> buffer, uint32_t fmt) {
    g_pHyprOpenGL->makeEGLCurrent();
    return makeShared<CGLRenderbuffer>(buffer, fmt);
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

    tex->allocate({width, height});

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
    }

    glTexImage2D(GL_TEXTURE_2D, 0, glIFormat, tex->m_size.x, tex->m_size.y, 0, glFormat, glType, DATA);

    return tex;
}

bool CHyprGLRenderer::explicitSyncSupported() {
    return g_pHyprOpenGL->explicitSyncSupported();
}

std::vector<SDRMFormat> CHyprGLRenderer::getDRMFormats() {
    return g_pHyprOpenGL->getDRMFormats();
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

void CHyprGLRenderer::drawShadow(const CBox& box, int round, float roundingPower, int range, CHyprColor color, float a) {
    g_pHyprOpenGL->renderRoundedShadow(box, round, roundingPower, range, color, a);
}

SP<ITexture> CHyprGLRenderer::blurFramebuffer(SP<IFramebuffer> source, float a, CRegion* originalDamage) {
    auto src = GLFB(source);
    return g_pHyprOpenGL->blurFramebufferWithDamage(a, originalDamage, *src)->getTexture();
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
            case FB_MONITOR_RENDER_MAIN: fb = g_pHyprRenderer->m_renderData.mainFB; break;
            case FB_MONITOR_RENDER_CURRENT: fb = g_pHyprRenderer->m_renderData.currentFB; break;
            case FB_MONITOR_RENDER_OUT: fb = g_pHyprRenderer->m_renderData.outFB; break;
            default: fb = nullptr;
        }

        if (!fb) {
            Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: main but null");
            return;
        }

    } else {
        switch (m_data.framebufferID) {
            case FB_MONITOR_RENDER_EXTRA_OFFLOAD: fb = g_pHyprRenderer->m_renderData.pMonitor->m_offloadFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR: fb = m_renderData.pMonitor->m_mirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP: fb = m_renderData.pMonitor->m_mirrorSwapFB; break;
            case FB_MONITOR_RENDER_EXTRA_OFF_MAIN: fb = g_pHyprRenderer->m_renderData.pMonitor->m_offMainFB; break;
            case FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR: fb = g_pHyprRenderer->m_renderData.pMonitor->m_monitorMirrorFB; break;
            case FB_MONITOR_RENDER_EXTRA_BLUR: fb = g_pHyprRenderer->m_renderData.pMonitor->m_blurFB; break;
            default: fb = nullptr;
        }

        if (!fb) {
            Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: not main but null");
            return;
        }
    }

    fb->bind();
};

void CHyprGLRenderer::draw(CPreBlurElement* element, const CRegion& damage) {
    auto dmg = damage;
    g_pHyprRenderer->preBlurForCurrentMonitor(&dmg);
};

void CHyprGLRenderer::draw(CRectPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;

    if (m_data.color.a == 1.F || !m_data.blur)
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color, {.damage = &damage, .round = m_data.round, .roundingPower = m_data.roundingPower});
    else
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color,
                                  {.round = m_data.round, .roundingPower = m_data.roundingPower, .blur = true, .blurA = m_data.blurA, .xray = m_data.xray});
};

void CHyprGLRenderer::draw(CRendererHintsPassElement* element, const CRegion& damage) {};

void CHyprGLRenderer::draw(CShadowPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;
    m_data.deco->render(g_pHyprRenderer->m_renderData.pMonitor.lock(), m_data.a);
};

void CHyprGLRenderer::draw(CTexPassElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;
    g_pHyprRenderer->pushMonitorTransformEnabled(m_data.flipEndFrame);

    g_pHyprOpenGL->m_renderData.surface = m_data.surface;

    CScopeGuard x = {[m_data]() {
        //
        g_pHyprRenderer->popMonitorTransformEnabled();
        g_pHyprOpenGL->m_renderData.surface.reset();
        if (m_data.useMirrorProjection)
            g_pHyprRenderer->setProjectionType(RPT_MONITOR);
    }};

    if (m_data.useMirrorProjection)
        g_pHyprRenderer->setProjectionType(RPT_MIRROR);

    auto discardOpacity = m_data.ignoreAlpha.has_value() ? *m_data.ignoreAlpha : m_data.discardOpacity;
    auto discardMode    = m_data.ignoreAlpha.has_value() ? DISCARD_ALPHA : m_data.discardMode;

    if (m_data.blur) {
        g_pHyprOpenGL->renderTexture(m_data.tex, m_data.box,
                                     {
                                         .surface               = m_data.surface,
                                         .a                     = m_data.a,
                                         .blur                  = true,
                                         .blurA                 = m_data.blurA,
                                         .overallA              = m_data.overallA,
                                         .round                 = m_data.round,
                                         .roundingPower         = m_data.roundingPower,
                                         .discardActive         = m_data.discardActive,
                                         .allowCustomUV         = m_data.allowCustomUV,
                                         .blockBlurOptimization = m_data.blockBlurOptimization.value_or(false),
                                         .cmBackToSRGB          = m_data.cmBackToSRGB,
                                         .cmBackToSRGBSource    = m_data.cmBackToSRGBSource,
                                         .discardMode           = discardMode,
                                         .discardOpacity        = discardOpacity,
                                         .clipRegion            = m_data.clipRegion,
                                         .currentLS             = m_data.currentLS,
                                     });
    } else {
        g_pHyprOpenGL->renderTexture(m_data.tex, m_data.box,
                                     {
                                         .damage             = m_data.damage.empty() ? &damage : &m_data.damage,
                                         .surface            = m_data.surface,
                                         .a                  = m_data.a,
                                         .round              = m_data.round,
                                         .roundingPower      = m_data.roundingPower,
                                         .discardActive      = m_data.discardActive,
                                         .allowCustomUV      = m_data.allowCustomUV,
                                         .cmBackToSRGB       = m_data.cmBackToSRGB,
                                         .cmBackToSRGBSource = m_data.cmBackToSRGBSource,
                                         .discardMode        = discardMode,
                                         .discardOpacity     = discardOpacity,
                                         .clipRegion         = m_data.clipRegion,
                                         .currentLS          = m_data.currentLS,
                                     });
    }
};

void CHyprGLRenderer::draw(CTextureMatteElement* element, const CRegion& damage) {
    const auto m_data = element->m_data;

    g_pHyprOpenGL->renderTextureMatte(m_data.tex, m_data.box, m_data.fb);
};

SP<ITexture> CHyprGLRenderer::getBlurTexture(PHLMONITORREF pMonitor) {
    return pMonitor->m_blurFB->getTexture();
}

void CHyprGLRenderer::unsetEGL() {
    if (!g_pHyprOpenGL)
        return;

    eglMakeCurrent(g_pHyprOpenGL->m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}
