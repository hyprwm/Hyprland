#include "Target.hpp"
#include "../space/Space.hpp"
#include "../../debug/log/Logger.hpp"

using namespace Layout;

void ITarget::setPosition(const CBox& box) {
    m_box = box.copy().translate(m_space ? m_space->workArea().pos() : Vector2D{});
}

void ITarget::assignToSpace(const SP<CSpace>& space) {
    if (m_space)
        m_space->remove(m_self.lock());

    m_space = space;

    if (space)
        space->add(m_self.lock());

    if (!space)
        Log::logger->log(Log::WARN, "ITarget: assignToSpace with a null space?");
}

SP<CSpace> ITarget::space() const {
    return m_space;
}
