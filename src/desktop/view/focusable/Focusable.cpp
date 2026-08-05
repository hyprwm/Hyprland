#include "Focusable.hpp"

using namespace Desktop::View;

IFocusable::IFocusable() {
    ;
}

IFocusable::~IFocusable() = default;

bool IFocusable::acceptsInput() const {
    return focusAvailable() && !isInputBlocked();
}

void IFocusable::setInputBlocked(eFocusBlockReason reason, bool blocked) {
    if (reason == FOCUS_BLOCK_NONE)
        return;

    if (blocked)
        m_reasons |= reason;
    else
        m_reasons &= ~reason;

    onInputBlockStateUpdated(isInputBlocked());
}

bool IFocusable::isInputBlocked() const {
    return m_reasons != FOCUS_BLOCK_NONE;
}

bool IFocusable::isInputBlockedReasonAnyOf(FocusBlockReasons reasons) const {
    return Hyprutils::Memory::sc<bool>(m_reasons & reasons);
}

bool IFocusable::noInputBlockedReasonsBesides(FocusBlockReasons reasons) const {
    return !(m_reasons & ~reasons);
}

bool IFocusable::hasInputBlockedReasonsBesides(FocusBlockReasons reasons) const {
    return !noInputBlockedReasonsBesides(reasons);
}

void IFocusable::onInputBlockStateUpdated(bool) {
    ;
}
