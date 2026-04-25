#include "ShadowPassElement.hpp"

CShadowPassElement::CShadowPassElement(const CShadowPassElement::SShadowData& data_) : m_data(data_) {
    ;
}

bool CShadowPassElement::needsLiveBlur() {
    return false;
}

bool CShadowPassElement::needsPrecomputeBlur() {
    return false;
}
