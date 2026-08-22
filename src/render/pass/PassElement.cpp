#include "PassElement.hpp"

std::optional<CBox> IPassElement::boundingBox(const Render::CRenderingContext&) {
    return std::nullopt;
}

CRegion IPassElement::opaqueRegion(const Render::CRenderingContext&) {
    return {};
}

bool IPassElement::disableSimplification() {
    return false;
}

void IPassElement::discard(Render::CRenderingContext&) {
    ;
}

bool IPassElement::undiscardable() {
    return false;
}

std::vector<UP<IPassElement>> IPassElement::draw(Render::CRenderingContext&) {
    return {};
}
