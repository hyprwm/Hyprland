#include "ElementRenderer.hpp"
#include "Renderer.hpp"
#include "../layout/LayoutManager.hpp"
#include "../desktop/view/Window.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Render;

void IElementRenderer::drawElement(WP<IPassElement> element, const CRegion& damage) {
    if (!element)
        return;

    switch (element->type()) {
        case EK_BORDER: draw(dynamicPointerCast<CBorderPassElement>(element), damage); break;
        case EK_CLEAR: draw(dynamicPointerCast<CClearPassElement>(element), damage); break;
        case EK_FRAMEBUFFER: draw(dynamicPointerCast<CFramebufferElement>(element), damage); break;
        case EK_PRE_BLUR: drawPreBlur(dynamicPointerCast<CPreBlurElement>(element), damage); break;
        case EK_RECT: drawRect(dynamicPointerCast<CRectPassElement>(element), damage); break;
        case EK_HINTS: drawHints(dynamicPointerCast<CRendererHintsPassElement>(element), damage); break;
        case EK_SHADOW: draw(dynamicPointerCast<CShadowPassElement>(element), damage); break;
        case EK_SURFACE: preDrawSurface(dynamicPointerCast<CSurfacePassElement>(element), damage); break;
        case EK_TEXTURE: drawTex(dynamicPointerCast<CTexPassElement>(element), damage); break;
        case EK_TEXTURE_MATTE: drawTexMatte(dynamicPointerCast<CTextureMatteElement>(element), damage); break;
        case EK_CUSTOM: drawCustom(element, damage); break;
        default: Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
    }
}

static std::optional<Vector2D> getSurfaceExpectedSize(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main) {
    const auto CAN_USE_WINDOW       = pWindow && main;
    const auto WINDOW_SIZE_MISALIGN = CAN_USE_WINDOW && pWindow->getReportedSize() != pWindow->wlSurface()->resource()->m_current.size;

    if (pSurface->m_current.viewport.hasDestination)
        return (pSurface->m_current.viewport.destination * pMonitor->m_scale).round();

    if (pSurface->m_current.viewport.hasSource)
        return (pSurface->m_current.viewport.source.size() * pMonitor->m_scale).round();

    if (WINDOW_SIZE_MISALIGN)
        return (pSurface->m_current.size * pMonitor->m_scale).round();

    if (CAN_USE_WINDOW)
        return (pWindow->getReportedSize() * pMonitor->m_scale).round();

    return std::nullopt;
}

