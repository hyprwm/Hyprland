#include "SurfacePassElement.hpp"
#include "../OpenGL.hpp"
#include "../../desktop/WLSurface.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/DRMSyncobj.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../Renderer.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

CSurfacePassElement::CSurfacePassElement(const CSurfacePassElement::SRenderData& data_) : data(data_) {
    ;
}

void CSurfacePassElement::draw(const CRegion& damage) {
    g_pHyprOpenGL->m_RenderData.currentWindow      = data.pWindow;
    g_pHyprOpenGL->m_RenderData.currentLS          = data.pLS;
    g_pHyprOpenGL->m_RenderData.clipBox            = data.clipBox;
    g_pHyprOpenGL->m_RenderData.discardMode        = data.discardMode;
    g_pHyprOpenGL->m_RenderData.discardOpacity     = data.discardOpacity;
    g_pHyprOpenGL->m_RenderData.useNearestNeighbor = data.useNearestNeighbor;
    g_pHyprOpenGL->m_bEndFrame                     = data.flipEndFrame;

    CScopeGuard x = {[]() {
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprOpenGL->m_RenderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        g_pHyprOpenGL->m_RenderData.useNearestNeighbor          = false;
        g_pHyprOpenGL->m_RenderData.clipBox                     = {};
        g_pHyprOpenGL->m_RenderData.discardMode                 = 0;
        g_pHyprOpenGL->m_RenderData.discardOpacity              = 0;
        g_pHyprOpenGL->m_RenderData.useNearestNeighbor          = false;
        g_pHyprOpenGL->m_bEndFrame                              = false;
        g_pHyprOpenGL->m_RenderData.currentWindow.reset();
        g_pHyprOpenGL->m_RenderData.currentLS.reset();
    }};

    if (!data.texture)
        return;

    const auto& TEXTURE = data.texture;

    // this is bad, probably has been logged elsewhere. Means the texture failed
    // uploading to the GPU.
    if (!TEXTURE->m_iTexID)
        return;

    // explicit sync: wait for the timeline, if any
    if (data.surface->syncobj && data.surface->syncobj->current.acquireTimeline) {
        if (!g_pHyprOpenGL->waitForTimelinePoint(data.surface->syncobj->current.acquireTimeline->timeline, data.surface->syncobj->current.acquirePoint)) {
            Debug::log(ERR, "Renderer: failed to wait for explicit timeline");
            return;
        }
    }

    const auto INTERACTIVERESIZEINPROGRESS = data.pWindow && g_pInputManager->currentlyDraggedWindow && g_pInputManager->dragMode == MBIND_RESIZE;
    TRACY_GPU_ZONE("RenderSurface");

    auto        PSURFACE = CWLSurface::fromResource(data.surface);

    const float ALPHA         = data.alpha * data.fadeAlpha * (PSURFACE ? PSURFACE->m_fAlphaModifier : 1.F);
    const float OVERALL_ALPHA = PSURFACE ? PSURFACE->m_fOverallOpacity : 1.F;
    const bool  BLUR          = data.blur && (!TEXTURE->m_bOpaque || ALPHA < 1.F || OVERALL_ALPHA < 1.F);

    auto        windowBox = getTexBox();

    const auto  PROJSIZEUNSCALED = windowBox.size();

    windowBox.scale(data.pMonitor->scale);
    windowBox.round();

    if (windowBox.width <= 1 || windowBox.height <= 1) {
        discard();
        return;
    }

    const bool MISALIGNEDFSV1 = std::floor(data.pMonitor->scale) != data.pMonitor->scale /* Fractional */ && data.surface->current.scale == 1 /* fs protocol */ &&
        windowBox.size() != data.surface->current.bufferSize /* misaligned */ && DELTALESSTHAN(windowBox.width, data.surface->current.bufferSize.x, 3) &&
        DELTALESSTHAN(windowBox.height, data.surface->current.bufferSize.y, 3) /* off by one-or-two */ &&
        (!data.pWindow || (!data.pWindow->m_vRealSize->isBeingAnimated() && !INTERACTIVERESIZEINPROGRESS)) /* not window or not animated/resizing */;

    if (data.surface->colorManagement.valid())
        Debug::log(TRACE, "FIXME: rendering surface with color management enabled, should apply necessary transformations");
    g_pHyprRenderer->calculateUVForSurface(data.pWindow, data.surface, data.pMonitor->self.lock(), data.mainSurface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);

    // check for fractional scale surfaces misaligning the buffer size
    // in those cases it's better to just force nearest neighbor
    // as long as the window is not animated. During those it'd look weird.
    // UV will fixup it as well
    if (MISALIGNEDFSV1)
        g_pHyprOpenGL->m_RenderData.useNearestNeighbor = true;

    float rounding      = data.rounding;
    float roundingPower = data.roundingPower;

    rounding -= 1; // to fix a border issue

    if (data.dontRound) {
        rounding      = 0;
        roundingPower = 2.0f;
    }

    const bool WINDOWOPAQUE    = data.pWindow && data.pWindow->m_pWLSurface->resource() == data.surface ? data.pWindow->opaque() : false;
    const bool CANDISABLEBLEND = ALPHA >= 1.f && OVERALL_ALPHA >= 1.f && rounding == 0 && WINDOWOPAQUE;

    if (CANDISABLEBLEND)
        g_pHyprOpenGL->blend(false);
    else
        g_pHyprOpenGL->blend(true);

    // FIXME: This is wrong and will bug the blur out as shit if the first surface
    // is a subsurface that does NOT cover the entire frame. In such cases, we probably should fall back
    // to what we do for misaligned surfaces (blur the entire thing and then render shit without blur)
    if (data.surfaceCounter == 0 && !data.popup) {
        if (BLUR)
            g_pHyprOpenGL->renderTextureWithBlur(TEXTURE, &windowBox, ALPHA, data.surface, rounding, roundingPower, data.blockBlurOptimization, data.fadeAlpha, OVERALL_ALPHA);
        else
            g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, ALPHA * OVERALL_ALPHA, rounding, roundingPower, false, true);
    } else {
        if (BLUR && data.popup)
            g_pHyprOpenGL->renderTextureWithBlur(TEXTURE, &windowBox, ALPHA, data.surface, rounding, roundingPower, true, data.fadeAlpha, OVERALL_ALPHA);
        else
            g_pHyprOpenGL->renderTexture(TEXTURE, &windowBox, ALPHA * OVERALL_ALPHA, rounding, roundingPower, false, true);
    }

    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback)
        data.surface->presentFeedback(data.when, data.pMonitor->self.lock());

    g_pHyprOpenGL->blend(true);
}

