#include "IHyprWindowDecoration.hpp"

#include "../../Window.hpp"

IHyprWindowDecoration::IHyprWindowDecoration(CWindow* pWindow) {
    m_pWindow = pWindow;
}

IHyprWindowDecoration::~IHyprWindowDecoration() {}

SWindowDecorationExtents IHyprWindowDecoration::getWindowDecorationReservedArea() {
    return SWindowDecorationExtents{};
}

CRegion IHyprWindowDecoration::getWindowDecorationRegion() {
    const SWindowDecorationExtents RESERVED   = getWindowDecorationReservedArea();
    const int                      BORDERSIZE = m_pWindow->getRealBorderSize();
    return CRegion(m_pWindow->m_vRealPosition.vec().x - (BORDERSIZE + RESERVED.topLeft.x) * (int)(RESERVED.topLeft.x != 0),
                   m_pWindow->m_vRealPosition.vec().y - (BORDERSIZE + RESERVED.topLeft.y) * (int)(RESERVED.topLeft.y != 0),
                   m_pWindow->m_vRealSize.vec().x + (BORDERSIZE + RESERVED.topLeft.x) * (int)(RESERVED.topLeft.x != 0) +
                       (BORDERSIZE + RESERVED.bottomRight.x) * (int)(RESERVED.bottomRight.x != 0),
                   m_pWindow->m_vRealSize.vec().y + (BORDERSIZE + RESERVED.topLeft.y) * (int)(RESERVED.topLeft.y != 0) +
                       (BORDERSIZE + RESERVED.bottomRight.y) * (int)(RESERVED.bottomRight.y != 0))
        .subtract(CRegion(m_pWindow->m_vRealPosition.vec().x - BORDERSIZE, m_pWindow->m_vRealPosition.vec().y - BORDERSIZE, m_pWindow->m_vRealSize.vec().x + 2 * BORDERSIZE,
                          m_pWindow->m_vRealSize.vec().y + 2 * BORDERSIZE));
}

bool IHyprWindowDecoration::allowsInput() {
    return false;
}
