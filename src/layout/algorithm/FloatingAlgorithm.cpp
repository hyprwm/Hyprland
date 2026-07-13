#include "FloatingAlgorithm.hpp"
#include "Algorithm.hpp"
#include "../space/Space.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../managers/fullscreen/handler/FullscreenHandler.hpp"

using namespace Layout;

void IFloatingAlgorithm::recalculate(eRecalculateReason reason) {
    if (!m_parent || !m_parent->space())
        return;

    // Avoid further pos recalc if in fullscreen
    if (Fullscreen::controller()->hasFullscreen(m_parent->space()->workspace(), true)) {
        m_defaultFullscreenHandler->syncTargetSizeAndPosition();
        return;
    }
}

void IFloatingAlgorithm::recenter(SP<ITarget> t) {
    const auto LAST = t->lastFloatingSize();

    if (LAST.x <= 5 || LAST.y <= 5)
        return;

    t->setPositionGlobal({m_parent->space()->workArea().middle() - LAST / 2.F, LAST});
}
