#include "IHyprWindowDecoration.hpp"

#include "../../Window.hpp"

IHyprWindowDecoration::IHyprWindowDecoration(CWindow* pWindow) {
    m_pWindow = pWindow;
}

IHyprWindowDecoration::~IHyprWindowDecoration() {}

CRegion IHyprWindowDecoration::getWindowDecorationRegion() {
    const SWindowDecorationExtents EXTENTS = getWindowDecorationExtents();
    if (!EXTENTS.isInternalDecoration && !EXTENTS.isReservedArea)
        return CRegion(0, 0, 0, 0);

    const int BORDERSIZE = EXTENTS.isInternalDecoration ? 0 : m_pWindow->getRealBorderSize();
    return CRegion(m_pWindow->m_vRealPosition.vec().x - (BORDERSIZE + EXTENTS.topLeft.x) * (int)(EXTENTS.topLeft.x != 0),
                   m_pWindow->m_vRealPosition.vec().y - (BORDERSIZE + EXTENTS.topLeft.y) * (int)(EXTENTS.topLeft.y != 0),
                   m_pWindow->m_vRealSize.vec().x + (BORDERSIZE + EXTENTS.topLeft.x) * (int)(EXTENTS.topLeft.x != 0) +
                       (BORDERSIZE + EXTENTS.bottomRight.x) * (int)(EXTENTS.bottomRight.x != 0),
                   m_pWindow->m_vRealSize.vec().y + (BORDERSIZE + EXTENTS.topLeft.y) * (int)(EXTENTS.topLeft.y != 0) +
                       (BORDERSIZE + EXTENTS.bottomRight.y) * (int)(EXTENTS.bottomRight.y != 0))
        .subtract(CRegion(m_pWindow->m_vRealPosition.vec().x - BORDERSIZE, m_pWindow->m_vRealPosition.vec().y - BORDERSIZE, m_pWindow->m_vRealSize.vec().x + 2 * BORDERSIZE,
                          m_pWindow->m_vRealSize.vec().y + 2 * BORDERSIZE));
}

void IHyprWindowDecoration::forceReload() {
    updateWindow(m_pWindow);
}

bool IHyprWindowDecoration::allowsInput() {
    return false;
}

void IHyprWindowDecoration::dragWindowToDecoration(CWindow*, const Vector2D&) {}

void IHyprWindowDecoration::clickDecoration(const Vector2D&) {}

void IHyprWindowDecoration::dragFromDecoration(const Vector2D&) {}

void addExtentsToBox(wlr_box* box, SWindowDecorationExtents* extents) {
    box->x -= extents->topLeft.x;
    box->y -= extents->topLeft.y;
    box->width += extents->topLeft.x + extents->bottomRight.x;
    box->height += extents->topLeft.y + extents->bottomRight.y;
}
