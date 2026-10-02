#include "ShadowPassElement.hpp"

CShadowPassElement::CShadowPassElement(const CShadowPassElement::SShadowData& data_) : m_data(data_) {
    ;
}

bool CShadowPassElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CShadowPassElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}
