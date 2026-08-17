#include "WindowTarget.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"

#include "../space/Space.hpp"
#include "../algorithm/Algorithm.hpp"

#include "../../protocols/core/Compositor.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"
#include "../../desktop/state/FloatState.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../state/MonitorState.hpp"
#include "../../desktop/Workspace.hpp"
#include "../../desktop/view/window/WindowGroupMembership.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../desktop/view/Group.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../helpers/math/Expression.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::Utils;
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

void CWindowTarget::setPositionGlobal(const STargetBox& box, uint8_t flags) {
    ITarget::setPositionGlobal(box, flags);

    updatePos(flags);
}

void CWindowTarget::updatePos(uint8_t flags) {

    if (!m_window)
        return;

    const auto effectiveWindow = [&]() {
        // FS state of a window in a group is owned by the current window of that group.
        return window()->grouping().group() ? window()->grouping().group()->current() : window();
    };

    g_pHyprRenderer->damageWindow(m_window.lock());
    CScopeGuard x([this] { g_pHyprRenderer->damageWindow(m_window.lock()); });

    const bool  CONFIGURECLIENT = !(flags & TARGET_UPDATE_NO_CLIENT_CONFIGURE);

    if (!m_space || !m_space->workspace())
        return;

    const auto PMONITOR         = m_space->workspace()->m_monitor;
    const auto PWORKSPACE       = m_space->workspace();
    const auto MONITOR_WORKAREA = m_space->workArea();

    // get specific gaps and rules for this workspace,
    // if user specified them in config
    const auto WORKSPACERULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(PWORKSPACE);

    if (!validMapped(m_window)) {
        if (m_window)
            g_layoutManager->removeTarget(m_window->layoutTarget());
        return;
    }

    // Non-FS Floating Windows
    if (floating() && m_window && !Fullscreen::controller()->isFullscreen(effectiveWindow())) {
        m_window->setBox(m_box.logicalBox);

        if (CONFIGURECLIENT)
            sendWindowSize();
        m_window->presentation().updateDecorations();

        return;
    }

    /* FS Handling */

    // prevent re-setting a covering FS window's pos after it is set by the FS calls
    if (m_window && Fullscreen::controller()->isFullscreen(effectiveWindow(), std::nullopt, true)) {
        if (!Fullscreen::controller()->m_windowPosSettingQueued)
            return;
    }

    // Default Handled FS (floating or tiling)
    if (const auto FSMODES = Fullscreen::controller()->getFullscreenModes(effectiveWindow());
        FSMODES.internal != Fullscreen::FSMODE_NONE && !Fullscreen::controller()->layoutManagedFS(effectiveWindow())) {
        if (FSMODES.internal == Fullscreen::FSMODE_FULLSCREEN) {
            m_window->setBox(m_box.logicalBox);

        } else if (FSMODES.internal == Fullscreen::FSMODE_MAXIMIZED) {
            CBox nodeBox   = m_box.logicalBox;
            CBox visualBox = m_box.visualBox.empty() ? nodeBox : m_box.visualBox;
            nodeBox.round();
            visualBox.round();

            // Reserved area must be updated before this is called
            // Reserved area for all windows in a group are owned by the leading window. Other windows are hidden anyway so this simply ensures their sizes are uniform when FSed
            const auto RESERVED = effectiveWindow()->getFullWindowReservedArea();

            m_window->setBox({visualBox.pos() + RESERVED.topLeft, visualBox.size() - (RESERVED.topLeft + RESERVED.bottomRight)});
        }

        m_window->presentation().updateDecorations();
        if (CONFIGURECLIENT)
            sendWindowSize();
        return;
    }

    // Layout handled FS (Tiled Only)
    if (const auto FSMODES = Fullscreen::controller()->getFullscreenModes(effectiveWindow());
        FSMODES.internal != Fullscreen::FSMODE_NONE && Fullscreen::controller()->layoutManagedFS(effectiveWindow())) {

        CBox nodeBox   = m_box.logicalBox;
        CBox visualBox = m_box.visualBox.empty() ? nodeBox : m_box.visualBox;
        nodeBox.round();
        visualBox.round();
        if (FSMODES.internal == Fullscreen::FSMODE_FULLSCREEN) {
            m_window->setBox(visualBox);
        } else if (FSMODES.internal == Fullscreen::FSMODE_MAXIMIZED) {

            // Reserved area must be updated before this is called
            // Reserved area for all windows in a group are owned by the leading window. Other windows are hidden anyway so this simply ensures their sizes are uniform when FSed
            const auto RESERVED = effectiveWindow()->getFullWindowReservedArea();
            m_window->setBox({visualBox.pos() + RESERVED.topLeft, visualBox.size() - (RESERVED.topLeft + RESERVED.bottomRight)});
        }

        m_window->presentation().updateDecorations();
        if (CONFIGURECLIENT)
            sendWindowSize();
        return;
    }

    /* Non-Fs Tiled Windows */

    g_pHyprRenderer->damageWindow(window());

    CBox nodeBox = m_box.logicalBox;
    nodeBox.round();

    auto calcPos  = m_box.visualBox.pos();
    auto calcSize = m_box.visualBox.size();

    if (m_box.visualBox.empty()) {
        calcPos  = nodeBox.pos();
        calcSize = nodeBox.size();
        // for gaps outer
        const bool DISPLAYLEFT   = STICKS(m_box.logicalBox.x, MONITOR_WORKAREA.x);
        const bool DISPLAYRIGHT  = STICKS(m_box.logicalBox.x + m_box.logicalBox.w, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w);
        const bool DISPLAYTOP    = STICKS(m_box.logicalBox.y, MONITOR_WORKAREA.y);
        const bool DISPLAYBOTTOM = STICKS(m_box.logicalBox.y + m_box.logicalBox.h, MONITOR_WORKAREA.y + MONITOR_WORKAREA.h);

        // this is used for scrolling, so that the gaps are correct when a window is the full width and has neighbors
        const bool        DISPLAYINVERSELEFT  = STICKS(m_box.logicalBox.x, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w);
        const bool        DISPLAYINVERSERIGHT = STICKS(m_box.logicalBox.x + m_box.logicalBox.w, MONITOR_WORKAREA.x);

        static auto       PGAPSINDATA = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
        auto* const       PGAPSIN     = sc<Config::CCssGapData*>((PGAPSINDATA.ptr()));
        auto              gapsIn      = (WORKSPACERULE && WORKSPACERULE->m_gapsIn.has_value()) ? WORKSPACERULE->m_gapsIn.value() : *PGAPSIN;

        const static auto REQUESTEDRATIO          = CConfigValue<Config::VEC2>("layout:single_window_aspect_ratio");
        const static auto REQUESTEDRATIOTOLERANCE = CConfigValue<Config::FLOAT>("layout:single_window_aspect_ratio_tolerance");

        Vector2D          ratioPadding;

        if ((*REQUESTEDRATIO).y != 0 && m_space->algorithm()->tiledTargets() <= 1 && m_window) {
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
    }

    if (isPseudo() && m_window) {
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

    static auto PCLAMP_TILED = CConfigValue<Config::INTEGER>("misc:size_limits_tiled");

    if (*PCLAMP_TILED) {
        Vector2D minSize = m_window->m_ruleApplicator->minSize().valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE});
        Vector2D maxSize = m_window->m_ruleApplicator->maxSize().valueOr(Vector2D{INFINITY, INFINITY});
        calcSize         = calcSize.clamp(minSize, maxSize);

        calcPos += (availableSpace - calcSize) / 2.0;

        calcPos.x = std::clamp(calcPos.x, MONITOR_WORKAREA.x, std::max(MONITOR_WORKAREA.x, MONITOR_WORKAREA.x + MONITOR_WORKAREA.w - calcSize.x));
        calcPos.y = std::clamp(calcPos.y, MONITOR_WORKAREA.y, std::max(MONITOR_WORKAREA.y, MONITOR_WORKAREA.y + MONITOR_WORKAREA.h - calcSize.y));
    }

    CBox wb = {calcPos, calcSize};
    wb.round(); // avoid rounding mess

    m_window->setBox(wb);

    m_window->presentation().updateDecorations();
    if (CONFIGURECLIENT)
        sendWindowSize();
}

