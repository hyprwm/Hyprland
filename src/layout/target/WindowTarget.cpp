#include "WindowTarget.hpp"

#include "../space/Space.hpp"
#include "../algorithm/Algorithm.hpp"

#include "../../protocols/core/Compositor.hpp"
#include "../../config/ConfigManager.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../xwayland/XSurface.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"

using namespace Layout;

SP<ITarget> CWindowTarget::create(PHLWINDOW w) {
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

    if (!m_space)
        return;

    if (fullscreenMode() == FSMODE_FULLSCREEN)
        return;

    if (floating() && fullscreenMode() != FSMODE_MAXIMIZED) {
        m_window->m_position = m_box.pos();
        m_window->m_size     = m_box.size();

        *m_window->m_realPosition = m_box.pos();
        *m_window->m_realSize     = m_box.size();

        m_window->sendWindowSize();
        m_window->updateWindowDecos();

        return;
    }

    // Tiled is more complicated.

    const auto PMONITOR   = m_space->workspace()->m_monitor;
    const auto PWORKSPACE = m_space->workspace();

    // for gaps outer
    const auto MONITOR_WORKAREA = m_space->workArea();
    const bool DISPLAYLEFT      = STICKS(m_box.x, MONITOR_WORKAREA.x);
    const bool DISPLAYRIGHT     = STICKS(m_box.x + m_box.w, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w);
    const bool DISPLAYTOP       = STICKS(m_box.y, MONITOR_WORKAREA.y);
    const bool DISPLAYBOTTOM    = STICKS(m_box.y + m_box.h, MONITOR_WORKAREA.y + MONITOR_WORKAREA.h);

    // this is used for scrolling, so that the gaps are correct when a window is the full width and has neighbors
    const bool DISPLAYINVERSELEFT  = STICKS(m_box.x, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w);
    const bool DISPLAYINVERSERIGHT = STICKS(m_box.x + m_box.w, MONITOR_WORKAREA.x);

    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(PWORKSPACE);

    if (!validMapped(m_window)) {
        if (m_window)
            g_layoutManager->removeTarget(m_window->layoutTarget());
        return;
    }

    if (fullscreenMode() == FSMODE_FULLSCREEN)
        return;

    g_pHyprRenderer->damageWindow(window());

    static auto PGAPSINDATA = CConfigValue<Hyprlang::CUSTOMTYPE>("general:gaps_in");
    auto* const PGAPSIN     = sc<CCssGapData*>((PGAPSINDATA.ptr())->getData());

    auto        gapsIn  = WORKSPACERULE.gapsIn.value_or(*PGAPSIN);
    CBox        nodeBox = m_box;
    nodeBox.round();

    m_window->m_size     = nodeBox.size();
    m_window->m_position = nodeBox.pos();

    m_window->updateWindowDecos();

    auto              calcPos  = m_window->m_position;
    auto              calcSize = m_window->m_size;

    const static auto REQUESTEDRATIO          = CConfigValue<Hyprlang::VEC2>("layout:single_window_aspect_ratio");
    const static auto REQUESTEDRATIOTOLERANCE = CConfigValue<Hyprlang::FLOAT>("layout:single_window_aspect_ratio_tolerance");

    Vector2D          ratioPadding;

    if ((*REQUESTEDRATIO).y != 0 && m_space->algorithm()->tiledTargets() <= 1) {
        const Vector2D originalSize = MONITOR_WORKAREA.size();

        const double   requestedRatio = (*REQUESTEDRATIO).x / (*REQUESTEDRATIO).y;
        const double   originalRatio  = originalSize.x / originalSize.y;

        if (requestedRatio > originalRatio) {
            double padding = originalSize.y - (originalSize.x / requestedRatio);

            if (padding / 2 > (*REQUESTEDRATIOTOLERANCE) * originalSize.y)
                ratioPadding = Vector2D{0., padding};
        } else if (requestedRatio < originalRatio) {
            double padding = originalSize.x - (originalSize.y * requestedRatio);

            if (padding / 2 > (*REQUESTEDRATIOTOLERANCE) * originalSize.x)
                ratioPadding = Vector2D{padding, 0.};
        }
    }

    const auto GAPOFFSETTOPLEFT = Vector2D(sc<double>(DISPLAYLEFT ? 0 : (DISPLAYINVERSELEFT ? 2 * gapsIn.m_left : gapsIn.m_left)), sc<double>(DISPLAYTOP ? 0 : gapsIn.m_top));

    const auto GAPOFFSETBOTTOMRIGHT =
        Vector2D(sc<double>(DISPLAYRIGHT ? 0 : (DISPLAYINVERSERIGHT ? 2 * gapsIn.m_right : gapsIn.m_right)), sc<double>(DISPLAYBOTTOM ? 0 : gapsIn.m_bottom));

    calcPos  = calcPos + GAPOFFSETTOPLEFT + ratioPadding / 2;
    calcSize = calcSize - GAPOFFSETTOPLEFT - GAPOFFSETBOTTOMRIGHT - ratioPadding;

    if (isPseudo()) {
        // Calculate pseudo
        float scale = 1;

        // adjust if doesn't fit
        if (m_pseudoSize.x > calcSize.x || m_pseudoSize.y > calcSize.y) {
            if (m_pseudoSize.x > calcSize.x)
                scale = calcSize.x / m_pseudoSize.x;

            if (m_pseudoSize.y * scale > calcSize.y)
                scale = calcSize.y / m_pseudoSize.y;

            auto DELTA = calcSize - m_pseudoSize * scale;
            calcSize   = m_pseudoSize * scale;
            calcPos    = calcPos + DELTA / 2.f; // center
        } else {
            auto DELTA = calcSize - m_pseudoSize;
            calcPos    = calcPos + DELTA / 2.f; // center
            calcSize   = m_pseudoSize;
        }
    }

    const auto RESERVED = m_window->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    Vector2D    availableSpace = calcSize;

    static auto PCLAMP_TILED = CConfigValue<Hyprlang::INT>("misc:size_limits_tiled");

    if (*PCLAMP_TILED) {
        const auto borderSize       = m_window->getRealBorderSize();
        Vector2D   monitorAvailable = MONITOR_WORKAREA.size() - Vector2D{2.0 * borderSize, 2.0 * borderSize};

        Vector2D   minSize = m_window->m_ruleApplicator->minSize().valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}).clamp(Vector2D{0, 0}, monitorAvailable);
        Vector2D   maxSize = m_window->isFullscreen() ? Vector2D{INFINITY, INFINITY} :
                                                        m_window->m_ruleApplicator->maxSize().valueOr(Vector2D{INFINITY, INFINITY}).clamp(Vector2D{0, 0}, monitorAvailable);
        calcSize           = calcSize.clamp(minSize, maxSize);

        calcPos += (availableSpace - calcSize) / 2.0;

        calcPos.x = std::clamp(calcPos.x, MONITOR_WORKAREA.x + borderSize, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w - calcSize.x - borderSize);
        calcPos.y = std::clamp(calcPos.y, MONITOR_WORKAREA.y + borderSize, MONITOR_WORKAREA.y + MONITOR_WORKAREA.h - calcSize.y - borderSize);
    }

    if (m_window->onSpecialWorkspace() && !m_window->isFullscreen()) {
        // if special, we adjust the coords a bit
        static auto PSCALEFACTOR = CConfigValue<Hyprlang::FLOAT>("dwindle:special_scale_factor");

        CBox        wb = {calcPos + (calcSize - calcSize * *PSCALEFACTOR) / 2.f, calcSize * *PSCALEFACTOR};
        wb.round(); // avoid rounding mess

        *m_window->m_realPosition = wb.pos();
        *m_window->m_realSize     = wb.size();
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round(); // avoid rounding mess

        *m_window->m_realSize     = wb.size();
        *m_window->m_realPosition = wb.pos();
    }

    m_window->updateWindowDecos();
}

