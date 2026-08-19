#include "GLElementRenderer.hpp"
#include "../pass/BoxShadowPassElement.hpp"
#include "../Renderer.hpp"
#include "../decorations/CHyprDropShadowDecoration.hpp"
#include "../OpenGL.hpp"
#include "../decorations/CHyprInnerGlowDecoration.hpp"
#include <cstdint>

using namespace Render::GL;

void CGLElementRenderer::draw(CRenderingContext& context, WP<CBorderPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;
    if (m_data.hasGrad2)
        g_pHyprOpenGL->renderBorder(
            context, m_data.box, m_data.grad1, m_data.grad2, m_data.lerp,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
    else
        g_pHyprOpenGL->renderBorder(
            context, m_data.box, m_data.grad1,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CClearPassElement> element, const CRegion& damage) {
    const auto& color = element->m_data.color;
    RASSERT(context.sceneMonitor, "Tried to render without begin()!");

    TRACY_GPU_ZONE("RenderClear");
    const std::array<GLfloat, 4> c = {sc<GLfloat>(color.r), sc<GLfloat>(color.g), sc<GLfloat>(color.b), sc<GLfloat>(color.a)};

    if (!context.damage.empty()) {
        context.damage.forEachRect([&c, &context](const auto& RECT) {
            g_pHyprOpenGL->scissor(context, &RECT, context.transformDamage);
            glClearBufferfv(GL_COLOR, 0, c.data());
        });

        g_pHyprOpenGL->scissor(context, nullptr);
    } else
        glClearBufferfv(GL_COLOR, 0, c.data());
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CFramebufferElement> element, const CRegion& damage) {
    Log::logger->log(Log::ERR, "Deprecated CFramebufferElement. Use context and CTexPassElement instead");
    // const auto       m_data = element->m_data;
    // SP<IFramebuffer> fb     = nullptr;

    // if (m_data.main) {
    //     switch (m_data.framebufferID) {
    //         case FB_MONITOR_RENDER_MAIN: fb = context.mainFB; break;
    //         case FB_MONITOR_RENDER_CURRENT: fb = context.currentFB; break;
    //         case FB_MONITOR_RENDER_OUT: fb = context.outFB; break;
    //         default: fb = nullptr;
    //     }

    //     if (!fb) {
    //         Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: main but null");
    //         return;
    //     }

    // } else {
    //     switch (m_data.framebufferID) {
    //         case FB_MONITOR_RENDER_EXTRA_OFFLOAD: fb = context.sceneMonitor->m_offloadFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MIRROR: fb = context.sceneMonitor->m_mirrorFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MIRROR_SWAP: fb = context.sceneMonitor->m_mirrorSwapFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_OFF_MAIN: fb = context.sceneMonitor->m_offMainFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_MONITOR_MIRROR: fb = context.sceneMonitor->m_monitorMirrorFB; break;
    //         case FB_MONITOR_RENDER_EXTRA_BLUR: fb = context.sceneMonitor->m_blurFB; break;
    //         default: fb = nullptr;
    //     }

    //     if (!fb) {
    //         Log::logger->log(Log::ERR, "BUG THIS: CFramebufferElement::draw: not main but null");
    //         return;
    //     }
    // }

    // g_pHyprRenderer->bindFB(fb);
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CPreBlurElement> element, const CRegion& damage) {
    auto dmg = damage;
    g_pHyprRenderer->preBlurForCurrentMonitor(context, dmg);
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CRectPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;

    if (m_data.color.a == 1.F || !m_data.blur)
        g_pHyprOpenGL->renderRect(context, m_data.box, m_data.color, {.damage = &damage, .round = m_data.round, .roundingPower = m_data.roundingPower});
    else
        g_pHyprOpenGL->renderRect(context, m_data.box, m_data.color,
                                  {.round          = m_data.round,
                                   .roundingPower  = m_data.roundingPower,
                                   .blur           = true,
                                   .blurA          = m_data.blurA,
                                   .xray           = m_data.xray,
                                   .blurPatternBox = m_data.blurPatternBox,
                                   .blurOwner      = m_data.blurOwner});
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CShadowPassElement> element, const CRegion& damage) {
    if (const auto BOX_SHADOW = dynamicPointerCast<CBoxShadowPassElement>(element)) {
        const auto&       DATA = BOX_SHADOW->m_boxData;
        CRenderingContext child{context, context.renderPass()};
        child.clipBox = DATA.clipBox;
        child.currentWindow.reset();
        child.damage = damage;

        g_pHyprOpenGL->renderRoundedShadow(child, DATA.box, DATA.round, DATA.roundingPower, DATA.range, Config::CGradientValueData{DATA.color}, DATA.a, DATA.cutoutBox, DATA.round);
        return;
    }

    const auto& m_data = element->m_data;
    const auto  DECO   = m_data.deco.lock();
    if (!DECO)
        return;
    DECO->render(context, context.sceneMonitor.lock(), m_data.a);
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CInnerGlowPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;
    const auto  DECO   = m_data.deco.lock();
    if (!DECO)
        return;
    DECO->render(context, context.sceneMonitor.lock(), m_data.a);
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CTexPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;

    g_pHyprOpenGL->renderTexture( //
        context, m_data.tex, m_data.box,
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

            .primarySurfaceUVTopLeft     = context.primarySurfaceUVTopLeft,
            .primarySurfaceUVBottomRight = context.primarySurfaceUVBottomRight,
            .motionBlur                  = m_data.motionBlur,
        });
};

void CGLElementRenderer::draw(CRenderingContext& context, WP<CTextureMatteElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;

    g_pHyprOpenGL->renderTextureMatte(context, m_data.tex, m_data.box, m_data.fb);
};
