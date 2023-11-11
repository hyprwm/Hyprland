#include "IHyprWindowDecoration.hpp"

#include "../../Window.hpp"

IHyprWindowDecoration::IHyprWindowDecoration(CWindow* pWindow) {
    m_pWindow = pWindow;
}

IHyprWindowDecoration::~IHyprWindowDecoration() {}

void IHyprWindowDecoration::onBeginWindowDragOnDeco(const Vector2D&) {
    ;
}

bool IHyprWindowDecoration::onEndWindowDragOnDeco(CWindow* pDraggedWindow, const Vector2D&) {
    return true;
}

void IHyprWindowDecoration::onMouseButtonOnDeco(const Vector2D&, wlr_pointer_button_event*) {
    ;
}

eDecorationLayer IHyprWindowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_UNDER;
}

uint64_t IHyprWindowDecoration::getDecorationFlags() {
    return 0;
}