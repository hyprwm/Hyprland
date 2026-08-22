#include "TexPassElement.hpp"
#include "../../helpers/MotionBlur.hpp"
#include "../Renderer.hpp"

CBox SMotionBlurData::extents() const {
    if (!enabled)
        return current;

    return MotionBlur::extents(previous, current);
}

CTexPassElement::CTexPassElement(const SRenderData& data) : m_data(data) {
    ;
}

CTexPassElement::CTexPassElement(CTexPassElement::SRenderData&& data) : m_data(std::move(data)) {
    ;
}

bool CTexPassElement::needsLiveBlur(const Render::CRenderingContext& context) {
    return usesLiveBlur(context);
}

bool CTexPassElement::needsPrecomputeBlur(const Render::CRenderingContext& context) {
    return m_data.blur && !usesLiveBlur(context);
}

bool CTexPassElement::usesLiveBlur(const Render::CRenderingContext& context) {
    if (m_usesLiveBlur.has_value())
        return *m_usesLiveBlur;

    if (m_data.liveBlurOverride.has_value()) {
        m_usesLiveBlur = m_data.blur && *m_data.liveBlurOverride;
        return *m_usesLiveBlur;
    }

    m_usesLiveBlur =
        m_data.blur && (m_data.blockBlurOptimization.value_or(false) || !g_pHyprRenderer->shouldUseNewBlurOptimizations(context, m_data.currentLS.lock(), m_data.blurOwner.lock()));
    return *m_usesLiveBlur;
}

std::optional<CBox> CTexPassElement::boundingBox(const Render::CRenderingContext& context) {
    if (m_data.motionBlur.enabled)
        return m_data.motionBlur.extents().copy().scale(1.F / context.sceneMonitor->m_scale).round();

    return m_data.box.copy().scale(1.F / context.sceneMonitor->m_scale).round();
}

CRegion CTexPassElement::opaqueRegion(const Render::CRenderingContext&) {
    return {}; // TODO:
}

void CTexPassElement::discard(Render::CRenderingContext& context) {
    ;
}
