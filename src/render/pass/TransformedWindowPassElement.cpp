#include "TransformedWindowPassElement.hpp"

CTransformedWindowPassElement::CTransformedWindowPassElement(CTransformedWindowPassElement::SData&& data) : m_data(std::move(data)) {
    ;
}

bool CTransformedWindowPassElement::needsLiveBlur() {
    return m_data.blur;
}

bool CTransformedWindowPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> CTransformedWindowPassElement::boundingBox() {
    if (m_data.motionBlur.enabled)
        return m_data.motionBlur.extents();

    return m_data.currentBox;
}

CRegion CTransformedWindowPassElement::opaqueRegion() {
    return {};
}

bool CTransformedWindowPassElement::disableSimplification() {
    return true;
}
