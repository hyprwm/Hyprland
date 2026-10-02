#include "RectPassElement.hpp"
#include "../Renderer.hpp"

CRectPassElement::CRectPassElement(const CRectPassElement::SRectData& data_) : m_data(data_) {
    ;
}

bool CRectPassElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return m_data.color.a < 1.F && m_data.blur && (!m_data.xray || g_pHyprRenderer->blurProviderRequiresLiveBlur());
}

bool CRectPassElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return m_data.color.a < 1.F && m_data.xray && m_data.blur && !g_pHyprRenderer->blurProviderRequiresLiveBlur();
}

std::optional<CBox> CRectPassElement::boundingBox(Render::CRenderContext& ctx) {
    return m_data.box.copy().scale(1.F / ctx.m_data.pMonitor->m_scale).round();
}

CRegion CRectPassElement::opaqueRegion(Render::CRenderContext& ctx) {
    if (m_data.color.a < 1.F)
        return CRegion{};

    CRegion rg = boundingBox(ctx)->expand(-m_data.round);

    if (!m_data.clipBox.empty())
        rg.intersect(m_data.clipBox);

    return rg;
}
