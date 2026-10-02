#include "BorderPassElement.hpp"

CBorderPassElement::CBorderPassElement(const CBorderPassElement::SBorderData& data_) : m_data(data_) {
    ;
}

bool CBorderPassElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CBorderPassElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}