void CWindowTarget::assignToSpace(const SP<CSpace>& space, std::optional<Vector2D> focalPoint) {
    if (!space) {
        ITarget::assignToSpace(space, focalPoint);
        return;
    }

    // keep the ref here so that moveToWorkspace doesn't unref the workspace
    // and assignToSpace doesn't think this is a new target because space wp is dead
    const auto WSREF = space->workspace();

    m_window->m_monitor = space->workspace()->m_monitor;
    m_window->moveToWorkspace(space->workspace());

    // layout and various update fns want the target to already have m_workspace set
    ITarget::assignToSpace(space, focalPoint);

    m_window->updateToplevel();
    m_window->updateWindowDecos();
}

bool CWindowTarget::floating() {
    return m_window->m_isFloating;
}

void CWindowTarget::setFloating(bool x) {
    if (x == m_window->m_isFloating)
        return;

    m_window->m_isFloating = x;
    m_window->m_pinned     = false;

    m_window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FLOATING);
}

Vector2D CWindowTarget::clampSizeForDesired(const Vector2D& size) const {
    Vector2D newSize = size;
    if (const auto m = m_window->minSize(); m)
        newSize = newSize.clamp(*m);
    if (const auto m = m_window->maxSize(); m)
        newSize = newSize.clamp(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}, *m);
    return newSize;
}