void IElementRenderer::calculateUVForSurface(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main, const Vector2D& projSize,
                                             const Vector2D& projSizeUnscaled, bool fixMisalignedFSV1) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;

    if (!pWindow || !pWindow->m_isX11) {
        static auto PEXPANDEDGES = CConfigValue<Hyprlang::INT>("render:expand_undersized_textures");

        Vector2D    uvTL;
        Vector2D    uvBR = Vector2D(1, 1);

        if (pSurface->m_current.viewport.hasSource) {
            // we stretch it to dest. if no dest, to 1,1
            Vector2D const& bufferSize   = pSurface->m_current.bufferSize;
            auto const&     bufferSource = pSurface->m_current.viewport.source;

            // calculate UV for the basic src_box. Assume dest == size. Scale to dest later
            uvTL = Vector2D(bufferSource.x / bufferSize.x, bufferSource.y / bufferSize.y);
            uvBR = Vector2D((bufferSource.x + bufferSource.width) / bufferSize.x, (bufferSource.y + bufferSource.height) / bufferSize.y);

            if (uvBR.x < 0.01f || uvBR.y < 0.01f) {
                uvTL = Vector2D();
                uvBR = Vector2D(1, 1);
            }
        }

        if (projSize != Vector2D{} && fixMisalignedFSV1) {
            // instead of nearest_neighbor (we will repeat / skip)
            // just cut off / expand surface
            const Vector2D PIXELASUV   = Vector2D{1, 1} / pSurface->m_current.bufferSize;
            const auto&    BUFFER_SIZE = pSurface->m_current.bufferSize;

            // compute MISALIGN from the adjusted UV coordinates.
            const Vector2D MISALIGNMENT = (uvBR - uvTL) * BUFFER_SIZE - projSize;

            if (MISALIGNMENT != Vector2D{})
                uvBR -= MISALIGNMENT * PIXELASUV;
        } else {
            // if the surface is smaller than our viewport, extend its edges.
            // this will break if later on xdg geometry is hit, but we really try
            // to let the apps know to NOT add CSD. Also if source is there.
            // there is no way to fix this if that's the case
            const auto MONITOR_WL_SCALE = std::ceil(pMonitor->m_scale);
            const bool SCALE_UNAWARE    = pMonitor->m_scale != 1.f && (MONITOR_WL_SCALE == pSurface->m_current.scale || !pSurface->m_current.viewport.hasDestination);
            const auto EXPECTED_SIZE    = getSurfaceExpectedSize(pWindow, pSurface, pMonitor, main).value_or((projSize * pMonitor->m_scale).round());

            const auto RATIO = projSize / EXPECTED_SIZE;
            if (!SCALE_UNAWARE || MONITOR_WL_SCALE == 1) {
                if (*PEXPANDEDGES && !SCALE_UNAWARE && (RATIO.x > 1 || RATIO.y > 1)) {
                    const auto FIX = RATIO.clamp(Vector2D{1, 1}, Vector2D{1000000, 1000000});
                    uvBR           = uvBR * FIX;
                }

                // FIXME: probably do this for in anims on all views...
                const auto SHOULD_SKIP = !pWindow || pWindow->m_animatingIn;
                if (!SHOULD_SKIP && (RATIO.x < 1 || RATIO.y < 1)) {
                    const auto FIX = RATIO.clamp(Vector2D{0.0001, 0.0001}, Vector2D{1, 1});
                    uvBR           = uvBR * FIX;
                }
            }
        }

        m_renderData.primarySurfaceUVTopLeft     = uvTL;
        m_renderData.primarySurfaceUVBottomRight = uvBR;

        if (m_renderData.primarySurfaceUVTopLeft == Vector2D() && m_renderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }

        if (!main || !pWindow)
            return;

        // FIXME: this doesn't work. We always set MAXIMIZED anyways, so this doesn't need to work, but it's problematic.

        // CBox geom = pWindow->m_xdgSurface->m_current.geometry;

        // // Adjust UV based on the xdg_surface geometry
        // if (geom.x != 0 || geom.y != 0 || geom.w != 0 || geom.h != 0) {
        //     const auto XPERC = geom.x / pSurface->m_current.size.x;
        //     const auto YPERC = geom.y / pSurface->m_current.size.y;
        //     const auto WPERC = (geom.x + geom.w ? geom.w : pSurface->m_current.size.x) / pSurface->m_current.size.x;
        //     const auto HPERC = (geom.y + geom.h ? geom.h : pSurface->m_current.size.y) / pSurface->m_current.size.y;

        //     const auto TOADDTL = Vector2D(XPERC * (uvBR.x - uvTL.x), YPERC * (uvBR.y - uvTL.y));
        //     uvBR               = uvBR - Vector2D((1.0 - WPERC) * (uvBR.x - uvTL.x), (1.0 - HPERC) * (uvBR.y - uvTL.y));
        //     uvTL               = uvTL + TOADDTL;
        // }

        m_renderData.primarySurfaceUVTopLeft     = uvTL;
        m_renderData.primarySurfaceUVBottomRight = uvBR;

        if (m_renderData.primarySurfaceUVTopLeft == Vector2D() && m_renderData.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }
    } else {
        m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }
}

