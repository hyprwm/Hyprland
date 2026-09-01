#include "ElementRenderer.hpp"
#include "Renderer.hpp"
#include "../layout/LayoutManager.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowEffectsController.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "render/pass/ClearPassElement.hpp"
#include "render/transformer/TransformerList.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include <algorithm>
#include <cstdint>

using namespace Render;

void IElementRenderer::drawElement(CRenderingContext& context, WP<IPassElement> element, const CRegion& damage) {
    if (!element)
        return;

    switch (element->type()) {
        case EK_BORDER: draw(context, dynamicPointerCast<CBorderPassElement>(element), damage); break;
        case EK_CLEAR: drawClear(context, dynamicPointerCast<CClearPassElement>(element), damage); break;
        case EK_FRAMEBUFFER: draw(context, dynamicPointerCast<CFramebufferElement>(element), damage); break;
        case EK_PRE_BLUR: drawPreBlur(context, dynamicPointerCast<CPreBlurElement>(element), damage); break;
        case EK_RECT: drawRect(context, dynamicPointerCast<CRectPassElement>(element), damage); break;
        case EK_HINTS: drawHints(context, dynamicPointerCast<CRendererHintsPassElement>(element), damage); break;
        case EK_SHADOW: draw(context, dynamicPointerCast<CShadowPassElement>(element), damage); break;
        case EK_INNER_GLOW: draw(context, dynamicPointerCast<CInnerGlowPassElement>(element), damage); break;
        case EK_SURFACE: preDrawSurface(context, dynamicPointerCast<CSurfacePassElement>(element), damage); break;
        case EK_TEXTURE: drawTex(context, dynamicPointerCast<CTexPassElement>(element), damage); break;
        case EK_TEXTURE_MATTE: drawTexMatte(context, dynamicPointerCast<CTextureMatteElement>(element), damage); break;
        case EK_TRANSFORMED_WINDOW: drawTransformedWindow(context, dynamicPointerCast<CTransformedWindowPassElement>(element), damage); break;
        case EK_BACKDROP_SCOPE: drawCustom(context, element, damage); break;
        case EK_CUSTOM: drawCustom(context, element, damage); break;
        default: Log::logger->log(Log::WARN, "Unimplimented draw for {}", element->passName());
    }
}

static std::optional<Vector2D> getSurfaceExpectedSize(PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main) {
    const auto CAN_USE_WINDOW       = pWindow && main;
    const auto WINDOW_SIZE_MISALIGN = CAN_USE_WINDOW && pWindow->backend().reportedSize() != pWindow->wlSurface()->resource()->m_current.size;

    if (pSurface->m_current.viewport.hasDestination)
        return (pSurface->m_current.viewport.destination * pMonitor->m_scale).round();

    if (pSurface->m_current.viewport.hasSource)
        return (pSurface->m_current.viewport.source.size() * pMonitor->m_scale).round();

    if (WINDOW_SIZE_MISALIGN)
        return (pSurface->m_current.size * pMonitor->m_scale).round();

    if (CAN_USE_WINDOW)
        return (pWindow->backend().reportedSize() * pMonitor->m_scale).round();

    return std::nullopt;
}

