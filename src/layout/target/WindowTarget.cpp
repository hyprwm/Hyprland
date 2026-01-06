#include "WindowTarget.hpp"

#include "../space/Space.hpp"

#include "../../protocols/core/Compositor.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../xwayland/XSurface.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"

using namespace Layout;

SP<CWindowTarget> CWindowTarget::create(PHLWINDOW w) {
    auto target    = SP<CWindowTarget>(new CWindowTarget(w));
    target->m_self = target;
    return target;
}

CWindowTarget::CWindowTarget(PHLWINDOW w) : m_window(w) {
    ;
}

eTargetType CWindowTarget::type() {
    return TARGET_TYPE_WINDOW;
}

void CWindowTarget::setPositionGlobal(const CBox& box) {
    ITarget::setPositionGlobal(box);

    updatePos();
}

void CWindowTarget::updatePos() {
    m_window->m_position = m_box.pos();
    m_window->m_size     = m_box.size();

    *m_window->m_realPosition = m_box.pos();
    *m_window->m_realSize     = m_box.size();

    m_window->sendWindowSize();
}

void CWindowTarget::assignToSpace(const SP<CSpace>& space) {
    ITarget::assignToSpace(space);

    if (!space)
        return;

    m_window->m_monitor = space->workspace()->m_monitor;

    m_window->moveToWorkspace(space->workspace());
    m_window->updateToplevel();
    m_window->updateWindowDecos();
}

bool CWindowTarget::floating() {
    return m_window->m_isFloating;
}

void CWindowTarget::setFloating(bool x) {
    m_window->m_isFloating = x;
}

std::expected<CBox, eGeometryFailure> CWindowTarget::desiredGeometry() {
    CBox       DESIRED_GEOM = g_pXWaylandManager->getGeometryForWindow(m_window.lock());
    const auto PMONITOR     = m_window->m_monitor.lock();
    const auto MONITOR_BOX  = PMONITOR->logicalBox();

    if (m_window->m_isX11) {
        Vector2D xy    = {DESIRED_GEOM.x, DESIRED_GEOM.y};
        xy             = g_pXWaylandManager->xwaylandToWaylandCoords(xy);
        DESIRED_GEOM.x = xy.x;
        DESIRED_GEOM.y = xy.y;
    } else if (m_window->m_ruleApplicator->persistentSize().valueOrDefault()) {
        DESIRED_GEOM.w = m_window->m_lastFloatingSize.x;
        DESIRED_GEOM.h = m_window->m_lastFloatingSize.y;
    }

    if (!PMONITOR) {
        Log::logger->log(Log::ERR, "{:m} has an invalid monitor in desiredGeometry!", m_window.lock());
        return std::unexpected(GEOMETRY_NO_DESIRED);
    }

    if (DESIRED_GEOM.width <= 2 || DESIRED_GEOM.height <= 2) {
        const auto SURFACE = m_window->wlSurface()->resource();

        if (SURFACE->m_current.size.x > 5 && SURFACE->m_current.size.y > 5) {
            const auto DESIRED_SIZE = SURFACE->m_current.size;

            // center on mon and call it a day
            return CBox{MONITOR_BOX.middle() - (DESIRED_SIZE / 2.F), DESIRED_SIZE};
        }

        if (m_window->m_isX11 && m_window->isX11OverrideRedirect()) {
            // check OR windows, they like their shit
            const auto SIZE = m_window->m_xwaylandSurface->m_geometry.w > 0 && m_window->m_xwaylandSurface->m_geometry.h > 0 ? m_window->m_xwaylandSurface->m_geometry.size() :
                                                                                                                               Vector2D{600, 400};

            if (m_window->m_xwaylandSurface->m_geometry.x != 0 && m_window->m_xwaylandSurface->m_geometry.y != 0)
                return CBox{g_pXWaylandManager->xwaylandToWaylandCoords(m_window->m_xwaylandSurface->m_geometry.pos()), SIZE};
        }

        return std::unexpected(m_window->m_isX11 && m_window->isX11OverrideRedirect() ? GEOMETRY_INVALID_DESIRED : GEOMETRY_NO_DESIRED);
    }

    // TODO: detect a popup in a more consistent way.
    if ((DESIRED_GEOM.x == 0 && DESIRED_GEOM.y == 0) || !m_window->m_isX11) {
        // if the pos isn't set, fall back to the center placement if it's not a child
        const auto CENTER_POS = MONITOR_BOX.middle() - DESIRED_GEOM.size() / 2.F;
        DESIRED_GEOM.x        = CENTER_POS.x;
        DESIRED_GEOM.y        = CENTER_POS.y;

        // otherwise middle of parent if available
        if (!m_window->m_isX11) {
            if (const auto PARENT = m_window->parent(); PARENT) {
                const auto POS = PARENT->m_realPosition->goal() + PARENT->m_realSize->goal() / 2.F - DESIRED_GEOM.size() / 2.F;
                DESIRED_GEOM.x = POS.x;
                DESIRED_GEOM.y = POS.y;
            }
        }
    } else {
        // if it is, we respect where it wants to put itself, but apply monitor offset if outside
        // most of these are popups

        Vector2D pos;

        if (const auto POPENMON = g_pCompositor->getMonitorFromVector(DESIRED_GEOM.middle()); POPENMON->m_id != PMONITOR->m_id)
            pos = Vector2D(DESIRED_GEOM.x, DESIRED_GEOM.y) - POPENMON->m_position + PMONITOR->m_position;
        else
            pos = Vector2D(DESIRED_GEOM.x, DESIRED_GEOM.y);
    }

    if (DESIRED_GEOM.w <= 2 || DESIRED_GEOM.h <= 2)
        return std::unexpected(GEOMETRY_NO_DESIRED);

    return DESIRED_GEOM;
}

PHLWINDOW CWindowTarget::window() const {
    return m_window.lock();
}

eFullscreenMode CWindowTarget::fullscreenMode() {
    return m_window->m_fullscreenState.internal;
}

void CWindowTarget::setFullscreenMode(eFullscreenMode mode) {
    m_window->m_fullscreenState.internal = mode;
}

std::optional<Vector2D> CWindowTarget::minSize() {
    return m_window->minSize();
}

std::optional<Vector2D> CWindowTarget::maxSize() {
    return m_window->maxSize();
}

void CWindowTarget::damageEntire() {
    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CWindowTarget::warpPositionSize() {
    m_window->m_realSize->warp();
    m_window->m_realPosition->warp();
}
