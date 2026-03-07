#include "BorderPassElement.hpp"

CBorderPassElement::CBorderPassElement(const CBorderPassElement::SBorderData& data_) : m_data(data_) {
    ;
}

bool CBorderPassElement::needsLiveBlur() {
    return false;
}

bool CBorderPassElement::needsPrecomputeBlur() {
    return false;
}
