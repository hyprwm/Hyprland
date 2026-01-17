#include "DefaultFloatingAlgorithm.hpp"

#include "../../Algorithm.hpp"

#include "../../../target/WindowTarget.hpp"
#include "../../../space/Space.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../helpers/Monitor.hpp"

using namespace Layout;
using namespace Layout::Floating;

constexpr const Vector2D DEFAULT_SIZE = {640, 400};

//
void CDefaultFloatingAlgorithm::newTarget(SP<ITarget> target) {
    const auto WORK_AREA    = m_parent->space()->workArea();
    const auto DESIRED_GEOM = target->desiredGeometry();

    CBox       windowGeometry;

    if (!DESIRED_GEOM) {
        switch (DESIRED_GEOM.error()) {
            case GEOMETRY_INVALID_DESIRED: {
                // if the desired is invalid, we hide the window.
                if (target->type() == TARGET_TYPE_WINDOW)
                    dynamicPointerCast<CWindowTarget>(target)->window()->setHidden(true);
                return;
            }
            case GEOMETRY_NO_DESIRED: {
                // add a default geom
                windowGeometry = CBox{WORK_AREA.middle() - DEFAULT_SIZE / 2.F, DEFAULT_SIZE};
                break;
            }
        }
    } else
        windowGeometry = DESIRED_GEOM.value();

    // FIXME: this is wrong
    if (WORK_AREA.containsPoint(windowGeometry.middle()))
        target->setPositionGlobal(windowGeometry);
    else
        target->setPositionGlobal(windowGeometry.translate(WORK_AREA.pos()));

    // TODO: not very OOP, is it?
    if (const auto WTARGET = dynamicPointerCast<CWindowTarget>(target); WTARGET) {
        static auto PXWLFORCESCALEZERO = CConfigValue<Hyprlang::INT>("xwayland:force_zero_scaling");

        const auto  PWINDOW  = WTARGET->window();
        const auto  PMONITOR = WTARGET->space()->workspace()->m_monitor.lock();

        if (*PXWLFORCESCALEZERO && PWINDOW->m_isX11)
            *PWINDOW->m_realSize = PWINDOW->m_realSize->goal() / PMONITOR->m_scale;

        if (PWINDOW->m_X11DoesntWantBorders || (PWINDOW->m_isX11 && PWINDOW->isX11OverrideRedirect())) {
            PWINDOW->m_realPosition->warp();
            PWINDOW->m_realSize->warp();
        }

        if (!PWINDOW->isX11OverrideRedirect())
            g_pCompositor->changeWindowZOrder(PWINDOW, true);
        else {
            PWINDOW->m_pendingReportedSize = PWINDOW->m_realSize->goal();
            PWINDOW->m_reportedSize        = PWINDOW->m_pendingReportedSize;
        }
    }
}

void CDefaultFloatingAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    auto       LAST_SIZE    = target->lastFloatingSize();
    const auto CURRENT_SIZE = target->position().size();

    if (LAST_SIZE.x < 5 || LAST_SIZE.y < 5)
        LAST_SIZE = target->desiredGeometry().value_or(CBox{{}, DEFAULT_SIZE}).size();

    // Avoid floating toggles that don't change size, they aren't easily visible to the user
    if (std::abs(LAST_SIZE.x - CURRENT_SIZE.x) < 5 && std::abs(LAST_SIZE.y - CURRENT_SIZE.y) < 5)
        LAST_SIZE += Vector2D{10, 10};

    const auto CURRENT_CENTER = target->position().middle();

    // put around the current center
    target->setPositionGlobal(CBox{CURRENT_CENTER - LAST_SIZE / 2.F, LAST_SIZE});
}

void CDefaultFloatingAlgorithm::removeTarget(SP<ITarget> target) {
    target->rememberFloatingSize(target->position().size());
}

void CDefaultFloatingAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    auto pos = target->position();
    pos.w += Δ.x;
    pos.h += Δ.y;
    pos.translate(-Δ / 2.F);
    target->setPositionGlobal(pos);

    if (g_layoutManager->dragController()->target() == target)
        target->warpPositionSize();
}

void CDefaultFloatingAlgorithm::moveTarget(const Vector2D& Δ, SP<ITarget> target) {
    auto pos = target->position();
    pos.translate(Δ);
    target->setPositionGlobal(pos);

    if (g_layoutManager->dragController()->target() == target)
        target->warpPositionSize();
}

void CDefaultFloatingAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    auto posABackup = a->position();
    a->setPositionGlobal(b->position());
    b->setPositionGlobal(posABackup);
}

void CDefaultFloatingAlgorithm::moveTargetInDirection(SP<ITarget> t, Layout::eDirection dir, bool silent) {
    ; // TODO:
}