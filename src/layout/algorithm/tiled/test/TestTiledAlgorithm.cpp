#include "TestTiledAlgorithm.hpp"

#include "../../Algorithm.hpp"

#include "../../../target/Target.hpp"
#include "../../../space/Space.hpp"

using namespace Layout;
using namespace Layout::Tiled;

void CTestTiledAlgorithm::newTarget(SP<ITarget> target) {
    const auto WORK_AREA = m_parent->space()->workArea();
    target->setPositionGlobal({WORK_AREA.pos() + Vector2D{20, 400}, {640, 400}});
}

void CTestTiledAlgorithm::movedTarget(SP<ITarget> target) {
    const auto WORK_AREA = m_parent->space()->workArea();
    target->setPositionGlobal({WORK_AREA.pos() + Vector2D{20, 400}, {640, 400}});
}

void CTestTiledAlgorithm::removeTarget(SP<ITarget> target) {
    ;
}

void CTestTiledAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    ;
}

void CTestTiledAlgorithm::moveTarget(const Vector2D& Δ, SP<ITarget> target) {
    ;
}