void IElementRenderer::drawRect(WP<CRectPassElement> element, const CRegion& damage) {
    auto& data         = element->m_data;
    auto& m_renderData = g_pHyprRenderer->m_renderData;

    if (data.box.w <= 0 || data.box.h <= 0)
        return;

    if (!data.clipBox.empty())
        m_renderData.clipBox = data.clipBox;

    data.modifiedBox = data.box;
    m_renderData.renderModif.applyToBox(data.modifiedBox);

    CBox transformedBox = data.box;
    transformedBox.transform(Math::wlTransformToHyprutils(Math::invertTransform(m_renderData.pMonitor->m_transform)), m_renderData.pMonitor->m_transformedSize.x,
                             m_renderData.pMonitor->m_transformedSize.y);

    data.TOPLEFT[0]  = sc<float>(transformedBox.x);
    data.TOPLEFT[1]  = sc<float>(transformedBox.y);
    data.FULLSIZE[0] = sc<float>(transformedBox.width);
    data.FULLSIZE[1] = sc<float>(transformedBox.height);

    data.drawRegion = data.color.a == 1.F || !data.blur ? damage : m_renderData.damage;

    if (m_renderData.clipBox.width != 0 && m_renderData.clipBox.height != 0) {
        CRegion damageClip{m_renderData.clipBox.x, m_renderData.clipBox.y, m_renderData.clipBox.width, m_renderData.clipBox.height};
        data.drawRegion = damageClip.intersect(data.drawRegion);
    }

    draw(element, damage);

    m_renderData.clipBox = {};
}

void IElementRenderer::drawHints(WP<CRendererHintsPassElement> element, const CRegion& damage) {
    const auto m_data = element->m_data;
    if (m_data.renderModif.has_value())
        g_pHyprRenderer->m_renderData.renderModif = *m_data.renderModif;
}

void IElementRenderer::drawPreBlur(WP<CPreBlurElement> element, const CRegion& damage) {
    TRACY_GPU_ZONE("RenderPreBlurForCurrentMonitor");
    auto&      m_renderData = g_pHyprRenderer->m_renderData;

    const auto SAVEDRENDERMODIF = m_renderData.renderModif;
    m_renderData.renderModif    = {}; // fix shit

    // make the fake dmg
    CRegion fakeDamage{0, 0, m_renderData.pMonitor->m_transformedSize.x, m_renderData.pMonitor->m_transformedSize.y};

    draw(element, fakeDamage);

    m_renderData.pMonitor->m_blurFBDirty        = false;
    m_renderData.pMonitor->m_blurFBShouldRender = false;

    m_renderData.renderModif = SAVEDRENDERMODIF;
}

