#include "PassElement.hpp"

std::optional<CBox> IPassElement::boundingBox() {
    return std::nullopt;
}

CRegion IPassElement::opaqueRegion() {
    return {};
}

bool IPassElement::disableSimplification() {
    return false;
}

void IPassElement::discard() {
    ;
}

bool IPassElement::undiscardable() {
    return false;
}