CBox CSurfacePassElement::getTexBox() {
    const double outputX = -data.pMonitor->vecPosition.x, outputY = -data.pMonitor->vecPosition.y;

    const auto   INTERACTIVERESIZEINPROGRESS = data.pWindow && g_pInputManager->currentlyDraggedWindow && g_pInputManager->dragMode == MBIND_RESIZE;
    auto         PSURFACE                    = CWLSurface::fromResource(data.surface);

    CBox         windowBox;
    if (data.surface && data.mainSurface) {
        windowBox = {(int)outputX + data.pos.x + data.localPos.x, (int)outputY + data.pos.y + data.localPos.y, data.w, data.h};

        // however, if surface buffer w / h < box, we need to adjust them
        const auto PWINDOW = PSURFACE ? PSURFACE->getWindow() : nullptr;

        // center the surface if it's smaller than the viewport we assign it
        if (PSURFACE && !PSURFACE->m_bFillIgnoreSmall && PSURFACE->small() /* guarantees PWINDOW */) {
            const auto CORRECT = PSURFACE->correctSmallVec();
            const auto SIZE    = PSURFACE->getViewporterCorrectedSize();

            if (!INTERACTIVERESIZEINPROGRESS) {
                windowBox.translate(CORRECT);

                windowBox.width  = SIZE.x * (PWINDOW->m_vRealSize->value().x / PWINDOW->m_vReportedSize.x);
                windowBox.height = SIZE.y * (PWINDOW->m_vRealSize->value().y / PWINDOW->m_vReportedSize.y);
            } else {
                windowBox.width  = SIZE.x;
                windowBox.height = SIZE.y;
            }
        }

    } else { //  here we clamp to 2, these might be some tiny specks
        windowBox = {(int)outputX + data.pos.x + data.localPos.x, (int)outputY + data.pos.y + data.localPos.y, std::max((float)data.surface->current.size.x, 2.F),
                     std::max((float)data.surface->current.size.y, 2.F)};
        if (data.pWindow && data.pWindow->m_vRealSize->isBeingAnimated() && data.surface && !data.mainSurface && data.squishOversized /* subsurface */) {
            // adjust subsurfaces to the window
            windowBox.width  = (windowBox.width / data.pWindow->m_vReportedSize.x) * data.pWindow->m_vRealSize->value().x;
            windowBox.height = (windowBox.height / data.pWindow->m_vReportedSize.y) * data.pWindow->m_vRealSize->value().y;
        }
    }

    if (data.squishOversized) {
        if (data.localPos.x + windowBox.width > data.w)
            windowBox.width = data.w - data.localPos.x;
        if (data.localPos.y + windowBox.height > data.h)
            windowBox.height = data.h - data.localPos.y;
    }

    return windowBox;
}