void IElementRenderer::calculateUVForSurface(CRenderingContext& context, PHLWINDOW pWindow, SP<CWLSurfaceResource> pSurface, PHLMONITOR pMonitor, bool main,
                                             const Vector2D& projSize, const Vector2D& projSizeUnscaled, bool fixMisalignedFSV1) {
    if (!pWindow || !pWindow->backend().isX11()) {
        static auto PEXPANDEDGES = CConfigValue<Hyprlang::INT>("render:expand_undersized_textures");

        Vector2D    uvTL;
        Vector2D    uvBR = Vector2D(1, 1);

        if (pSurface->m_current.viewport.hasSource) {
            // we stretch it to dest. if no dest, to 1,1
            Vector2D const& bufferSize   = pSurface->m_current.bufferSize;
            auto            bufferSource = pSurface->m_current.viewport.source;

            // Viewporter spec order: 1. transform, 2. scale, 3. viewport
            Vector2D const trc = pSurface->m_current.transform % 2 == 1 ? Vector2D{bufferSize.y, bufferSize.x} : bufferSize;
            bufferSource.scale(pSurface->m_current.scale);
            bufferSource.transform(Math::wlTransformToHyprutils(Math::invertTransform(pSurface->m_current.transform)), trc.x, trc.y);

            // calculate UV for the basic src_box. Assume dest == size. Scale to dest later
            uvTL = Vector2D(bufferSource.x / bufferSize.x, bufferSource.y / bufferSize.y);
            uvBR = Vector2D((bufferSource.x + bufferSource.width) / bufferSize.x, (bufferSource.y + bufferSource.height) / bufferSize.y);

            if (uvBR.x < 0.00001f || uvBR.y < 0.00001f) {
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
                const auto SHOULD_SKIP = !pWindow || pWindow->presentation().animatingIn();
                if (!SHOULD_SKIP && (RATIO.x < 1 || RATIO.y < 1)) {
                    const auto FIX = RATIO.clamp(Vector2D{0.0001, 0.0001}, Vector2D{1, 1});
                    uvBR           = uvBR * FIX;
                }
            }
        }

        context.primarySurfaceUVTopLeft     = uvTL;
        context.primarySurfaceUVBottomRight = uvBR;

        if (context.primarySurfaceUVTopLeft == Vector2D() && context.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            context.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            context.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }

        if (!main || !pWindow)
            return;

        // FIXME: this doesn't work. We always set MAXIMIZED anyways, so this doesn't need to work, but it's problematic.

        // CBox geom = pWindow->backend().geometry().box;

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

        context.primarySurfaceUVTopLeft     = uvTL;
        context.primarySurfaceUVBottomRight = uvBR;

        if (context.primarySurfaceUVTopLeft == Vector2D() && context.primarySurfaceUVBottomRight == Vector2D(1, 1)) {
            // No special UV mods needed
            context.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
            context.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        }
    } else {
        context.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        context.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }
}

void IElementRenderer::drawRect(CRenderingContext& context, WP<CRectPassElement> element, const CRegion& damage) {
    auto& data = element->m_data;

    if (data.box.w <= 0 || data.box.h <= 0)
        return;

    if (!data.clipBox.empty())
        context.clipBox = data.clipBox;

    data.modifiedBox = data.box;
    context.renderModif.applyToBox(data.modifiedBox);

    data.TOPLEFT[0]  = sc<float>(data.modifiedBox.x);
    data.TOPLEFT[1]  = sc<float>(data.modifiedBox.y);
    data.FULLSIZE[0] = sc<float>(data.modifiedBox.width);
    data.FULLSIZE[1] = sc<float>(data.modifiedBox.height);

    data.drawRegion = data.color.a == 1.F || !data.blur ? damage : context.damage;

    if (context.clipBox.width != 0 && context.clipBox.height != 0) {
        CRegion damageClip{context.clipBox.x, context.clipBox.y, context.clipBox.width, context.clipBox.height};
        data.drawRegion = damageClip.intersect(data.drawRegion);
    }

    draw(context, element, damage);

    context.clipBox = {};
}

void IElementRenderer::drawHints(CRenderingContext& context, WP<CRendererHintsPassElement> element, const CRegion& damage) {
    const auto& m_data = element->m_data;
    if (m_data.renderModif.has_value())
        context.renderModif = *m_data.renderModif;
}

void IElementRenderer::drawPreBlur(CRenderingContext& context, WP<CPreBlurElement> element, const CRegion& damage) {
    TRACY_GPU_ZONE("RenderPreBlurForCurrentMonitor");
    CRenderingContext child{context, context.renderPass()};
    child.renderModif = {};

    // make the fake dmg
    CRegion fakeDamage{0, 0, child.sceneMonitor->m_transformedSize.x, child.sceneMonitor->m_transformedSize.y};

    child.damage = fakeDamage; // the clear inside scissors to renderData.damage, it has to match the blit

    draw(child, element, fakeDamage);

    context.sceneMonitor->m_blurFBDirty = false;
    context.precomputeBlur              = false;
}

void IElementRenderer::drawClear(CRenderingContext& context, WP<CClearPassElement> element, const CRegion& damage) {
    element->m_data.color = g_pHyprRenderer->getConvertedColor(context, element->m_data.color); // FIXME create element copy?
    draw(context, element, damage);
}

