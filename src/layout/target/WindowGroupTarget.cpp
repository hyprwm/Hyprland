#include "WindowGroupTarget.hpp"

#include "../space/Space.hpp"
#include "../algorithm/Algorithm.hpp"
#include "WindowTarget.hpp"
#include "Target.hpp"

#include "../../render/Renderer.hpp"

using namespace Layout;

SP<CWindowGroupTarget> CWindowGroupTarget::create(SP<Desktop::View::CGroup> g) {
    auto target    = SP<CWindowGroupTarget>(new CWindowGroupTarget(g));
    target->m_self = target;
    return target;
}

CWindowGroupTarget::CWindowGroupTarget(SP<Desktop::View::CGroup> g) : m_group(g) {
    ;
}

eTargetType CWindowGroupTarget::type() {
    return TARGET_TYPE_GROUP;
}

void CWindowGroupTarget::setPositionGlobal(const CBox& box) {
    ITarget::setPositionGlobal(box);

    updatePos();
}

void CWindowGroupTarget::updatePos() {
    for (const auto& w : m_group->windows()) {
        w->m_target->setPositionGlobal(m_box);
    }
}

void CWindowGroupTarget::assignToSpace(const SP<CSpace>& space) {
    ITarget::assignToSpace(space);

    for (const auto& w : m_group->windows()) {
        w->m_target->setSpaceGhost(space);
    }
}

bool CWindowGroupTarget::floating() {
    return m_group->head()->m_target->floating();
}

void CWindowGroupTarget::setFloating(bool x) {
    for (const auto& w : m_group->windows()) {
        w->m_target->setFloating(x);
    }
}

std::expected<CBox, eGeometryFailure> CWindowGroupTarget::desiredGeometry() {
    return m_group->head()->m_target->desiredGeometry();
}

PHLWINDOW CWindowGroupTarget::window() const {
    return m_group->head();
}

eFullscreenMode CWindowGroupTarget::fullscreenMode() {
    return m_group->head()->m_fullscreenState.internal;
}

void CWindowGroupTarget::setFullscreenMode(eFullscreenMode mode) {
    m_group->head()->m_fullscreenState.internal = mode;
}

std::optional<Vector2D> CWindowGroupTarget::minSize() {
    return m_group->head()->minSize();
}

std::optional<Vector2D> CWindowGroupTarget::maxSize() {
    return m_group->head()->maxSize();
}

void CWindowGroupTarget::damageEntire() {
    g_pHyprRenderer->damageWindow(m_group->head());
}

void CWindowGroupTarget::warpPositionSize() {
    for (const auto& w : m_group->windows()) {
        w->m_target->warpPositionSize();
    }
}