bool CSurfacePassElement::needsLiveBlur() {
    auto        PSURFACE = CWLSurface::fromResource(data.surface);

    const float ALPHA = data.alpha * data.fadeAlpha * (PSURFACE ? PSURFACE->m_fAlphaModifier * PSURFACE->m_fOverallOpacity : 1.F);
    const bool  BLUR  = data.blur && (!data.texture || !data.texture->m_bOpaque || ALPHA < 1.F);

    if (!data.pLS && !data.pWindow)
        return BLUR;

    const bool NEWOPTIM = g_pHyprOpenGL->shouldUseNewBlurOptimizations(data.pLS, data.pWindow);

    return BLUR && !NEWOPTIM;
}

bool CSurfacePassElement::needsPrecomputeBlur() {
    auto        PSURFACE = CWLSurface::fromResource(data.surface);

    const float ALPHA = data.alpha * data.fadeAlpha * (PSURFACE ? PSURFACE->m_fAlphaModifier * PSURFACE->m_fOverallOpacity : 1.F);
    const bool  BLUR  = data.blur && (!data.texture || !data.texture->m_bOpaque || ALPHA < 1.F);

    if (!data.pLS && !data.pWindow)
        return BLUR;

    const bool NEWOPTIM = g_pHyprOpenGL->shouldUseNewBlurOptimizations(data.pLS, data.pWindow);

    return BLUR && NEWOPTIM;
}

std::optional<CBox> CSurfacePassElement::boundingBox() {
    return getTexBox();
}

CRegion CSurfacePassElement::opaqueRegion() {
    auto        PSURFACE = CWLSurface::fromResource(data.surface);

    const float ALPHA = data.alpha * data.fadeAlpha * (PSURFACE ? PSURFACE->m_fAlphaModifier * PSURFACE->m_fOverallOpacity : 1.F);

    if (ALPHA < 1.F)
        return {};

    if (data.surface && data.surface->current.size == Vector2D{data.w, data.h}) {
        CRegion    opaqueSurf = data.surface->current.opaque.copy().intersect(CBox{{}, {data.w, data.h}});
        const auto texBox     = getTexBox();
        opaqueSurf.scale(texBox.size() / Vector2D{data.w, data.h});
        return opaqueSurf.translate(data.pos + data.localPos - data.pMonitor->vecPosition).expand(-data.rounding);
    }

    return data.texture && data.texture->m_bOpaque ? boundingBox()->expand(-data.rounding) : CRegion{};
}

void CSurfacePassElement::discard() {
    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback) {
        Debug::log(TRACE, "discard for invisible surface");
        data.surface->presentFeedback(data.when, data.pMonitor->self.lock(), true);
    }
}