void IElementRenderer::drawSurface(CRenderingContext& context, WP<CSurfacePassElement> element, const CRegion& damage) {
    const auto&                   m_data = element->m_data;

    Hyprutils::Utils::CScopeGuard x = {[&context]() {
        context.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        context.primarySurfaceUVBottomRight = Vector2D(-1, -1);
    }};

    if (!m_data.texture) {
        element->discard(context);
        return;
    }

    const auto& TEXTURE = m_data.texture;

    // this is bad, probably has been logged elsewhere. Means the texture failed
    // uploading to the GPU.
    if (!TEXTURE->ok()) {
        element->discard(context);
        return;
    }

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
        element->discard(context);
        return;
    }

    const bool MISALIGNEDFSV1 = std::floor(m_data.pMonitor->m_scale) != m_data.pMonitor->m_scale /* Fractional */ && m_data.surface->m_current.scale == 1 /* fs protocol */ &&
        windowBox.size() != m_data.surface->m_current.bufferSize /* misaligned */ && DELTALESSTHAN(windowBox.width, m_data.surface->m_current.bufferSize.x, 3) &&
        DELTALESSTHAN(windowBox.height, m_data.surface->m_current.bufferSize.y, 3) /* off by one-or-two */ &&
        (!m_data.pWindow || (!m_data.pWindow->sizeAnimation()->isBeingAnimated() && !INTERACTIVERESIZEINPROGRESS)) /* not window or not animated/resizing */ &&
        (!m_data.pLS || (!m_data.pLS->sizeAnimation()->isBeingAnimated())); /* not LS or not animated */

    calculateUVForSurface(context, m_data.pWindow, m_data.surface, m_data.pMonitor->m_self.lock(), m_data.mainSurface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);

    auto    cancelRender = false;
    CRegion clipRegion;

    if (!context.renderingTransformedSource) {
        clipRegion = element->visibleRegion(context, cancelRender);
        if (cancelRender) {
            element->discard(context);
            return;
        }
    }

    const auto surfaceDamage = [&context, &windowBox] {
        if (context.renderingTransformedSource)
            return context.damage.copy();

        CRegion renderDamage = context.damage.copy().intersect(windowBox);
        context.renderModif.applyToRegion(renderDamage);
        return renderDamage;
    };

    // check for fractional scale surfaces misaligning the buffer size
    // in those cases it's better to just force nearest neighbor
    // as long as the window is not animated. During those it'd look weird.
    // UV will fixup it as well
    if (MISALIGNEDFSV1)
        context.useNearestNeighbor = true;

    float rounding      = m_data.rounding;
    float roundingPower = m_data.roundingPower;

    rounding -= 1; // to fix a border issue

    if (m_data.dontRound) {
        rounding      = 0;
        roundingPower = 2.0f;
    }

    const bool WINDOWOPAQUE    = m_data.pWindow && m_data.pWindow->wlSurface()->resource() == m_data.surface ? m_data.pWindow->presentation().opaque() : false;
    const bool CANDISABLEBLEND = ALPHA >= 1.f && OVERALL_ALPHA >= 1.f && rounding <= 0 && WINDOWOPAQUE;

    if (CANDISABLEBLEND)
        g_pHyprRenderer->blend(false);
    else
        g_pHyprRenderer->blend(true);

    // FIXME: This is wrong and will bug the blur out as shit if the first surface
    // is a subsurface that does NOT cover the entire frame. In such cases, we probably should fall back
    // to what we do for misaligned surfaces (blur the entire thing and then render shit without blur)
    if (m_data.surfaceCounter == 0 && !m_data.popup) {
        if (BLUR) {
            CBox blurPatternBox = {m_data.pos.x - m_data.pMonitor->m_position.x, m_data.pos.y - m_data.pMonitor->m_position.y, m_data.w, m_data.h};
            blurPatternBox.scale(m_data.pMonitor->m_scale).round();

            drawElement(context,
                        makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex                   = TEXTURE,
                            .box                   = windowBox,
                            .a                     = ALPHA,
                            .blurA                 = m_data.fadeAlpha,
                            .overallA              = OVERALL_ALPHA,
                            .round                 = rounding,
                            .roundingPower         = roundingPower,
                            .blur                  = true,
                            .blurPatternBox        = blurPatternBox,
                            .blockBlurOptimization = m_data.blockBlurOptimization,
                            .allowCustomUV         = true,
                            .surface               = m_data.surface,
                            .wrapX                 = m_data.wrapX,
                            .wrapY                 = m_data.wrapY,
                            .discardMode           = m_data.discardMode,
                            .discardOpacity        = m_data.discardOpacity,
                            .clipRegion            = clipRegion,
                            .currentLS             = m_data.pLS,
                            .blurOwner             = m_data.pWindow,
                        }),

                        surfaceDamage());
        } else
            drawElement(context,
                        makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex            = TEXTURE,
                            .box            = windowBox,
                            .a              = ALPHA * OVERALL_ALPHA,
                            .round          = rounding,
                            .roundingPower  = roundingPower,
                            .discardActive  = false,
                            .allowCustomUV  = true,
                            .surface        = m_data.surface,
                            .wrapX          = m_data.wrapX,
                            .wrapY          = m_data.wrapY,
                            .discardMode    = m_data.discardMode,
                            .discardOpacity = m_data.discardOpacity,
                            .clipRegion     = clipRegion,
                            .currentLS      = m_data.pLS,
                        }),
                        surfaceDamage());
    } else {
        if (BLUR && m_data.popup)
            drawElement(context,
                        makeShared<CTexPassElement>(CTexPassElement::SRenderData{
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
                            .wrapX                 = m_data.wrapX,
                            .wrapY                 = m_data.wrapY,
                            .discardMode           = m_data.discardMode,
                            .discardOpacity        = m_data.discardOpacity,
                            .clipRegion            = clipRegion,
                            .currentLS             = m_data.pLS,
                        }),
                        surfaceDamage());
        else
            drawElement(context,
                        makeShared<CTexPassElement>(CTexPassElement::SRenderData{
                            .tex            = TEXTURE,
                            .box            = windowBox,
                            .a              = ALPHA * OVERALL_ALPHA,
                            .round          = rounding,
                            .roundingPower  = roundingPower,
                            .discardActive  = false,
                            .allowCustomUV  = true,
                            .surface        = m_data.surface,
                            .wrapX          = m_data.wrapX,
                            .wrapY          = m_data.wrapY,
                            .discardMode    = m_data.discardMode,
                            .discardOpacity = m_data.discardOpacity,
                            .clipRegion     = clipRegion,
                            .currentLS      = m_data.pLS,
                        }),
                        surfaceDamage());
    }

    g_pHyprRenderer->blend(true);

    if (!context.blockSurfaceFeedback)
        element->m_data.surface->presentFeedback(element->m_data.when, context.outputMonitor.lock());
};

