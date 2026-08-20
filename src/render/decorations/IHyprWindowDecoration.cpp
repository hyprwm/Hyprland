#include "IHyprWindowDecoration.hpp"

IHyprWindowDecoration::IHyprWindowDecoration(PHLWINDOW pWindow) : m_window(pWindow) {
    ;
}

PHLWINDOW IHyprWindowDecoration::owningWindow() const {
    return m_window.lock();
}

SP<IHyprWindowDecoration> IHyprWindowDecoration::self() const {
    return m_self.lock();
}

void IHyprWindowDecoration::setSelf(const SP<IHyprWindowDecoration>& self) {
    m_self = self;
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

void IHyprWindowDecoration::initializeAnimations() {
    ;
}

void IHyprWindowDecoration::updateState() {
    ;
}

void IHyprWindowDecoration::onWindowMap() {
    ;
}

void IHyprWindowDecoration::onWindowFocus() {
    ;
}
