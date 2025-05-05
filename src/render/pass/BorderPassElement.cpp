#include "BorderPassElement.hpp"
#include "../OpenGL.hpp"

CBorderPassElement::CBorderPassElement(const CBorderPassElement::SBorderData& data_) : m_data(data_) {
    ;
}

void CBorderPassElement::draw(const CRegion& damage) {
    if (m_data.hasGrad2)
        g_pHyprOpenGL->renderBorder(m_data.box, m_data.grad1, m_data.grad2, m_data.lerp, m_data.round, m_data.roundingPower, m_data.borderSize, m_data.a, m_data.outerRound);
    else
        g_pHyprOpenGL->renderBorder(m_data.box, m_data.grad1, m_data.round, m_data.roundingPower, m_data.borderSize, m_data.a, m_data.outerRound);
}

bool CBorderPassElement::needsLiveBlur() {
    return false;
}

bool CBorderPassElement::needsPrecomputeBlur() {
    return false;
}
