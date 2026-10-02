#include "PassElement.hpp"

std::optional<CBox> IPassElement::boundingBox(Render::CRenderContext& ctx) {
    return std::nullopt;
}

CRegion IPassElement::opaqueRegion(Render::CRenderContext& ctx) {
    return {};
}

bool IPassElement::disableSimplification(Render::CRenderContext& ctx) {
    return false;
}

bool IPassElement::requiresFullDamage(Render::CRenderContext& ctx) {
    return false;
}

void IPassElement::discard(Render::CRenderContext& ctx) {
    ;
}

bool IPassElement::undiscardable(Render::CRenderContext& ctx) {
    return false;
}

std::vector<UP<IPassElement>> IPassElement::draw(Render::CRenderContext& ctx) {
    return {};
}
