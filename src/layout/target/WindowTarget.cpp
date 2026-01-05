#include "WindowTarget.hpp"

using namespace Layout;

SP<CWindowTarget> CWindowTarget::create(PHLWINDOW w) {
    auto target    = SP<CWindowTarget>(new CWindowTarget(w));
    target->m_self = target;
    return target;
}

CWindowTarget::CWindowTarget(PHLWINDOW w) : m_window(w) {
    ;
}

eTargetType CWindowTarget::type() {
    return TARGET_TYPE_WINDOW;
}

void CWindowTarget::setPosition(const CBox& box) {
    ITarget::setPosition(box);

    // FIXME:
    m_window->m_position = box.pos();
    m_window->m_size     = box.size();

    *m_window->m_realPosition = box.pos();
    *m_window->m_realSize     = box.size();
}

void CWindowTarget::assignToSpace(const SP<CSpace>& space) {
    ITarget::assignToSpace(space);
}

bool CWindowTarget::shouldBeFloated() {
    return m_window->m_isFloating;
}
