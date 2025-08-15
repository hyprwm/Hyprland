#include "SurfacePassElement.hpp"
#include "../OpenGL.hpp"
#include "../../desktop/WLSurface.hpp"
#include "../../desktop/Window.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/DRMSyncobj.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../Renderer.hpp"

#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

CSurfacePassElement::CSurfacePassElement(const CSurfacePassElement::SRenderData& data_) : m_data(data_) {
    ;
}

void CSurfacePassElement::draw(const CRegion& damage) {
    g_pHyprOpenGL->m_renderData.currentWindow      = m_data.pWindow;
    g_pHyprOpenGL->m_renderData.surface            = m_data.surface;
    g_pHyprOpenGL->m_renderData.currentLS          = m_data.pLS;
    g_pHyprOpenGL->m_renderData.clipBox            = m_data.clipBox;
    g_pHyprOpenGL->m_renderData.discardMode        = m_data.discardMode;
    g_pHyprOpenGL->m_renderData.discardOpacity     = m_data.discardOpacity;
    g_pHyprOpenGL->m_renderData.useNearestNeighbor = m_data.useNearestNeighbor;
    g_pHyprOpenGL->pushMonitorTransformEnabled(m_data.flipEndFrame);

    CScopeGuard x = {[]() {
        g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft     = Vector2D(-1, -1);
        g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight = Vector2D(-1, -1);
        g_pHyprOpenGL->m_renderData.useNearestNeighbor          = false;
        g_pHyprOpenGL->m_renderData.clipBox                     = {};
        g_pHyprOpenGL->m_renderData.clipRegion                  = {};
        g_pHyprOpenGL->m_renderData.discardMode                 = 0;
        g_pHyprOpenGL->m_renderData.discardOpacity              = 0;
        g_pHyprOpenGL->m_renderData.useNearestNeighbor          = false;
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
    if (!TEXTURE->m_texID)
        return;

    const auto INTERACTIVERESIZEINPROGRESS = m_data.pWindow && g_pInputManager->m_currentlyDraggedWindow && g_pInputManager->m_dragMode == MBIND_RESIZE;
    TRACY_GPU_ZONE("RenderSurface");

    auto        PSURFACE = CWLSurface::fromResource(m_data.surface);

    const float ALPHA         = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier : 1.F);
    const float OVERALL_ALPHA = PSURFACE ? PSURFACE->m_overallOpacity : 1.F;
    const bool  BLUR          = m_data.blur && (!TEXTURE->m_opaque || ALPHA < 1.F || OVERALL_ALPHA < 1.F);

    auto        windowBox = getTexBox();

    const auto  PROJSIZEUNSCALED = windowBox.size();

    windowBox.scale(m_data.pMonitor->m_scale);
    windowBox.round();

    if (windowBox.width <= 1 || windowBox.height <= 1) {
        discard();
        return;
    }

    const bool MISALIGNEDFSV1 = std::floor(m_data.pMonitor->m_scale) != m_data.pMonitor->m_scale /* Fractional */ && m_data.surface->m_current.scale == 1 /* fs protocol */ &&
        windowBox.size() != m_data.surface->m_current.bufferSize /* misaligned */ && DELTALESSTHAN(windowBox.width, m_data.surface->m_current.bufferSize.x, 3) &&
        DELTALESSTHAN(windowBox.height, m_data.surface->m_current.bufferSize.y, 3) /* off by one-or-two */ &&
        (!m_data.pWindow || (!m_data.pWindow->m_realSize->isBeingAnimated() && !INTERACTIVERESIZEINPROGRESS)) /* not window or not animated/resizing */;

    g_pHyprRenderer->calculateUVForSurface(m_data.pWindow, m_data.surface, m_data.pMonitor->m_self.lock(), m_data.mainSurface, windowBox.size(), PROJSIZEUNSCALED, MISALIGNEDFSV1);

    auto cancelRender                      = false;
    g_pHyprOpenGL->m_renderData.clipRegion = visibleRegion(cancelRender);
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

    const bool WINDOWOPAQUE    = m_data.pWindow && m_data.pWindow->m_wlSurface->resource() == m_data.surface ? m_data.pWindow->opaque() : false;
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

    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback)
        m_data.surface->presentFeedback(m_data.when, m_data.pMonitor->m_self.lock());

    // add async (dmabuf) buffers to usedBuffers so we can handle release later
    // sync (shm) buffers will be released in commitState, so no need to track them here
    if (m_data.surface->m_current.buffer && !m_data.surface->m_current.buffer->isSynchronous())
        g_pHyprRenderer->m_usedAsyncBuffers.emplace_back(m_data.surface->m_current.buffer);

    g_pHyprOpenGL->blend(true);
}