void IElementRenderer::preDrawSurface(CRenderingContext& context, WP<CSurfacePassElement> element, const CRegion& damage) {
    context.clipBox            = context.renderingTransformedSource ? CBox{} : element->m_data.clipBox;
    context.useNearestNeighbor = element->m_data.useNearestNeighbor;
    context.currentWindow      = element->m_data.pWindow;

    drawSurface(context, element, damage);

    // add async (dmabuf) buffers to usedBuffers so we can handle release later
    // sync (shm) buffers will be released in commitState, so no need to track them here.
    const auto OUTPUT_MONITOR = context.outputMonitor.lock();
    if (OUTPUT_MONITOR && element->m_data.surface->m_current.buffer && !element->m_data.surface->m_current.buffer->isSynchronous() &&
        std::ranges::none_of(OUTPUT_MONITOR->m_usedAsyncBuffers,
                             [&](const auto& e) { return e.first == element->m_data.surface && e.second == element->m_data.surface->m_current.buffer; }))
        OUTPUT_MONITOR->m_usedAsyncBuffers.emplace_back(element->m_data.surface, element->m_data.surface->m_current.buffer);

    context.clipBox            = {};
    context.useNearestNeighbor = false;
    context.currentWindow.reset();
}

void IElementRenderer::drawTex(CRenderingContext& context, WP<CTexPassElement> element, const CRegion& damage) {
    if (!element->m_data.clipBox.empty())
        context.clipBox = element->m_data.clipBox;

    context.surface = element->m_data.surface;

    const auto transformClipRegion = [&element, &context] {
        CRegion clipRegion = element->m_data.clipRegion.copy();
        context.renderModif.applyToRegion(clipRegion);
        element->m_data.clipRegion = clipRegion;
    };

    Hyprutils::Utils::CScopeGuard x = {[&context]() {
        context.surface.reset();
        context.clipBox = {};
    }};

    if (element->m_data.blur) {
        // make a damage region for this window
        CRegion texDamage = element->m_data.useProvidedDamage ? element->m_data.damage : context.damage;
        if (!element->m_data.useProvidedDamage && !context.renderingTransformedSource)
            texDamage.intersect(element->m_data.box.x, element->m_data.box.y, element->m_data.box.width, element->m_data.box.height);

        // While renderTextureInternalWithDamage will clip the blur as well,
        // clipping texDamage here allows blur generation to be optimized.
        if (!element->m_data.clipRegion.empty())
            texDamage.intersect(element->m_data.clipRegion);

        if (texDamage.empty())
            return;

        if (!element->m_data.useProvidedDamage)
            context.renderModif.applyToRegion(texDamage);

        element->m_data.damage = texDamage;

        // amazing hack: the surface has an opaque region!
        const auto& surface = element->m_data.surface;
        const auto& box     = element->m_data.box;
        CRegion     inverseOpaque;
        if (element->m_data.a >= 1.f && surface && std::round(surface->m_current.size.x * context.sceneMonitor->m_scale) == box.w &&
            std::round(surface->m_current.size.y * context.sceneMonitor->m_scale) == box.h) {
            pixman_box32_t surfbox = {0, 0, surface->m_current.size.x * surface->m_current.scale, surface->m_current.size.y * surface->m_current.scale};
            inverseOpaque          = surface->m_current.opaque;
            inverseOpaque.invert(&surfbox).intersect(0, 0, surface->m_current.size.x * surface->m_current.scale, surface->m_current.size.y * surface->m_current.scale);

            if (inverseOpaque.empty()) {
                element->m_data.blur = false;
                transformClipRegion();
                draw(context, element, damage);
                return;
            }
        } else
            inverseOpaque = {0, 0, element->m_data.box.width, element->m_data.box.height};

        inverseOpaque.scale(context.sceneMonitor->m_scale);
        element->m_data.blockBlurOptimization = element->usesLiveBlur(context);

        //   vvv TODO: layered blur fbs?
        SP<IFramebuffer> blurredFB;
        if (element->m_data.blockBlurOptimization.value_or(false)) {
            inverseOpaque.translate(box.pos());
            context.renderModif.applyToRegion(inverseOpaque);
            inverseOpaque.intersect(element->m_data.damage);
            auto patternBox = element->m_data.blurPatternBox.value_or(box);
            context.renderModif.applyToBox(patternBox);
            std::optional<SBlurShape> shape;
            if (!element->m_data.blurShapeInvalid) {
                auto shapeBox = box;
                context.renderModif.applyToBox(shapeBox);
                if (std::abs(shapeBox.rot) < 0.0001F)
                    shape = SBlurShape{
                        .box           = shapeBox,
                        .radius        = std::max(sc<float>(element->m_data.round), 0.F),
                        .roundingPower = element->m_data.roundingPower,
                    };
            }
            blurredFB =
                g_pHyprRenderer->blurMainFramebuffer(context, element->m_data.a, inverseOpaque, {.patternBox = patternBox, .owner = element->m_data.blurOwner, .shape = shape});
            element->m_data.blurredBG = blurredFB->getTexture();
        } else
            element->m_data.blurredBG = context.sceneMonitor->resources()->m_blurFB->getTexture();

        transformClipRegion();
        draw(context, element, damage);
    } else {
        transformClipRegion();
        draw(context, element, damage);
    }
}

