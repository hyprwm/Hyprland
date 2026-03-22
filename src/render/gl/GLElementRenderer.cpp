#include "GLElementRenderer.hpp"
#include "../Renderer.hpp"
#include "../decorations/CHyprDropShadowDecoration.hpp"
#include "../OpenGL.hpp"
#include <cstdint>

using namespace Render::GL;

void CGLElementRenderer::draw(WP<CBorderPassElement> element, const CRegion& damage) {
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

void CGLElementRenderer::draw(WP<CClearPassElement> element, const CRegion& damage) {
    const auto& color = element->m_data.color;
    RASSERT(g_pHyprRenderer->m_renderData.pMonitor, "Tried to render without begin()!");

    TRACY_GPU_ZONE("RenderClear");

    GLCALL(glClearColor(color.r, color.g, color.b, color.a));

    if (!g_pHyprRenderer->m_renderData.damage.empty()) {
        g_pHyprRenderer->m_renderData.damage.forEachRect([](const auto& RECT) {
            g_pHyprOpenGL->scissor(&RECT, g_pHyprRenderer->m_renderData.transformDamage);
            glClear(GL_COLOR_BUFFER_BIT);
        });
    }
};

void CGLElementRenderer::draw(WP<CFramebufferElement> element, const CRegion& damage) {
    Log::logger->log(Log::ERR, "Deprecated CFramebufferElement. Use g_pHyprRenderer->m_renderData and CTexPassElement instead");
    // const auto       m_data = element->m_data;
    // SP<IFramebuffer> fb     = nullptr;

    // if (m_data.main) {
    //     switch (m_data.framebufferID) {
    //         case FB_MONITOR_RENDER_MAIN: fb = g_pHyprRenderer->m_renderData.mainFB; break;
    //         case FB_MONITOR_RENDER_CURRENT: fb = g_pHyprRenderer->m_renderData.currentFB; break;
    //         case FB_MONITOR_RENDER_OUT: fb = g_pHyprRenderer->m_renderData.outFB; break;
    //         default: fb = nullptr;
    //     }

    //     if (!fb) {
    //         Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: main but null");
    //         return;
    //     }

    // } else {
    //     switch (m_data.framebufferID) {
    //         case FB_MONITOR_RENDER_EXTRA_OFFLOAD: fb = g_pHyprRenderer->m_renderData.pMonitor->m_offloadFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MIRROR: fb = g_pHyprRenderer->m_renderData.pMonitor->m_mirrorFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP: fb = g_pHyprRenderer->m_renderData.pMonitor->m_mirrorSwapFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_OFF_MAIN: fb = g_pHyprRenderer->m_renderData.pMonitor->m_offMainFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR: fb = g_pHyprRenderer->m_renderData.pMonitor->m_monitorMirrorFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_BLUR: fb = g_pHyprRenderer->m_renderData.pMonitor->m_blurFB; break;
    //         default: fb = nullptr;
    //     }

    //     if (!fb) {
    //         Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: not main but null");
    //         return;
    //     }
    // }

    // g_pHyprRenderer->bindFB(fb);
};

void CGLElementRenderer::draw(WP<CPreBlurElement> element, const CRegion& damage) {
    auto dmg = damage;
    g_pHyprRenderer->preBlurForCurrentMonitor(&dmg);
};

void CGLElementRenderer::draw(WP<CRectPassElement> element, const CRegion& damage) {
    const auto m_data = element->m_data;

    if (m_data.color.a == 1.F || !m_data.blur)
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color, {.damage = &damage, .round = m_data.round, .roundingPower = m_data.roundingPower});
    else
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color,
                                  {.round = m_data.round, .roundingPower = m_data.roundingPower, .blur = true, .blurA = m_data.blurA, .xray = m_data.xray});
};

void CGLElementRenderer::draw(WP<CShadowPassElement> element, const CRegion& damage) {
    const auto m_data = element->m_data;
    m_data.deco->render(g_pHyprRenderer->m_renderData.pMonitor.lock(), m_data.a);
};

void CGLElementRenderer::draw(WP<CTexPassElement> element, const CRegion& damage) {
    const auto m_data = element->m_data;

    g_pHyprOpenGL->renderTexture( //
        m_data.tex, m_data.box,
        {
            // blur settings for m_data.blur == true
            .blur                  = m_data.blur,
            .blurA                 = m_data.blurA,
            .overallA              = m_data.overallA,
            .blockBlurOptimization = m_data.blockBlurOptimization.value_or(false),
            .blurredBG             = m_data.blurredBG,

            // common settings
            .damage             = m_data.damage.empty() ? &damage : &m_data.damage,
            .surface            = m_data.surface,
            .a                  = m_data.a,
            .round              = m_data.round,
            .roundingPower      = m_data.roundingPower,
            .discardActive      = m_data.discardActive,
            .allowCustomUV      = m_data.allowCustomUV,
            .cmBackToSRGB       = m_data.cmBackToSRGB,
            .cmBackToSRGBSource = m_data.cmBackToSRGBSource,
            .discardMode        = m_data.ignoreAlpha.has_value() ? sc<uint32_t>(DISCARD_ALPHA) : m_data.discardMode,
            .discardOpacity     = m_data.ignoreAlpha.has_value() ? *m_data.ignoreAlpha : m_data.discardOpacity,
            .clipRegion         = m_data.clipRegion,
            .currentLS          = m_data.currentLS,

            .primarySurfaceUVTopLeft     = g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft,
            .primarySurfaceUVBottomRight = g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight,
        });
};

void CGLElementRenderer::draw(WP<CTextureMatteElement> element, const CRegion& damage) {
    const auto m_data = element->m_data;

    g_pHyprOpenGL->renderTextureMatte(m_data.tex, m_data.box, m_data.fb);
};