std::expected<SGeometryRequested, eGeometryFailure> CWindowTarget::desiredGeometry() {

    SGeometryRequested requested;

    CBox               DESIRED_GEOM = g_pXWaylandManager->getGeometryForWindow(m_window.lock());
    const auto         PMONITOR     = m_window->m_monitor.lock();

    requested.size = clampSizeForDesired(DESIRED_GEOM.size());

    if (m_window->m_isX11) {
        Vector2D xy   = {DESIRED_GEOM.x, DESIRED_GEOM.y};
        xy            = g_pXWaylandManager->xwaylandToWaylandCoords(xy);
        requested.pos = xy;
    }

    const auto STOREDSIZE = m_window->m_ruleApplicator->persistentSize().valueOrDefault() ? g_pConfigManager->getStoredFloatingSize(m_window.lock()) : std::nullopt;

    if (STOREDSIZE)
        requested.size = clampSizeForDesired(*STOREDSIZE);

    if (!PMONITOR) {
        Log::logger->log(Log::ERR, "{:m} has an invalid monitor in desiredGeometry!", m_window.lock());
        return std::unexpected(GEOMETRY_NO_DESIRED);
    }

    if (DESIRED_GEOM.width <= 2 || DESIRED_GEOM.height <= 2) {
        const auto SURFACE = m_window->wlSurface()->resource();

        if (SURFACE->m_current.size.x > 5 && SURFACE->m_current.size.y > 5) {
            // center on mon and call it a day
            requested.pos.reset();
            requested.size = clampSizeForDesired(SURFACE->m_current.size);
            return requested;
        }

        if (m_window->m_isX11 && m_window->isX11OverrideRedirect()) {
            // check OR windows, they like their shit
            const auto SIZE = clampSizeForDesired(m_window->m_xwaylandSurface->m_geometry.w > 0 && m_window->m_xwaylandSurface->m_geometry.h > 0 ?
                                                      m_window->m_xwaylandSurface->m_geometry.size() :
                                                      Vector2D{600, 400});

            if (m_window->m_xwaylandSurface->m_geometry.x != 0 && m_window->m_xwaylandSurface->m_geometry.y != 0) {
                requested.size = SIZE;
                requested.pos  = g_pXWaylandManager->xwaylandToWaylandCoords(m_window->m_xwaylandSurface->m_geometry.pos());
                return requested;
            }
        }

        return std::unexpected(m_window->m_isX11 && m_window->isX11OverrideRedirect() ? GEOMETRY_INVALID_DESIRED : GEOMETRY_NO_DESIRED);
    }

    // TODO: detect a popup in a more consistent way.
    if ((DESIRED_GEOM.x == 0 && DESIRED_GEOM.y == 0) || !m_window->m_isX11) {
        // middle of parent if available
        if (!m_window->m_isX11) {
            if (const auto PARENT = m_window->parent(); PARENT) {
                const auto POS = PARENT->m_realPosition->goal() + PARENT->m_realSize->goal() / 2.F - DESIRED_GEOM.size() / 2.F;
                requested.pos  = POS;
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

        requested.pos = pos;
    }

    if (DESIRED_GEOM.w <= 2 || DESIRED_GEOM.h <= 2)
        return std::unexpected(GEOMETRY_NO_DESIRED);

    return requested;
}

PHLWINDOW CWindowTarget::window() const {
    return m_window.lock();
}

eFullscreenMode CWindowTarget::fullscreenMode() {
    return m_window->m_fullscreenState.internal;
}

void CWindowTarget::setFullscreenMode(eFullscreenMode mode) {
    if (floating() && m_window->m_fullscreenState.internal == FSMODE_NONE)
        rememberFloatingSize(m_box.size());

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
    m_window->updateWindowDecos();
}

void CWindowTarget::onUpdateSpace() {
    if (!space())
        return;

    m_window->m_monitor = space()->workspace()->m_monitor;
    m_window->moveToWorkspace(space()->workspace());
    m_window->updateToplevel();
    m_window->updateWindowDecos();
}
