#include "BorderPassElement.hpp"

CBorderPassElement::CBorderPassElement(const CBorderPassElement::SBorderData& data_) : m_data(data_) {
    ;
}

bool CBorderPassElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CBorderPassElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}