void CWindowTarget::assignToSpace(const SP<CSpace>& space, std::optional<Vector2D> focalPoint) {
    if (!space) {
        ITarget::assignToSpace(space, focalPoint);
        return;
    }

    m_cantLockCursor = false;

    // keep the ref here so that moveToWorkspace doesn't unref the workspace
    // and assignToSpace doesn't think this is a new target because space wp is dead
    const auto WSREF = space->workspace();

    m_window->m_monitor = space->workspace()->m_monitor;
    m_window->moveToWorkspace(space->workspace());

    // layout and various update fns want the target to already have m_workspace set
    ITarget::assignToSpace(space, focalPoint);

    m_window->updateToplevel();
    m_window->presentation().updateDecorations();
}

bool CWindowTarget::floating() {
    return m_floating;
}

void CWindowTarget::setFloating(bool x) {
    if (x == m_floating)
        return;

    m_cantLockCursor = false;

    m_floating = x;
    m_window->m_state &= ~Desktop::View::WINDOW_STATE_PINNED;

    m_window->m_ruleApplicator->propertiesChanged(Desktop::Rule::RULE_PROP_FLOATING);
}

void CWindowTarget::setFloatingInitial(bool x) {
    m_floating = x;
}

bool CWindowTarget::cantLockCursor() const {
    return m_cantLockCursor;
}