void IElementRenderer::drawTexMatte(CRenderingContext& context, WP<CTextureMatteElement> element, const CRegion& damage) {
    if (context.damage.empty())
        return;

    const auto& m_data = element->m_data;
    if (m_data.disableTransformAndModify) {
        CRenderingContext child{context, context.renderPass()};
        child.renderModif.enabled = false;
        draw(child, element, damage);
    } else
        draw(context, element, damage);
}

static CBox motionBlurSourceBox(const SMotionBlurData& motionBlur, const CBox& outputBox, double padding) {
    if (!motionBlur.enabled || motionBlur.samples < 1)
        return outputBox.intersection(motionBlur.current);

    CBox required;
    bool hasRequired = false;
    for (int i = 0; i < motionBlur.samples; ++i) {
        const double t         = sc<double>(i) / motionBlur.samples;
        const CBox   sampleBox = {
            motionBlur.current.x + (motionBlur.previous.x - motionBlur.current.x) * t,
            motionBlur.current.y + (motionBlur.previous.y - motionBlur.current.y) * t,
            motionBlur.current.w + (motionBlur.previous.w - motionBlur.current.w) * t,
            motionBlur.current.h + (motionBlur.previous.h - motionBlur.current.h) * t,
        };
        const CBox visibleSample = outputBox.intersection(sampleBox);
        if (visibleSample.empty() || sampleBox.w <= 0.0 || sampleBox.h <= 0.0)
            continue;

        const Vector2D scale        = motionBlur.current.size() / sampleBox.size();
        CBox           sourceSample = {
            motionBlur.current.pos() + (visibleSample.pos() - sampleBox.pos()) * scale,
            visibleSample.size() * scale,
        };
        sourceSample.expand(padding);

        if (!hasRequired) {
            required    = sourceSample;
            hasRequired = true;
            continue;
        }

        const double x1 = std::min(required.x, sourceSample.x);
        const double y1 = std::min(required.y, sourceSample.y);
        const double x2 = std::max(required.x + required.w, sourceSample.x + sourceSample.w);
        const double y2 = std::max(required.y + required.h, sourceSample.y + sourceSample.h);
        required        = {x1, y1, x2 - x1, y2 - y1};
    }

    return hasRequired ? required.intersection(motionBlur.current) : CBox{};
}