CBox CSurfacePassElement::getTexBox() {
    const double outputX = -m_data.pMonitor->m_position.x, outputY = -m_data.pMonitor->m_position.y;

    const auto   INTERACTIVERESIZEINPROGRESS = m_data.pWindow && g_pInputManager->m_currentlyDraggedWindow && g_pInputManager->m_dragMode == MBIND_RESIZE;
    auto         PSURFACE                    = CWLSurface::fromResource(m_data.surface);

    CBox         windowBox;
    if (m_data.surface && m_data.mainSurface) {
        windowBox = {sc<int>(outputX) + m_data.pos.x + m_data.localPos.x, sc<int>(outputY) + m_data.pos.y + m_data.localPos.y, m_data.w, m_data.h};

        // however, if surface buffer w / h < box, we need to adjust them
        const auto PWINDOW = PSURFACE ? PSURFACE->getWindow() : nullptr;

        // center the surface if it's smaller than the viewport we assign it
        if (PSURFACE && !PSURFACE->m_fillIgnoreSmall && PSURFACE->small() /* guarantees PWINDOW */) {
            const auto CORRECT = PSURFACE->correctSmallVec();
            const auto SIZE    = PSURFACE->getViewporterCorrectedSize();

            if (!INTERACTIVERESIZEINPROGRESS) {
                windowBox.translate(CORRECT);

                windowBox.width  = SIZE.x * (PWINDOW->m_realSize->value().x / PWINDOW->m_reportedSize.x);
                windowBox.height = SIZE.y * (PWINDOW->m_realSize->value().y / PWINDOW->m_reportedSize.y);
            } else {
                windowBox.width  = SIZE.x;
                windowBox.height = SIZE.y;
            }
        }

    } else { //  here we clamp to 2, these might be some tiny specks
        windowBox = {sc<int>(outputX) + m_data.pos.x + m_data.localPos.x, sc<int>(outputY) + m_data.pos.y + m_data.localPos.y,
                     std::max(sc<float>(m_data.surface->m_current.size.x), 2.F), std::max(sc<float>(m_data.surface->m_current.size.y), 2.F)};
        if (m_data.pWindow && m_data.pWindow->m_realSize->isBeingAnimated() && m_data.surface && !m_data.mainSurface && m_data.squishOversized /* subsurface */) {
            // adjust subsurfaces to the window
            windowBox.width  = (windowBox.width / m_data.pWindow->m_reportedSize.x) * m_data.pWindow->m_realSize->value().x;
            windowBox.height = (windowBox.height / m_data.pWindow->m_reportedSize.y) * m_data.pWindow->m_realSize->value().y;
        }
    }

    if (m_data.squishOversized) {
        if (m_data.localPos.x + windowBox.width > m_data.w)
            windowBox.width = m_data.w - m_data.localPos.x;
        if (m_data.localPos.y + windowBox.height > m_data.h)
            windowBox.height = m_data.h - m_data.localPos.y;
    }

    return windowBox;
}