void IElementRenderer::drawSurface(WP<CSurfacePassElement> element, const CRegion& damage) {
    const auto                    m_data       = element->m_data;
    auto&                         m_renderData = g_pHyprRenderer->m_renderData;

    Hyprutils::Utils::CScopeGuard x = {[]() {
        g_pHyprRenderer->m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprRenderer->m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
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

    calculateUVForSurface(m_data.pWindow, m_data.surface, m_data.pMonitor->m_self.lock(), m_data.mainSurface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);

    auto cancelRender = false;
    auto clipRegion   = element->visibleRegion(cancelRender);
    if (cancelRender)
        return;

    // check for fractional scale surfaces misaligning the buffer size
    // in those cases it's better to just force nearest neighbor
    // as long as the window is not animated. During those it'd look weird.
    // UV will fixup it as well
    if (MISALIGNEDFSV1)
        m_renderData.useNearestNeighbor = true;

    float rounding      = m_data.rounding;
    float roundingPower = m_data.roundingPower;

    rounding -= 1; // to fix a border issue

    if (m_data.dontRound) {
        rounding      = 0;
        roundingPower = 2.0f;
    }

    const bool WINDOWOPAQUE    = m_data.pWindow && m_data.pWindow->wlSurface()->resource() == m_data.surface ? m_data.pWindow->opaque() : false;
    const bool CANDISABLEBLEND = ALPHA >= 1.f && OVERALL_ALPHA >= 1.f && rounding <= 0 && WINDOWOPAQUE;

    if (CANDISABLEBLEND)
        g_pHyprRenderer->blend(false);
    else
        g_pHyprRenderer->blend(true);

    // FIXME: This is wrong and will bug the blur out as shit if the first surface
    // is a subsurface that does NOT cover the entire frame. In such cases, we probably should fall back
    // to what we do for misaligned surfaces (blur the entire thing and then render shit without blur)
    if (m_data.surfaceCounter == 0 && !m_data.popup) {
        if (BLUR)
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex                   = TEXTURE,
                            .box                   = windowBox,
                            .a                     = ALPHA,
                            .blurA                 = m_data.fadeAlpha,
                            .overallA              = OVERALL_ALPHA,
                            .round                 = rounding,
                            .roundingPower         = roundingPower,
                            .blur                  = true,
                            .blockBlurOptimization = m_data.blockBlurOptimization,
                            .allowCustomUV         = true,
                            .surface               = m_data.surface,
                            .discardMode           = m_data.discardMode,
                            .discardOpacity        = m_data.discardOpacity,
                            .clipRegion            = clipRegion,
                            .currentLS             = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(windowBox));
        else
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex            = TEXTURE,
                            .box            = windowBox,
                            .a              = ALPHA * OVERALL_ALPHA,
                            .round          = rounding,
                            .roundingPower  = roundingPower,
                            .discardActive  = false,
                            .allowCustomUV  = true,
                            .surface        = m_data.surface,
                            .discardMode    = m_data.discardMode,
                            .discardOpacity = m_data.discardOpacity,
                            .clipRegion     = clipRegion,
                            .currentLS      = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(windowBox));
    } else {
        if (BLUR && m_data.popup)
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex                   = TEXTURE,
                            .box                   = windowBox,
                            .a                     = ALPHA,
                            .blurA                 = m_data.fadeAlpha,
                            .overallA              = OVERALL_ALPHA,
                            .round                 = rounding,
                            .roundingPower         = roundingPower,
                            .blur                  = true,
                            .blockBlurOptimization = true,
                            .allowCustomUV         = true,
                            .surface               = m_data.surface,
                            .discardMode           = m_data.discardMode,
                            .discardOpacity        = m_data.discardOpacity,
                            .clipRegion            = clipRegion,
                            .currentLS             = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(windowBox));
        else
            drawElement(makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex            = TEXTURE,
                            .box            = windowBox,
                            .a              = ALPHA * OVERALL_ALPHA,
                            .round          = rounding,
                            .roundingPower  = roundingPower,
                            .discardActive  = false,
                            .allowCustomUV  = true,
                            .surface        = m_data.surface,
                            .discardMode    = m_data.discardMode,
                            .discardOpacity = m_data.discardOpacity,
                            .clipRegion     = clipRegion,
                            .currentLS      = m_data.pLS,
                        }),
                        m_renderData.damage.copy().intersect(windowBox));
    }

    g_pHyprRenderer->blend(true);
};

void IElementRenderer::preDrawSurface(WP<CSurfacePassElement> element, const CRegion& damage) {
    auto& m_renderData              = g_pHyprRenderer->m_renderData;
    m_renderData.clipBox            = element->m_data.clipBox;
    m_renderData.useNearestNeighbor = element->m_data.useNearestNeighbor;
    g_pHyprRenderer->pushMonitorTransformEnabled(element->m_data.flipEndFrame);
    m_renderData.currentWindow = element->m_data.pWindow;

    drawSurface(element, damage);

    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback)
        element->m_data.surface->presentFeedback(element->m_data.when, element->m_data.pMonitor->m_self.lock());

    // add async (dmabuf) buffers to usedBuffers so we can handle release later
    // sync (shm) buffers will be released in commitState, so no need to track them here
    if (element->m_data.surface->m_current.buffer && !element->m_data.surface->m_current.buffer->isSynchronous())
        g_pHyprRenderer->m_usedAsyncBuffers.emplace_back(element->m_data.surface->m_current.buffer);

    m_renderData.clipBox            = {};
    m_renderData.useNearestNeighbor = false;
    g_pHyprRenderer->popMonitorTransformEnabled();
    m_renderData.currentWindow.reset();
}

