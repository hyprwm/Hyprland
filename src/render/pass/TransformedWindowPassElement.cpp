#include "TransformedWindowPassElement.hpp"

CTransformedWindowPassElement::CTransformedWindowPassElement(CTransformedWindowPassElement::SData&& data) : m_data(std::move(data)) {
    ;
}

bool CTransformedWindowPassElement::needsLiveBlur(const Render::CRenderingContext& context) {
    return (m_data.blur && m_data.blurUsesLive) || (m_data.pass && m_data.pass->needsLiveBlur(context));
}

bool CTransformedWindowPassElement::needsPrecomputeBlur(const Render::CRenderingContext& context) {
    return (m_data.blur && !m_data.blurUsesLive) || (m_data.pass && m_data.pass->needsPrecomputeBlur(context));
}

std::optional<CBox> CTransformedWindowPassElement::boundingBox(const Render::CRenderingContext&) {
    if (m_data.motionBlur.enabled)
        return m_data.motionBlur.extents();

    return m_data.transformedBox.empty() ? m_data.currentBox : m_data.transformedBox;
}

CRegion CTransformedWindowPassElement::opaqueRegion(const Render::CRenderingContext&) {
    return {};
}

bool CTransformedWindowPassElement::disableSimplification() {
    return true;
}