bool CSurfacePassElement::needsLiveBlur() {
    auto        PSURFACE = CWLSurface::fromResource(m_data.surface);

    const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier * PSURFACE->m_overallOpacity : 1.F);
    const bool  BLUR  = m_data.blur && (!m_data.texture || !m_data.texture->m_opaque || ALPHA < 1.F);

    if (!m_data.pLS && !m_data.pWindow)
        return BLUR;

    if (m_data.popup)
        return BLUR;

    const bool NEWOPTIM = g_pHyprOpenGL->shouldUseNewBlurOptimizations(m_data.pLS, m_data.pWindow);

    return BLUR && !NEWOPTIM;
}

bool CSurfacePassElement::needsPrecomputeBlur() {
    auto        PSURFACE = CWLSurface::fromResource(m_data.surface);

    const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier * PSURFACE->m_overallOpacity : 1.F);
    const bool  BLUR  = m_data.blur && (!m_data.texture || !m_data.texture->m_opaque || ALPHA < 1.F);

    if (!m_data.pLS && !m_data.pWindow)
        return BLUR;

    if (m_data.popup)
        return false;

    const bool NEWOPTIM = g_pHyprOpenGL->shouldUseNewBlurOptimizations(m_data.pLS, m_data.pWindow);

    return BLUR && NEWOPTIM;
}

std::optional<CBox> CSurfacePassElement::boundingBox() {
    return getTexBox();
}

CRegion CSurfacePassElement::opaqueRegion() {
    auto        PSURFACE = CWLSurface::fromResource(m_data.surface);

    const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier * PSURFACE->m_overallOpacity : 1.F);

    if (ALPHA < 1.F)
        return {};

    if (m_data.surface && m_data.surface->m_current.size == Vector2D{m_data.w, m_data.h}) {
        CRegion    opaqueSurf = m_data.surface->m_current.opaque.copy().intersect(CBox{{}, {m_data.w, m_data.h}});
        const auto texBox     = getTexBox();
        opaqueSurf.scale(texBox.size() / Vector2D{m_data.w, m_data.h});
        return opaqueSurf.translate(m_data.pos + m_data.localPos - m_data.pMonitor->m_position).expand(-m_data.rounding);
    }

    return m_data.texture && m_data.texture->m_opaque ? boundingBox()->expand(-m_data.rounding) : CRegion{};
}

CRegion CSurfacePassElement::visibleRegion(bool& cancel) {
    auto PSURFACE = CWLSurface::fromResource(m_data.surface);
    if (!PSURFACE)
        return {};

    const auto& bufferSize = m_data.surface->m_current.bufferSize;

    auto        visibleRegion = PSURFACE->m_visibleRegion.copy();
    if (visibleRegion.empty())
        return {};

    visibleRegion.intersect(CBox(Vector2D(), bufferSize));

    if (visibleRegion.empty()) {
        cancel = true;
        return visibleRegion;
    }

    // deal with any rounding errors that might come from scaling
    visibleRegion.expand(1);

    auto uvTL = g_pHyprOpenGL->m_renderData.primarySurfaceUVTopLeft;
    auto uvBR = g_pHyprOpenGL->m_renderData.primarySurfaceUVBottomRight;

    if (uvTL == Vector2D(-1, -1))
        uvTL = Vector2D(0, 0);

    if (uvBR == Vector2D(-1, -1))
        uvBR = Vector2D(1, 1);

    visibleRegion.translate(-uvTL * bufferSize);

    auto texBox = getTexBox();
    texBox.scale(m_data.pMonitor->m_scale);
    texBox.round();

    visibleRegion.scale((Vector2D(1, 1) / (uvBR - uvTL)) * (texBox.size() / bufferSize));
    visibleRegion.translate((m_data.pos + m_data.localPos - m_data.pMonitor->m_position) * m_data.pMonitor->m_scale);

    return visibleRegion;
}

void CSurfacePassElement::discard() {
    if (!g_pHyprRenderer->m_bBlockSurfaceFeedback) {
        Debug::log(TRACE, "discard for invisible surface");
        m_data.surface->presentFeedback(m_data.when, m_data.pMonitor->m_self.lock(), true);
    }
}
