#include "PopupAnimationController.hpp"

#include "../Popup.hpp"

using namespace Desktop::View;

CPopupAnimationController::CPopupAnimationController(CPopup* parent) : m_parent(parent) {
    ;
}

Animation::SViewAnimationContext CPopupAnimationController::animateIn() const {
    Animation::SViewAnimationContext ctx;

    ctx.pos.from  = m_parent->coordsGlobal();
    ctx.pos.to    = m_parent->coordsGlobal();
    ctx.size.from = m_parent->size();
    ctx.size.to   = m_parent->size();
    ctx.alpha     = {.from = 0.F, .to = 1.F};

    return ctx;
}

Animation::SViewAnimationContext CPopupAnimationController::animateOut() const {
    Animation::SViewAnimationContext ctx;

    ctx.pos.from  = m_parent->coordsGlobal();
    ctx.pos.to    = m_parent->coordsGlobal();
    ctx.size.from = m_parent->size();
    ctx.size.to   = m_parent->size();
    ctx.alpha     = {.from = m_parent->alpha()[POPUP_ALPHA_FADE]->value(), .to = 0.F};

    return ctx;
}

void CPopupAnimationController::apply(const Animation::SViewAnimationContext& ctx) const {
    if (ctx.alpha.from != ctx.alpha.to) {
        m_parent->alpha()[POPUP_ALPHA_FADE]->setValueAndWarp(ctx.alpha.from);
        *m_parent->alpha()[POPUP_ALPHA_FADE] = ctx.alpha.to;
    }
}
