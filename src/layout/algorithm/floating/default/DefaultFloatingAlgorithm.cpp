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
    const auto WORK_AREA    = m_parent->space()->workArea(true);
    const auto DESIRED_GEOM = target->desiredGeometry();
    const auto MONITOR_POS  = m_parent->space()->workspace()->m_monitor->logicalBox().pos();

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
    } else {
        if (DESIRED_GEOM->pos)
            windowGeometry = CBox{DESIRED_GEOM->pos.value(), DESIRED_GEOM->size};
        else
            windowGeometry = CBox{WORK_AREA.middle() - DESIRED_GEOM->size / 2.F, DESIRED_GEOM->size};
    }

    bool posOverridden = false;

    if (target->window() && target->window()->m_firstMap) {
        const auto WINDOW = target->window();

        // set this here so that expressions can use it. This could be wrong of course.
        WINDOW->m_realSize->setValueAndWarp(DESIRED_GEOM ? DESIRED_GEOM->size : DEFAULT_SIZE);

        if (!WINDOW->m_ruleApplicator->static_.size.empty()) {
            const auto COMPUTED = WINDOW->calculateExpression(WINDOW->m_ruleApplicator->static_.size);
            if (!COMPUTED)
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", WINDOW->m_ruleApplicator->static_.size);
            else {
                windowGeometry.w = COMPUTED->x;
                windowGeometry.h = COMPUTED->y;

                // update for pos to work with size.
                WINDOW->m_realPosition->setValueAndWarp(*COMPUTED);
            }
        }

        if (!WINDOW->m_ruleApplicator->static_.position.empty()) {
            const auto COMPUTED = WINDOW->calculateExpression(WINDOW->m_ruleApplicator->static_.position);
            if (!COMPUTED)
                Log::logger->log(Log::ERR, "failed to parse {} as an expression", WINDOW->m_ruleApplicator->static_.position);
            else {
                windowGeometry.x = COMPUTED->x + MONITOR_POS.x;
                windowGeometry.y = COMPUTED->y + MONITOR_POS.y;
                posOverridden    = true;
            }
        }

        if (WINDOW->m_ruleApplicator->static_.center.value_or(false)) {
            const auto POS   = WORK_AREA.middle() - windowGeometry.size() / 2.f;
            windowGeometry.x = POS.x;
            windowGeometry.y = POS.y;
            posOverridden    = true;
        }
    } else if (target->lastFloatingSize().x > 5 && target->lastFloatingSize().y > 5) {
        windowGeometry.w = target->lastFloatingSize().x;
        windowGeometry.h = target->lastFloatingSize().y;
    }

    if (!posOverridden && (!DESIRED_GEOM || !DESIRED_GEOM->pos))
        windowGeometry = CBox{WORK_AREA.middle() - windowGeometry.size() / 2.F, windowGeometry.size()};

    if (posOverridden || WORK_AREA.containsPoint(windowGeometry.middle()))
        target->setPositionGlobal(windowGeometry);
    else {
        const auto POS   = WORK_AREA.middle() - windowGeometry.size() / 2.f;
        windowGeometry.x = POS.x;
        windowGeometry.y = POS.y;

        target->setPositionGlobal(windowGeometry);
    }

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

    // ignore positioning a dragged target
    if (g_layoutManager->dragController()->target() == target)
        return;

    if (LAST_SIZE.x < 5 || LAST_SIZE.y < 5) {
        const auto DESIRED = target->desiredGeometry();
        LAST_SIZE          = DESIRED ? DESIRED->size : DEFAULT_SIZE;
    }

    if (target->wasTiling()) {
        // Avoid floating toggles that don't change size, they aren't easily visible to the user
        if (std::abs(LAST_SIZE.x - CURRENT_SIZE.x) < 5 && std::abs(LAST_SIZE.y - CURRENT_SIZE.y) < 5)
            LAST_SIZE += Vector2D{10, 10};

        // calculate new position
        const auto OLD_CENTER = target->position().middle();

        // put around the current center, fit in workArea
        target->setPositionGlobal(fitBoxInWorkArea(CBox{OLD_CENTER - LAST_SIZE / 2.F, LAST_SIZE}, target));

    } else {
        // calculate new position
        const auto THIS_MON_POS = m_parent->space()->workspace()->m_monitor->m_position;
        const auto OLD_POS      = target->position().pos();
        const auto MON_FROM_OLD = g_pCompositor->getMonitorFromVector(OLD_POS);
        const auto NEW_POS      = MON_FROM_OLD ? OLD_POS - MON_FROM_OLD->m_position + THIS_MON_POS : OLD_POS;

        // put around the current center, fit in workArea
        target->setPositionGlobal(fitBoxInWorkArea(CBox{NEW_POS, LAST_SIZE}, target));
    }
}

CBox CDefaultFloatingAlgorithm::fitBoxInWorkArea(const CBox& box, SP<ITarget> t) {
    const auto WORK_AREA = m_parent->space()->workArea(true);
    const auto EXTENTS   = t->window() ? t->window()->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS) : SBoxExtents{};
    CBox       targetBox = box.copy().addExtents(EXTENTS);

    targetBox.x = std::max(targetBox.x, WORK_AREA.x);
    targetBox.y = std::max(targetBox.y, WORK_AREA.y);

    if (targetBox.x + targetBox.w > WORK_AREA.x + WORK_AREA.w)
        targetBox.x = WORK_AREA.x + WORK_AREA.w - targetBox.w;

    if (targetBox.y + targetBox.h > WORK_AREA.y + WORK_AREA.h)
        targetBox.y = WORK_AREA.y + WORK_AREA.h - targetBox.h;

    return targetBox.addExtents(SBoxExtents{.topLeft = -EXTENTS.topLeft, .bottomRight = -EXTENTS.bottomRight});
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

void CDefaultFloatingAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    auto       pos  = t->position();
    auto       work = m_parent->space()->workArea(true);

    const auto EXTENTS = t->window() ? t->window()->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS) : SBoxExtents{};

    switch (dir) {
        case Math::DIRECTION_LEFT: pos.x = work.x + EXTENTS.topLeft.x; break;
        case Math::DIRECTION_RIGHT: pos.x = work.x + work.w - pos.w - EXTENTS.bottomRight.x; break;
        case Math::DIRECTION_UP: pos.y = work.y + EXTENTS.topLeft.y; break;
        case Math::DIRECTION_DOWN: pos.y = work.y + work.h - pos.h - EXTENTS.bottomRight.y; break;
        default: Log::logger->log(Log::ERR, "Invalid direction in CDefaultFloatingAlgorithm::moveTargetInDirection"); break;
    }

    t->setPositionGlobal(pos);
}