static bool transformPlanFits(const SWindowTransformPlan& plan, double scale, bool hasMatte) {
    static const auto LIMITS = [] {
        GLint textureSize = 0;
        GLint viewport[2] = {0, 0};
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &textureSize);
        glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);
        return Vector2D{std::min(textureSize, viewport[0]), std::min(textureSize, viewport[1])};
    }();

    constexpr uint64_t MAX_TRANSFORMER_PIXELS = 64ULL * 1024ULL * 1024ULL;
    uint64_t           totalPixels            = 0;

    const double       canvasPadding = 1.0 / scale;
    const auto         ADD_CANVAS    = [&](const CBox& logicalBox) {
        const CBox canvas = pixelBoxForLogical(logicalBox.copy().expand(canvasPadding), scale);
        if (canvas.empty() || canvas.w > LIMITS.x || canvas.h > LIMITS.y)
            return false;

        const uint64_t pixels = sc<uint64_t>(canvas.w) * sc<uint64_t>(canvas.h);
        if (pixels > MAX_TRANSFORMER_PIXELS - totalPixels)
            return false;

        totalPixels += pixels;
        return true;
    };

    if (!ADD_CANVAS(plan.sourceBox))
        return false;

    for (const auto& stage : plan.stages) {
        if (stage.allocatesOutputBuffer && !ADD_CANVAS(stage.outputBox))
            return false;
    }

    return !hasMatte || totalPixels <= MAX_TRANSFORMER_PIXELS / 2;
}

