#include "GLElementRenderer.hpp"
#include "../Renderer.hpp"
#include "../decorations/CHyprDropShadowDecoration.hpp"
#include "../OpenGL.hpp"
#include "../decorations/CHyprInnerGlowDecoration.hpp"
#include <cstdint>

using namespace Render::GL;

void CGLElementRenderer::draw(WP<CBorderPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;
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
    RASSERT(g_pHyprRenderer->context().m_data.pMonitor, "Tried to render without begin()!");

    TRACY_GPU_ZONE("RenderClear");
    const std::array<GLfloat, 4> c = {sc<GLfloat>(color.r), sc<GLfloat>(color.g), sc<GLfloat>(color.b), sc<GLfloat>(color.a)};

    if (!g_pHyprRenderer->context().m_data.damage.empty()) {
        g_pHyprRenderer->context().m_data.damage.forEachRect([&c](const auto& RECT) {
            g_pHyprOpenGL->scissor(&RECT, g_pHyprRenderer->context().m_data.transformDamage);
            glClearBufferfv(GL_COLOR, 0, c.data());
        });

        g_pHyprOpenGL->scissor(nullptr);
    } else
        glClearBufferfv(GL_COLOR, 0, c.data());
};

void CGLElementRenderer::draw(WP<CFramebufferElement> element, const CRegion& damage) {
    LOG(Log::ERR, "Deprecated CFramebufferElement. Use g_pHyprRenderer->context().m_data and CTexPassElement instead");
    // const auto       m_data = element->m_data;
    // SP<IFramebuffer> fb     = nullptr;

    // if (m_data.main) {
    //     switch (m_data.framebufferID) {
    //         case FB_MONITOR_RENDER_MAIN: fb = g_pHyprRenderer->context().m_data.mainFB; break;
    //         case FB_MONITOR_RENDER_CURRENT: fb = g_pHyprRenderer->context().m_data.currentFB; break;
    //         case FB_MONITOR_RENDER_OUT: fb = g_pHyprRenderer->context().m_data.outFB; break;
    //         default: fb = nullptr;
    //     }

    //     if (!fb) {
    //         LOG(Log::ERR, "BUG THIS: CFramebufferElement::draw: main but null");
    //         return;
    //     }

    // } else {
    //     switch (m_data.framebufferID) {
    //         case FB_MONITOR_RENDER_EXTRA_OFFLOAD: fb = g_pHyprRenderer->context().m_data.pMonitor->m_offloadFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MIRROR: fb = g_pHyprRenderer->context().m_data.pMonitor->m_mirrorFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP: fb = g_pHyprRenderer->context().m_data.pMonitor->m_mirrorSwapFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_OFF_MAIN: fb = g_pHyprRenderer->context().m_data.pMonitor->m_offMainFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR: fb = g_pHyprRenderer->context().m_data.pMonitor->m_monitorMirrorFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_BLUR: fb = g_pHyprRenderer->context().m_data.pMonitor->m_blurFB; break;
    //         default: fb = nullptr;
    //     }

    //     if (!fb) {
    //         LOG(Log::ERR, "BUG THIS: CFramebufferElement::draw: not main but null");
    //         return;
    //     }
    // }

    // g_pHyprRenderer->bindFB(fb);
};

void CGLElementRenderer::draw(WP<CPreBlurElement> element, const CRegion& damage) {
    auto dmg = damage;
    g_pHyprRenderer->preBlurForCurrentMonitor(dmg);
};

void CGLElementRenderer::draw(WP<CRectPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;

    if (m_data.color.a == 1.F || !m_data.blur)
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color,
                                  {.damage = &damage, .round = m_data.round, .roundingPower = m_data.roundingPower, .workspacePresentation = m_data.workspacePresentation});
    else
        g_pHyprOpenGL->renderRect(m_data.box, m_data.color,
                                  {.round                 = m_data.round,
                                   .roundingPower         = m_data.roundingPower,
                                   .blur                  = true,
                                   .blurA                 = m_data.blurA,
                                   .xray                  = m_data.xray,
                                   .blurPatternBox        = m_data.blurPatternBox,
                                   .blurOwner             = m_data.blurOwner,
                                   .workspacePresentation = m_data.workspacePresentation});
};

void CGLElementRenderer::draw(WP<CShadowPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;
    const auto  DECO   = m_data.deco.lock();
    if (!DECO)
        return;
    DECO->render(g_pHyprRenderer->context().m_data.pMonitor.lock(), m_data.a, m_data.presentation);
};

void CGLElementRenderer::draw(WP<CInnerGlowPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;
    const auto  DECO   = m_data.deco.lock();
    if (!DECO)
        return;
    DECO->render(g_pHyprRenderer->context().m_data.pMonitor.lock(), m_data.a, m_data.presentation);
};

void CGLElementRenderer::draw(WP<CTexPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;

    g_pHyprOpenGL->renderTexture( //
        m_data.tex, m_data.box,
        {
            // blur settings for m_data.blur == true
            .blur                  = m_data.blur,
            .forceBlurBlend        = m_data.forceBlurBlend,
            .blurA                 = m_data.blurA,
            .overallA              = m_data.overallA,
            .blockBlurOptimization = m_data.blockBlurOptimization.value_or(false),
            .blurredBG             = m_data.blurredBG,
            .blurAlphaMatte        = m_data.blurAlphaMatte,

            // common settings
            .damage         = m_data.damage.empty() ? &damage : &m_data.damage,
            .surface        = m_data.surface,
            .a              = m_data.a,
            .round          = m_data.round,
            .roundingPower  = m_data.roundingPower,
            .discardActive  = m_data.discardActive,
            .allowCustomUV  = m_data.allowCustomUV,
            .wrapX          = m_data.wrapX,
            .wrapY          = m_data.wrapY,
            .cmBackToSRGB   = m_data.cmBackToSRGB,
            .discardMode    = m_data.ignoreAlpha.has_value() ? sc<uint32_t>(DISCARD_ALPHA) : m_data.discardMode,
            .discardOpacity = m_data.ignoreAlpha.has_value() ? *m_data.ignoreAlpha : m_data.discardOpacity,
            .clipRegion     = m_data.clipRegion,
            .currentLS      = m_data.currentLS,

            .primarySurfaceUVTopLeft     = g_pHyprRenderer->context().m_data.primarySurfaceUVTopLeft,
            .primarySurfaceUVBottomRight = g_pHyprRenderer->context().m_data.primarySurfaceUVBottomRight,
            .motionBlur                  = m_data.motionBlur,
        });
};

void CGLElementRenderer::draw(WP<CTextureMatteElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;

    g_pHyprOpenGL->renderTextureMatte(m_data.tex, m_data.box, m_data.fb);
};
