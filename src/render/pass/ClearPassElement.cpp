#include "ClearPassElement.hpp"
#include "../OpenGL.hpp"

CClearPassElement::CClearPassElement(const CClearPassElement::SClearData& data_) : data(data_) {
    ;
}

void CClearPassElement::draw(const CRegion& damage) {
    g_pHyprOpenGL->clear(data.color);
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
