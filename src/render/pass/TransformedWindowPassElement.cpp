#include "TransformedWindowPassElement.hpp"

CTransformedWindowPassElement::CTransformedWindowPassElement(CTransformedWindowPassElement::SData&& data) : m_data(std::move(data)) {
    ;
}

bool CTransformedWindowPassElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return (m_data.blur && m_data.blurUsesLive) || (m_data.pass && m_data.pass->needsLiveBlur(ctx));
}

bool CTransformedWindowPassElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return (m_data.blur && !m_data.blurUsesLive) || (m_data.pass && m_data.pass->needsPrecomputeBlur(ctx));
}

std::optional<CBox> CTransformedWindowPassElement::boundingBox(Render::CRenderContext& ctx) {
    if (m_data.motionBlur.enabled)
        return m_data.motionBlur.extents();

    return m_data.transformedBox.empty() ? m_data.currentBox : m_data.transformedBox;
}

CRegion CTransformedWindowPassElement::opaqueRegion(Render::CRenderContext& ctx) {
    return {};
}

bool CTransformedWindowPassElement::disableSimplification(Render::CRenderContext& ctx) {
    return true;
}
