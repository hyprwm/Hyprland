#include "FloatingAlgorithm.hpp"
#include "Algorithm.hpp"
#include "../space/Space.hpp"

using namespace Layout;

void IFloatingAlgorithm::recalculate() {
    ;
}

void IFloatingAlgorithm::recenter(SP<ITarget> t) {
    const auto LAST = t->lastFloatingSize();

    if (LAST.x <= 5 || LAST.y <= 5)
        return;

    t->setPositionGlobal({m_parent->space()->workArea().middle() - LAST / 2.F, LAST});
}
