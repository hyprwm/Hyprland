#include "LayoutManager.hpp"

#include "space/Space.hpp"
#include "target/Target.hpp"

using namespace Layout;

void CLayoutManager::newTarget(SP<ITarget> target, SP<CSpace> space) {
    target->assignToSpace(space);
}

void CLayoutManager::removeTarget(SP<ITarget> target) {
    target->assignToSpace(nullptr);
}

void CLayoutManager::changeFloatingMode(SP<ITarget> target) {
    ;
}

void CLayoutManager::beginDragTarget(SP<ITarget> target) {
    ;
}

void CLayoutManager::resizeTarget(SP<ITarget> target, eRectCorner corner) {
    ;
}

void CLayoutManager::moveTarget(SP<ITarget> target) {
    ;
}

void CLayoutManager::endDragTarget(SP<ITarget> target) {
    ;
}

void CLayoutManager::fullscreenRequestForTarget(SP<ITarget> target, eFullscreenMode currentEffectiveMode, eFullscreenMode effectiveMode) {
    ;
}

void CLayoutManager::switchTargets(SP<ITarget> a, SP<ITarget> b) {
    ;
}

void CLayoutManager::moveInDirection(SP<ITarget> target, const std::string& direction, bool silent) {
    ;
}

SP<ITarget> CLayoutManager::getNextCandidate(SP<ITarget> from) {
    return nullptr;
}

bool CLayoutManager::isReachable(SP<ITarget> target) {
    return true;
}

void CLayoutManager::bringTargetToTop(SP<ITarget> target) {
    ;
}

std::optional<Vector2D> CLayoutManager::predictSizeForNewTiledTarget() {
    return std::nullopt;
}

void CLayoutManager::fitIfFloatingOnMonitor(SP<ITarget> target) {
    ;
}