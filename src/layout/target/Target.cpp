#include "Target.hpp"
#include "../space/Space.hpp"
#include "../../debug/log/Logger.hpp"

using namespace Layout;

void ITarget::setPositionGlobal(const CBox& box) {
    m_box = box;
}

void ITarget::assignToSpace(const SP<CSpace>& space) {
    if (m_space == space)
        return;

    const bool HAD_SPACE = !!m_space;

    if (m_space)
        m_space->remove(m_self.lock());

    m_space = space;

    if (space && HAD_SPACE)
        space->move(m_self.lock());
    else if (space)
        space->add(m_self.lock());

    if (!space)
        Log::logger->log(Log::WARN, "ITarget: assignToSpace with a null space?");
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