void IElementRenderer::drawTex(WP<CTexPassElement> element, const CRegion& damage) {
    auto& m_renderData = g_pHyprRenderer->m_renderData;
    if (!element->m_data.clipBox.empty())
        m_renderData.clipBox = element->m_data.clipBox;

    g_pHyprRenderer->pushMonitorTransformEnabled(element->m_data.flipEndFrame);
    if (element->m_data.useMirrorProjection)
        g_pHyprRenderer->setProjectionType(RPT_MIRROR);

    m_renderData.surface = element->m_data.surface;

    Hyprutils::Utils::CScopeGuard x = {[useMirrorProjection = element->m_data.useMirrorProjection]() {
        g_pHyprRenderer->popMonitorTransformEnabled();
        if (useMirrorProjection)
            g_pHyprRenderer->setProjectionType(RPT_MONITOR);
        g_pHyprRenderer->m_renderData.surface.reset();
        g_pHyprRenderer->m_renderData.clipBox = {};
    }};

    if (element->m_data.blur) {
        // make a damage region for this window
        CRegion texDamage{m_renderData.damage};
        texDamage.intersect(element->m_data.box.x, element->m_data.box.y, element->m_data.box.width, element->m_data.box.height);

        // While renderTextureInternalWithDamage will clip the blur as well,
        // clipping texDamage here allows blur generation to be optimized.
        if (!element->m_data.clipRegion.empty())
            texDamage.intersect(element->m_data.clipRegion);

        if (texDamage.empty())
            return;

        m_renderData.renderModif.applyToRegion(texDamage);

        element->m_data.damage = texDamage;

        // amazing hack: the surface has an opaque region!
        const auto& surface = element->m_data.surface;
        const auto& box     = element->m_data.box;
        CRegion     inverseOpaque;
        if (element->m_data.a >= 1.f && surface && std::round(surface->m_current.size.x * m_renderData.pMonitor->m_scale) == box.w &&
            std::round(surface->m_current.size.y * m_renderData.pMonitor->m_scale) == box.h) {
            pixman_box32_t surfbox = {0, 0, surface->m_current.size.x * surface->m_current.scale, surface->m_current.size.y * surface->m_current.scale};
            inverseOpaque          = surface->m_current.opaque;
            inverseOpaque.invert(&surfbox).intersect(0, 0, surface->m_current.size.x * surface->m_current.scale, surface->m_current.size.y * surface->m_current.scale);

            if (inverseOpaque.empty()) {
                element->m_data.blur = false;
                draw(element, damage);
                return;
            }
        } else
            inverseOpaque = {0, 0, element->m_data.box.width, element->m_data.box.height};

        inverseOpaque.scale(m_renderData.pMonitor->m_scale);
        element->m_data.blockBlurOptimization = element->m_data.blockBlurOptimization.value_or(false) ||
            !g_pHyprRenderer->shouldUseNewBlurOptimizations(element->m_data.currentLS.lock(), m_renderData.currentWindow.lock());

        //   vvv TODO: layered blur fbs?
        if (element->m_data.blockBlurOptimization.value_or(false)) {
            inverseOpaque.translate(box.pos());
            m_renderData.renderModif.applyToRegion(inverseOpaque);
            inverseOpaque.intersect(element->m_data.damage);
            element->m_data.blurredBG = g_pHyprRenderer->blurMainFramebuffer(element->m_data.a, &inverseOpaque);
        } else
            element->m_data.blurredBG = m_renderData.pMonitor->resources()->m_blurFB->getTexture();

        draw(element, damage);
    } else
        draw(element, damage);
}

void IElementRenderer::drawTexMatte(WP<CTextureMatteElement> element, const CRegion& damage) {
    if (g_pHyprRenderer->m_renderData.damage.empty())
        return;

    const auto m_data = element->m_data;
    if (m_data.disableTransformAndModify) {
        g_pHyprRenderer->pushMonitorTransformEnabled(true);
        g_pHyprRenderer->m_renderData.renderModif.enabled = false;
        draw(element, damage);
        g_pHyprRenderer->m_renderData.renderModif.enabled = true;
        g_pHyprRenderer->popMonitorTransformEnabled();
    } else
        draw(element, damage);
}

void IElementRenderer::drawCustom(WP<IPassElement> element, const CRegion& damage) {
    const auto& elements = element->draw();
    for (const auto& el : elements) {
        drawElement(el, damage);
    }
}
