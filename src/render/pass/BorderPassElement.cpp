#include "BorderPassElement.hpp"
#include "../OpenGL.hpp"

CBorderPassElement::CBorderPassElement(const CBorderPassElement::SBorderData& data_) : data(data_) {
    ;
}

void CBorderPassElement::draw(const CRegion& damage) {
    if (data.hasGrad2)
        g_pHyprOpenGL->renderBorder(&data.box, data.grad1, data.grad2, data.lerp, data.round, data.borderSize, data.a, data.outerRound);
    else
        g_pHyprOpenGL->renderBorder(&data.box, data.grad1, data.round, data.borderSize, data.a, data.outerRound);
}

bool CBorderPassElement::needsLiveBlur() {
    return false;
}

bool CBorderPassElement::needsPrecomputeBlur() {
    return false;
}