void CWindowTarget::setCantLockCursor(bool x) {
    m_cantLockCursor = x;
}

Vector2D CWindowTarget::clampSizeForDesired(const Vector2D& size) {
    Vector2D newSize = size;
    if (const auto m = minSize(); m)
        newSize = newSize.clamp(*m);
    if (const auto m = maxSize(); m)
        newSize = newSize.clamp(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}, *m);
    return newSize;
}

std::expected<SGeometryRequested, eGeometryFailure> CWindowTarget::desiredGeometry() {

    SGeometryRequested requested;

    const auto         CLIENT_GEOMETRY = m_window->backend().geometry();
    CBox               DESIRED_GEOM    = CLIENT_GEOMETRY.box;
    const auto         PMONITOR        = m_window->m_monitor.lock();

    requested.size = clampSizeForDesired(DESIRED_GEOM.size());

    if (m_window->backend().isX11())
        requested.pos = DESIRED_GEOM.pos() + (DESIRED_GEOM.size() - requested.size) / 2.F;

    const auto STOREDSIZE = m_window->m_ruleApplicator->persistentSize().valueOrDefault() ? Desktop::floatState()->get(m_window.lock()) : std::nullopt;

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
            requested.size = clampSizeForDesired(m_window->backend().clientToLogical(CBox{{}, SURFACE->m_current.size}, PMONITOR).size());
            return requested;
        }

        const bool X11_OVERRIDE_REDIRECT = m_window->backend().isX11() && m_window->backend().traits().overrideRedirect;
        if (X11_OVERRIDE_REDIRECT) {
            // check OR windows, they like their shit
            const auto CLIENT_BOX  = m_window->backend().logicalToClient(CLIENT_GEOMETRY.box, PMONITOR);
            const auto CLIENT_SIZE = CLIENT_BOX.w > 0 && CLIENT_BOX.h > 0 ? CLIENT_BOX.size() : Vector2D{600, 400};
            const auto SIZE        = clampSizeForDesired(m_window->backend().clientToLogical(CBox{{}, CLIENT_SIZE}, PMONITOR).size());

            if (CLIENT_BOX.x != 0 && CLIENT_BOX.y != 0) {
                requested.size = SIZE;
                requested.pos  = CLIENT_GEOMETRY.box.pos();
                return requested;
            }
        }

        return std::unexpected(X11_OVERRIDE_REDIRECT ? GEOMETRY_INVALID_DESIRED : GEOMETRY_NO_DESIRED);
    }

    // TODO: detect a popup in a more consistent way.
    if ((DESIRED_GEOM.x == 0 && DESIRED_GEOM.y == 0) || !m_window->backend().isX11()) {
        // middle of parent if available
        if (!m_window->backend().isX11()) {
            if (const auto PARENT = m_window->backend().parent(); PARENT) {
                const auto POS = PARENT->position(Desktop::View::IGeometric::GEOMETRIC_GOAL) + PARENT->size(Desktop::View::IGeometric::GEOMETRIC_GOAL) / 2.F - requested.size / 2.F;
                requested.pos  = POS;
            }
        }
    } else {
        // if it is, we respect where it wants to put itself, but apply monitor offset if outside
        // most of these are popups

        Vector2D pos;

        if (const auto POPENMON = State::monitorState()->query().vec(DESIRED_GEOM.middle()).run(); POPENMON->m_id != PMONITOR->m_id)
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

std::optional<Vector2D> CWindowTarget::minSize() {
    if (m_window->m_ruleApplicator->minSize().hasValue())
        return m_window->m_ruleApplicator->minSize().value();

    return m_window->backend().geometryHints(Desktop::View::eBackendState::BACKEND_STATE_CURRENT).minSize;
}

std::optional<Vector2D> CWindowTarget::maxSize() {
    if (m_window->m_ruleApplicator->maxSize().hasValue())
        return m_window->m_ruleApplicator->maxSize().value();

    if (m_window->m_ruleApplicator->noMaxSize().valueOrDefault())
        return std::nullopt;

    return m_window->backend().geometryHints(Desktop::View::eBackendState::BACKEND_STATE_CURRENT).maxSize;
}

bool CWindowTarget::clampWindowSize(const std::optional<Vector2D> minSize, const std::optional<Vector2D> maxSize) {
    const Vector2D REALSIZE = m_window->size(Desktop::View::IGeometric::GEOMETRIC_GOAL);
    const Vector2D MAX      = Fullscreen::controller()->isFullscreen(m_window.lock()) ? Vector2D{INFINITY, INFINITY} : maxSize.value_or(Vector2D{INFINITY, INFINITY});
    const Vector2D NEWSIZE  = REALSIZE.clamp(minSize.value_or(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}), MAX);
    const bool     changed  = !(NEWSIZE == REALSIZE);

    if (changed) {
        const Vector2D DELTA = REALSIZE - NEWSIZE;
        m_window->layoutTarget()->setPositionGlobal(CBox{m_window->position(Desktop::View::IGeometric::GEOMETRIC_GOAL) + DELTA / 2.0, NEWSIZE});
    }

    return changed;
}