void IElementRenderer::drawTransformedWindow(CRenderingContext& context, WP<CTransformedWindowPassElement> element, const CRegion& damage) {
    if (!element || !element->m_data.pass)
        return;

    auto&      renderData = context;
    const auto pMonitor   = renderData.sceneMonitor;
    if (!pMonitor)
        return;

    const auto      PWINDOW           = element->m_data.window.lock();
    bool            applyTransformers = PWINDOW && !element->m_data.standalone && !element->m_data.renderingSnapshot;
    const CBox      MONITORBOX        = CBox{{}, pMonitor->m_size};

    SMotionBlurData motionBlur         = applyTransformers ? element->m_data.motionBlur : SMotionBlurData{};
    const CBox      visualBox          = applyTransformers ? (motionBlur.enabled ? motionBlur.extents() : element->m_data.transformedBox) : element->m_data.currentBox;
    const bool      HASRENDERMODIFIERS = renderData.renderModif.enabled && !renderData.renderModif.modifs.empty();
    CBox            visibleOutput      = HASRENDERMODIFIERS ? visualBox : visualBox.intersection(MONITORBOX);
    if (visibleOutput.empty())
        return;

    CBox                 transformerOutput = motionBlur.enabled ? motionBlurSourceBox(motionBlur, visibleOutput, 1.0 / pMonitor->m_scale) : visibleOutput;

    SWindowTransformPlan plan;
    if (applyTransformers)
        plan = PWINDOW->effects().transformers()->plan(element->m_data.currentBox, transformerOutput);
    else {
        plan.sourceBox = transformerOutput.intersection(element->m_data.currentBox);
        plan.outputBox = plan.sourceBox;
    }

    const auto renderNestedDirect = [&] {
        CRenderingContext child{context, *element->m_data.pass};
        child.currentWindow = element->m_data.window;
        child.surface.reset();
        child.clipBox                    = {};
        child.renderingTransformedSource = false;
        auto framebufferGuard            = g_pHyprRenderer->bindTempFB(child, context.currentFB);
        element->m_data.pass->render(child, damage);
    };

    if (plan.sourceBox.empty() || !transformPlanFits(plan, pMonitor->m_scale, element->m_data.blur)) {
        applyTransformers = false;
        motionBlur        = {};
        visibleOutput     = HASRENDERMODIFIERS ? element->m_data.currentBox : element->m_data.currentBox.intersection(MONITORBOX);
        if (visibleOutput.empty())
            return;

        plan           = {};
        plan.sourceBox = visibleOutput;
        plan.outputBox = visibleOutput;
        if (!transformPlanFits(plan, pMonitor->m_scale, element->m_data.blur)) {
            renderNestedDirect();
            return;
        }
    }

    const double CANVASPADDING = 1.0 / pMonitor->m_scale;
    CBox         SOURCECANVAS  = pixelBoxForLogical(plan.sourceBox.copy().expand(CANVASPADDING), pMonitor->m_scale);
    auto         fb            = pMonitor->resources()->getUnusedWorkBuffer(SOURCECANVAS.size());
    auto         matteFB       = element->m_data.blur ? pMonitor->resources()->getUnusedWorkBuffer(SOURCECANVAS.size()) : nullptr;
    if (!fb || (element->m_data.blur && !matteFB)) {
        fb.reset();
        matteFB.reset();

        applyTransformers = false;
        motionBlur        = {};
        plan              = {};
        plan.sourceBox    = MONITORBOX;
        plan.outputBox    = MONITORBOX;
        SOURCECANVAS      = {0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y};
        fb                = pMonitor->resources()->getUnusedWorkBuffer();
        matteFB           = element->m_data.blur ? pMonitor->resources()->getUnusedWorkBuffer() : nullptr;
        if (!fb || (element->m_data.blur && !matteFB)) {
            renderNestedDirect();
            return;
        }
    }

    const CRegion  CANVASDAMAGE      = CRegion{0, 0, sc<int>(SOURCECANVAS.w), sc<int>(SOURCECANVAS.h)};
    const Vector2D CANVASTRANSLATION = -SOURCECANVAS.pos();
    const auto     transformWindowFB = [&](CRenderingContext& transformContext, const SWindowTransformBuffer& in) {
        if (!applyTransformers)
            return in;

        return PWINDOW->effects().transformers()->transform(transformContext, in, plan,
                                                            SWindowTransformContext{
                                                                .currentBox        = element->m_data.currentBox,
                                                                .inputBox          = plan.sourceBox,
                                                                .outputBox         = plan.outputBox,
                                                                .monitor           = pMonitor,
                                                                .standalone        = element->m_data.standalone,
                                                                .renderingSnapshot = element->m_data.renderingSnapshot,
                                                            });
    };

    SWindowTransformBuffer last;
    {
        CRenderingContext child{context, *element->m_data.pass};
        child.currentWindow = element->m_data.window;
        child.surface.reset();
        child.clipBox         = {};
        child.damage          = CANVASDAMAGE;
        child.transformDamage = false;
        child.fbSize          = SOURCECANVAS.size();
        auto framebufferGuard = g_pHyprRenderer->bindTempFB(child, fb);
        g_pHyprRenderer->setProjectionType(child, RPT_EXPORT);

        g_pHyprRenderer->draw(child, CClearPassElement::SClearData{CHyprColor(0, 0, 0, 0)});

        child.renderModif = {};
        child.renderModif.modifs.emplace_back(std::make_pair<>(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, CANVASTRANSLATION));
        child.noSimplify                 = true;
        child.renderingTransformedSource = true;
        element->m_data.pass->render(child, CANVASDAMAGE);

        child.renderModif = {};
        last              = transformWindowFB(child, {.framebuffer = fb, .box = SOURCECANVAS});
    }

    SP<IFramebuffer>     blurAlphaMatteFB;
    SP<Render::ITexture> blurAlphaMatte;
    if (matteFB) {
        SWindowTransformBuffer matteLast;
        {
            CRenderPass       mattePass;
            CRenderingContext child{context, mattePass};
            child.currentWindow = element->m_data.window;
            child.surface.reset();
            child.clipBox         = {};
            child.damage          = CANVASDAMAGE;
            child.transformDamage = false;
            child.fbSize          = SOURCECANVAS.size();
            auto framebufferGuard = g_pHyprRenderer->bindTempFB(child, matteFB);
            g_pHyprRenderer->setProjectionType(child, RPT_EXPORT);

            g_pHyprRenderer->draw(child, CClearPassElement::SClearData{CHyprColor(0, 0, 0, 1)});

            child.renderModif = {};
            child.renderModif.modifs.emplace_back(std::make_pair<>(SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, CANVASTRANSLATION));

            g_pHyprRenderer->draw(child,
                                  CRectPassElement::SRectData{
                                      .box           = element->m_data.blurBox,
                                      .color         = CHyprColor(1, 1, 1, 1),
                                      .round         = element->m_data.blurRound,
                                      .roundingPower = element->m_data.blurRoundingPower,
                                  },
                                  CANVASDAMAGE);

            child.renderModif = {};
            matteLast         = transformWindowFB(child, {.framebuffer = matteFB, .box = SOURCECANVAS});
        }

        if (last.framebuffer && matteLast.framebuffer && matteLast.framebuffer->getTexture() && matteLast.success == last.success && matteLast.box == last.box &&
            matteLast.framebuffer->getTexture()->m_size == last.framebuffer->getTexture()->m_size) {
            blurAlphaMatteFB = matteLast.framebuffer;
            blurAlphaMatte   = matteLast.framebuffer->getTexture();
        }
    }

    if (!last.framebuffer || !last.framebuffer->getTexture())
        return;

    const CBox EXPECTEDOUTPUT = plan.stages.empty() ? SOURCECANVAS : pixelBoxForLogical(plan.stages.back().outputBox.copy().expand(CANVASPADDING), pMonitor->m_scale);
    if (!last.success || last.box != EXPECTEDOUTPUT)
        motionBlur = {};

    CBox outputBox = last.box;
    if (motionBlur.enabled) {
        motionBlur.previous.scale(pMonitor->m_scale);
        motionBlur.current.scale(pMonitor->m_scale);
        motionBlur.source          = motionBlur.current;
        motionBlur.sourceTexOrigin = last.box.pos();
        motionBlur.sourceTexSize   = last.framebuffer->getTexture()->m_size;
        outputBox                  = pixelBoxForLogical(visibleOutput, pMonitor->m_scale);
    }

    CTexPassElement::SRenderData data;
    data.tex        = last.framebuffer->getTexture();
    data.box        = outputBox;
    data.a          = 1.F;
    data.motionBlur = motionBlur;

    CRegion outputRegion{outputBox};
    renderData.renderModif.applyToRegion(outputRegion);
    const CRegion drawDamage = damage.copy().intersect(outputRegion);
    data.damage              = drawDamage;
    data.useProvidedDamage   = true;

    if (element->m_data.blur && blurAlphaMatte) {
        data.blur             = true;
        data.forceBlurBlend   = true;
        data.blurPatternBox   = element->m_data.blurBox;
        data.blurShapeInvalid = true;
        data.liveBlurOverride = element->m_data.blurUsesLive;
        data.blurOwner        = element->m_data.window;
        data.blurA            = element->m_data.blurA;
        data.blurAlphaMatte   = blurAlphaMatte;
        data.discardMode      = 0;
    }

    (void)blurAlphaMatteFB;
    drawElement(context, makeShared<CTexPassElement>(data), drawDamage);
}

void IElementRenderer::drawCustom(CRenderingContext& context, WP<IPassElement> element, const CRegion& damage) {
    const auto& elements = element->draw(context);
    for (const auto& el : elements) {
        drawElement(context, el, damage);
    }
}
