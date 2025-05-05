#include "IHyprWindowDecoration.hpp"

class CWindow;

IHyprWindowDecoration::IHyprWindowDecoration(PHLWINDOW pWindow) : m_window(pWindow) {
    ;
}

bool IHyprWindowDecoration::onInputOnDeco(const eInputType, const Vector2D&, std::any) {
    return false;
}

eDecorationLayer IHyprWindowDecoration::getDecorationLayer() {
    return DECORATION_LAYER_UNDER;
}

uint64_t IHyprWindowDecoration::getDecorationFlags() {
    return 0;
}

std::string IHyprWindowDecoration::getDisplayName() {
    return "Unknown Decoration";
}
