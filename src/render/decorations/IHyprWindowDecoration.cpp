#include "IHyprWindowDecoration.hpp"

#include "../../Window.hpp"

IHyprWindowDecoration::IHyprWindowDecoration(CWindow* pWindow) {
    m_pWindow = pWindow;
}

IHyprWindowDecoration::~IHyprWindowDecoration() {}

SWindowDecorationExtents IHyprWindowDecoration::getWindowDecorationReservedArea() {
    return SWindowDecorationExtents{};
}

wlr_box IHyprWindowDecoration::getWindowDecorationBox() {
    const SWindowDecorationExtents RESERVED   = getWindowDecorationReservedArea();
    const int                      BORDERSIZE = m_pWindow->getRealBorderSize();
    switch ((RESERVED.topLeft.x != 0) + (RESERVED.topLeft.y != 0) + (RESERVED.bottomRight.x != 0) + (RESERVED.bottomRight.y != 0)) {
        case 0: return {0, 0, 0, 0};
        case 1:
            return {m_pWindow->m_vRealPosition.vec().x - (BORDERSIZE + RESERVED.topLeft.x) * (int)(RESERVED.topLeft.x != 0),
                    m_pWindow->m_vRealPosition.vec().y - (BORDERSIZE + RESERVED.topLeft.y) * (int)(RESERVED.topLeft.y != 0),
                    (RESERVED.topLeft.y || RESERVED.bottomRight.y) ? m_pWindow->m_vRealSize.vec().x : RESERVED.topLeft.x + RESERVED.bottomRight.x,
                    (RESERVED.topLeft.x || RESERVED.bottomRight.x) ? m_pWindow->m_vRealSize.vec().y : RESERVED.topLeft.y + RESERVED.bottomRight.y};
        default:
            // untested
            return {m_pWindow->m_vRealPosition.vec().x - (BORDERSIZE + RESERVED.topLeft.x) * (int)(RESERVED.topLeft.x != 0),
                    m_pWindow->m_vRealPosition.vec().y - (BORDERSIZE + RESERVED.topLeft.y) * (int)(RESERVED.topLeft.y != 0),
                    m_pWindow->m_vRealSize.vec().x + (BORDERSIZE + RESERVED.topLeft.x) * (int)(RESERVED.topLeft.x != 0) +
                        (BORDERSIZE + RESERVED.bottomRight.x) * (int)(RESERVED.bottomRight.x != 0),
                    m_pWindow->m_vRealSize.vec().y + (BORDERSIZE + RESERVED.topLeft.y) * (int)(RESERVED.topLeft.y != 0) +
                        (BORDERSIZE + RESERVED.bottomRight.y) * (int)(RESERVED.bottomRight.y != 0)};
    }
}

bool IHyprWindowDecoration::allowsInput() {
    return false;
}
