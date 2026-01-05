#include "DefaultFloatingAlgorithm.hpp"

#include "../../Algorithm.hpp"

#include "../../../target/Target.hpp"
#include "../../../space/Space.hpp"

using namespace Layout;
using namespace Layout::Floating;

void CDefaultFloatingAlgorithm::newTarget(SP<ITarget> target) {
    const auto WORK_AREA = m_parent->space()->workArea();
    target->setPosition({WORK_AREA.pos() + Vector2D{20, 20}, {640, 400}});
}

void CDefaultFloatingAlgorithm::removeTarget(SP<ITarget> target) {
    ;
}