std::optional<double> CWindowTarget::calculateSingleExpr(const std::string& s) {
    const auto        PMONITOR     = m_window->m_monitor ? m_window->m_monitor : Desktop::focusState()->monitor();
    const auto        CURSOR_LOCAL = g_pInputManager->getMouseCoordsInternal() - (PMONITOR ? PMONITOR->m_position : Vector2D{});

    Math::CExpression expr;
    expr.addVariable("window_w", m_window->size(Desktop::View::IGeometric::GEOMETRIC_GOAL).x);
    expr.addVariable("window_h", m_window->size(Desktop::View::IGeometric::GEOMETRIC_GOAL).y);
    expr.addVariable("window_x", m_window->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).x - (PMONITOR ? PMONITOR->m_position.x : 0));
    expr.addVariable("window_y", m_window->position(Desktop::View::IGeometric::GEOMETRIC_GOAL).y - (PMONITOR ? PMONITOR->m_position.y : 0));

    expr.addVariable("monitor_w", PMONITOR ? PMONITOR->m_size.x : 1920);
    expr.addVariable("monitor_h", PMONITOR ? PMONITOR->m_size.y : 1080);

    expr.addVariable("cursor_x", CURSOR_LOCAL.x);
    expr.addVariable("cursor_y", CURSOR_LOCAL.y);

    return expr.compute(s);
}

std::optional<Vector2D> CWindowTarget::calculateExpression(const Math::SExpressionVec2& expr) {
    const auto LHS = calculateSingleExpr(expr.x);
    const auto RHS = calculateSingleExpr(expr.y);

    if (!LHS || !RHS)
        return std::nullopt;

    return Vector2D{*LHS, *RHS};
}

void CWindowTarget::sendWindowSize(bool force) {
    const auto PMONITOR = m_window->m_monitor.lock();

    Log::logger->log(Log::TRACE, "sendWindowSize: window:{:x},title:{} with real pos {}, real size {} (force: {})", rc<uintptr_t>(m_window.get()), m_window->metadata().title(),
                     m_window->position(Desktop::View::IGeometric::GEOMETRIC_GOAL), m_window->size(Desktop::View::IGeometric::GEOMETRIC_GOAL), force);

    m_window->backend().configure(CBox{m_window->position(Desktop::View::IGeometric::GEOMETRIC_GOAL), m_window->size(Desktop::View::IGeometric::GEOMETRIC_GOAL)}, PMONITOR, force);
}

void CWindowTarget::damageEntire() {
    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CWindowTarget::warpPositionSize() {
    m_window->finishAnimation();
    m_window->presentation().updateDecorations();
}

void CWindowTarget::onUpdateSpace() {
    if (!space())
        return;

    m_window->m_monitor = space()->workspace()->m_monitor;
    m_window->moveToWorkspace(space()->workspace());
    m_window->updateToplevel();
    m_window->updateWindowData();
    m_window->presentation().updateDecorations();
}
