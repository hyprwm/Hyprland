#include "Target.hpp"
#include "../space/Space.hpp"
#include "../../debug/log/Logger.hpp"

using namespace Layout;

void ITarget::setPositionGlobal(const CBox& box) {
    m_box = box;
    m_box.round();
}

void ITarget::assignToSpace(const SP<CSpace>& space, std::optional<Vector2D> focalPoint) {
    if (m_space == space && !m_ghostSpace)
        return;

    const bool HAD_SPACE = !!m_space;

    if (m_space && !m_ghostSpace)
        m_space->remove(m_self.lock());

    m_space = space;

    if (space && HAD_SPACE)
        space->move(m_self.lock(), focalPoint);
    else if (space)
        space->add(m_self.lock());

    if (!space)
        Log::logger->log(Log::WARN, "ITarget: assignToSpace with a null space?");

    m_ghostSpace = false;
}

void ITarget::setSpaceGhost(const SP<CSpace>& space) {
    if (m_space)
        assignToSpace(nullptr);

    m_space = space;

    m_ghostSpace = true;
}

SP<CSpace> ITarget::space() const {
    return m_space;
}

PHLWORKSPACE ITarget::workspace() const {
    if (!m_space)
        return nullptr;

    return m_space->workspace();
}

CBox ITarget::position() const {
    return m_box;
}

void ITarget::rememberFloatingSize(const Vector2D& size) {
    m_floatingSize = size;
}

Vector2D ITarget::lastFloatingSize() const {
    return m_floatingSize;
}

void ITarget::recalc() {
    setPositionGlobal(m_box);
}

void ITarget::setPseudo(bool x) {
    if (m_pseudo == x)
        return;

    m_pseudo = x;

    recalc();
}

bool ITarget::isPseudo() const {
    return m_pseudo;
}

void ITarget::setPseudoSize(const Vector2D& size) {
    m_pseudoSize = size;

    recalc();
}

Vector2D ITarget::pseudoSize() {
    return m_pseudoSize;
}

void ITarget::swap(SP<ITarget> b) {
    if (b->space() == m_space) {
        // simplest
        m_space->swap(m_self.lock(), b);
        return;
    }

    // spaces differ
    if (m_space)
        m_space->swap(m_self.lock(), b);
    if (b->space())
        b->space()->swap(b, m_self.lock());

    std::swap(m_space, b->m_space);

    // recalc both
    if (m_space)
        m_space->recalculate();
    if (b->space())
        b->space()->recalculate();
}
