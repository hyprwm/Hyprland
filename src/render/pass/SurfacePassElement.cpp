#include "SurfacePassElement.hpp"
#include "../OpenGL.hpp"
#include "../../desktop/view/WLSurface.hpp"
#include "../../desktop/view/Window.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/DRMSyncobj.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../Renderer.hpp"

#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

CSurfacePassElement::CSurfacePassElement(const CSurfacePassElement::SRenderData& data_) : m_data(data_) {
    ;
}

CBox CSurfacePassElement::getTexBox() {
    const double outputX = -m_data.pMonitor->m_position.x, outputY = -m_data.pMonitor->m_position.y;

    const auto   INTERACTIVERESIZEINPROGRESS = m_data.pWindow && g_layoutManager->dragController()->target() && g_layoutManager->dragController()->mode() == MBIND_RESIZE;
    auto         PSURFACE                    = Desktop::View::CWLSurface::fromResource(m_data.surface);

    CBox         windowBox;
    if (m_data.surface && m_data.mainSurface) {
        windowBox = {sc<int>(outputX) + m_data.pos.x + m_data.localPos.x, sc<int>(outputY) + m_data.pos.y + m_data.localPos.y, m_data.w, m_data.h};

        // however, if surface buffer w / h < box, we need to adjust them
        const auto PWINDOW = PSURFACE ? Desktop::View::CWindow::fromView(PSURFACE->view()) : nullptr;

        // center the surface if it's smaller than the viewport we assign it
        if (PSURFACE && !PSURFACE->m_fillIgnoreSmall && PSURFACE->small() /* guarantees PWINDOW */) {
            const auto CORRECT  = PSURFACE->correctSmallVec();
            const auto SIZE     = PSURFACE->getViewporterCorrectedSize();
            const auto REPORTED = PWINDOW->getReportedSize();

            if (!INTERACTIVERESIZEINPROGRESS) {
                windowBox.translate(CORRECT);

                windowBox.width  = SIZE.x * (PWINDOW->m_realSize->value().x / REPORTED.x);
                windowBox.height = SIZE.y * (PWINDOW->m_realSize->value().y / REPORTED.y);
            } else {
                windowBox.width  = SIZE.x;
                windowBox.height = SIZE.y;
            }
        }
    } else { //  here we clamp to 2, these might be some tiny specks

        const auto SURFSIZE = m_data.surface->m_current.size;

        windowBox = {sc<int>(outputX) + m_data.pos.x + m_data.localPos.x, sc<int>(outputY) + m_data.pos.y + m_data.localPos.y, std::max(sc<float>(SURFSIZE.x), 2.F),
                     std::max(sc<float>(SURFSIZE.y), 2.F)};
        if (m_data.pWindow && m_data.pWindow->m_realSize->isBeingAnimated() && m_data.surface && !m_data.mainSurface && m_data.squishOversized /* subsurface */) {
            // adjust subsurfaces to the window
            const auto REPORTED = m_data.pWindow->getReportedSize();
            if (REPORTED.x != 0 && REPORTED.y != 0) {
                windowBox.width  = (windowBox.width / REPORTED.x) * m_data.pWindow->m_realSize->value().x;
                windowBox.height = (windowBox.height / REPORTED.y) * m_data.pWindow->m_realSize->value().y;
            }
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
    auto        PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface);

    const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier * PSURFACE->m_overallOpacity : 1.F);
    const bool  BLUR  = m_data.blur && (!m_data.texture || !m_data.texture->m_opaque || ALPHA < 1.F);

    if (!m_data.pLS && !m_data.pWindow)
        return BLUR;

    if (m_data.popup)
        return BLUR;

    const bool NEWOPTIM = g_pHyprRenderer->shouldUseNewBlurOptimizations(m_data.pLS, m_data.pWindow);

    return BLUR && !NEWOPTIM;
}

bool CSurfacePassElement::needsPrecomputeBlur() {
    auto        PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface);

    const float ALPHA = m_data.alpha * m_data.fadeAlpha * (PSURFACE ? PSURFACE->m_alphaModifier * PSURFACE->m_overallOpacity : 1.F);
    const bool  BLUR  = m_data.blur && (!m_data.texture || !m_data.texture->m_opaque || ALPHA < 1.F);

    if (!m_data.pLS && !m_data.pWindow)
        return BLUR;

    if (m_data.popup)
        return false;

    const bool NEWOPTIM = g_pHyprRenderer->shouldUseNewBlurOptimizations(m_data.pLS, m_data.pWindow);

    return BLUR && NEWOPTIM;
}

std::optional<CBox> CSurfacePassElement::boundingBox() {
    return getTexBox();
}

CRegion CSurfacePassElement::opaqueRegion() {
    auto        PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface);

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
    auto PSURFACE = Desktop::View::CWLSurface::fromResource(m_data.surface);
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
        Log::logger->log(Log::TRACE, "discard for invisible surface");
        m_data.surface->presentFeedback(m_data.when, m_data.pMonitor->m_self.lock(), true);
    }
}
