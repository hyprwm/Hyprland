#include "ShadowPassElement.hpp"

CShadowPassElement::CShadowPassElement(const CShadowPassElement::SShadowData& data_) : m_data(data_) {
    ;
}

bool CShadowPassElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CShadowPassElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}
