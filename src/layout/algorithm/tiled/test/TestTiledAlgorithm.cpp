#include "TestTiledAlgorithm.hpp"

#include "../../Algorithm.hpp"

#include "../../../target/Target.hpp"
#include "../../../space/Space.hpp"

using namespace Layout;
using namespace Layout::Tiled;

void CTestTiledAlgorithm::newTarget(SP<ITarget> target) {
    const auto WORK_AREA = m_parent->space()->workArea();
    target->setPosition({WORK_AREA.pos() + Vector2D{20, 400}, {640, 400}});
}

void CTestTiledAlgorithm::removeTarget(SP<ITarget> target) {
    ;
}