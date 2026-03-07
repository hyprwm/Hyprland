#include "ClearPassElement.hpp"

CClearPassElement::CClearPassElement(const CClearPassElement::SClearData& data_) : m_data(data_) {
    ;
}

bool CClearPassElement::needsLiveBlur() {
    return false;
}

bool CClearPassElement::needsPrecomputeBlur() {
    return false;
}

std::optional<CBox> CClearPassElement::boundingBox() {
    return CBox{{}, {INT16_MAX, INT16_MAX}};
}

CRegion CClearPassElement::opaqueRegion() {
    return *boundingBox();
}
