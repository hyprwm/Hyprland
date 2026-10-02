#include "ClearPassElement.hpp"

CClearPassElement::CClearPassElement(const CClearPassElement::SClearData& data_) : m_data(data_) {
    ;
}

bool CClearPassElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CClearPassElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}

std::optional<CBox> CClearPassElement::boundingBox(Render::CRenderContext& ctx) {
    return CBox{{}, {INT16_MAX, INT16_MAX}};
}

CRegion CClearPassElement::opaqueRegion(Render::CRenderContext& ctx) {
    return *boundingBox(ctx);
}
