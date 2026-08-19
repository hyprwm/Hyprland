#include "ClearPassElement.hpp"

CClearPassElement::CClearPassElement(const CClearPassElement::SClearData& data_) : m_data(data_) {
    ;
}

bool CClearPassElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CClearPassElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}

std::optional<CBox> CClearPassElement::boundingBox(const Render::CRenderingContext&) {
    return CBox{{}, {INT16_MAX, INT16_MAX}};
}

CRegion CClearPassElement::opaqueRegion(const Render::CRenderingContext& context) {
    return *boundingBox(context);
}
