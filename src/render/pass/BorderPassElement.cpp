#include "BorderPassElement.hpp"
#include "../OpenGL.hpp"

CBorderPassElement::CBorderPassElement(const CBorderPassElement::SBorderData& data_) : m_data(data_) {
    ;
}

void CBorderPassElement::draw(const CRegion& damage) {
    if (m_data.hasGrad2)
        g_pHyprOpenGL->renderBorder(
            m_data.box, m_data.grad1, m_data.grad2, m_data.lerp,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
    else
        g_pHyprOpenGL->renderBorder(
            m_data.box, m_data.grad1,
            {.round = m_data.round, .roundingPower = m_data.roundingPower, .borderSize = m_data.borderSize, .a = m_data.a, .outerRound = m_data.outerRound});
}

bool CBorderPassElement::needsLiveBlur() {
    return false;
}

bool CBorderPassElement::needsPrecomputeBlur() {
    return false;
}
