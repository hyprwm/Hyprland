#include "WindowQuery.hpp"
#include "WindowState.hpp"
#include "../Workspace.hpp"
#include "../history/WindowHistoryTracker.hpp"
#include "../view/Window.hpp"
#include "../../config/ConfigValue.hpp"
#include "../../output/Monitor.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <ranges>
#include <unordered_map>

using namespace Desktop;
using namespace Desktop::View;

CWindowQuery::CWindowQuery(const CWindowState& state) : m_state(state) {
    ;
}

PHLWINDOW CWindowQuery::inDirection(PHLWINDOW window, Math::eDirection direction) const {
    if (direction == Math::DIRECTION_DEFAULT)
        return nullptr;

    if (!window)
        return nullptr;

    const auto PMONITOR = window->m_monitor.lock();

    if (!PMONITOR)
        return nullptr; // ??

    const auto WINDOWIDEALBB = window->isFullscreen() ? CBox{PMONITOR->m_position, PMONITOR->m_size} : window->getWindowIdealBoundingBoxIgnoreReserved();
    const auto PWORKSPACE    = window->m_workspace;

    if (!PWORKSPACE)
        return nullptr; // ??

    return inDirection({.origin             = WINDOWIDEALBB,
                        .workspace          = PWORKSPACE,
                        .direction          = direction,
                        .floatingPreference = window->m_isFloating,
                        .ignoreWindow       = window,
                        .useVectorAngles    = window->m_isFloating});
}

PHLWINDOW CWindowQuery::inDirection(const SWindowDirectionQuery& query) const {
    if (query.direction == Math::DIRECTION_DEFAULT)
        return nullptr;

    if (!query.workspace)
        return nullptr;

    // 0 -> history, 1 -> shared length
    static auto PMETHOD          = CConfigValue<Config::INTEGER>("binds:focus_preferred_method");
    static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");

    const auto  POSA  = query.origin.pos();
    const auto  SIZEA = query.origin.size();

    auto        leaderValue        = -1;
    auto        floatingPreference = query.floatingPreference;
    PHLWINDOW   leaderWindow       = nullptr;

    if (!query.useVectorAngles) {
        // helper to check if two rectangles are adjacent along an axis, considering slight overlaps.
        // returns true if: STICKS (delta <= 2) OR rectangles overlap but no more than 50% of the smaller dimension.
        static auto isAdjacent = [](const double aMin, const double aMax, const double bMin, const double bMax) -> bool {
            constexpr double STICK_THRESHOLD   = 2.0;
            constexpr double MAX_OVERLAP_RATIO = 0.5;

            const double     aEdge = aMin;
            const double     bEdge = bMax;
            const double     delta = aEdge - bEdge;

            // old STICKS check for 2px
            if (std::abs(delta) < STICK_THRESHOLD)
                return true;

            if (delta >= 0)
                return false;

            const double overlap = -delta;
            const double sizeA   = aMax - aMin;
            const double sizeB   = bMax - bMin;

            // reject if one rectangle fully contains the other
            if ((bMin <= aMin && bMax >= aMax) || (aMin <= bMin && aMax >= bMax))
                return false;

            // accept if overlap is at most 50% of the smaller dimension
            return overlap <= std::min(sizeA, sizeB) * MAX_OVERLAP_RATIO;
        };

        auto find = [&]() {
            for (auto const& w : m_state.windows()) {
                if (w == query.ignoreWindow || !w->m_workspace || !w->m_isMapped || (!w->isFullscreen() && w->m_isFloating) || !w->m_workspace->isVisible())
                    continue;

                if (w->isHidden())
                    continue;

                if (w->hasInputBlockedReasonsBesides(INPUT_BLOCK_BELOW_FULLSCREEN))
                    continue;

                if (query.workspace->m_monitor == w->m_monitor && query.workspace != w->m_workspace)
                    continue;

                if (query.workspace->m_hasFullscreenWindow && !w->isAllowedOverFullscreen())
                    continue;

                if (!*PMONITORFALLBACK && query.workspace->m_monitor != w->m_monitor)
                    continue;

                if ((!w->isFullscreen() && w->m_isFloating) != floatingPreference)
                    continue;

                // prioritize windows on the same workspace.
                // this is especially important for scrolling layouts - we want to first move to a window
                // on the same workspace before moving onto another.
                const auto LEADER_IS_ON_SAME_WORKSPACE = leaderWindow && leaderWindow->m_workspace == query.workspace;

                if (LEADER_IS_ON_SAME_WORKSPACE && w->m_workspace != query.workspace)
                    continue;

                const auto BWINDOWIDEALBB = w->getWindowIdealBoundingBoxIgnoreReserved();

                const auto POSB  = Vector2D(BWINDOWIDEALBB.x, BWINDOWIDEALBB.y);
                const auto SIZEB = Vector2D(BWINDOWIDEALBB.width, BWINDOWIDEALBB.height);

                double     intersectLength = -1;

                switch (query.direction) {
                    case Math::DIRECTION_LEFT:
                        if (isAdjacent(POSA.x, POSA.x + SIZEA.x, POSB.x, POSB.x + SIZEB.x))
                            intersectLength = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                        break;
                    case Math::DIRECTION_RIGHT:
                        if (isAdjacent(POSB.x, POSB.x + SIZEB.x, POSA.x, POSA.x + SIZEA.x))
                            intersectLength = std::max(0.0, std::min(POSA.y + SIZEA.y, POSB.y + SIZEB.y) - std::max(POSA.y, POSB.y));
                        break;
                    case Math::DIRECTION_UP:
                        if (isAdjacent(POSA.y, POSA.y + SIZEA.y, POSB.y, POSB.y + SIZEB.y))
                            intersectLength = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                        break;
                    case Math::DIRECTION_DOWN:
                        if (isAdjacent(POSB.y, POSB.y + SIZEB.y, POSA.y, POSA.y + SIZEA.y))
                            intersectLength = std::max(0.0, std::min(POSA.x + SIZEA.x, POSB.x + SIZEB.x) - std::max(POSA.x, POSB.x));
                        break;
                    default: break;
                }

                // if we have a leader on another workspace, and this window is on the same workspace,
                // override minimum requirements and always select this as the new leader
                const bool OVERRIDE_MIN_REQ = leaderWindow && !LEADER_IS_ON_SAME_WORKSPACE && w->m_workspace == query.workspace;

                // ...as long as there is any intersect.
                if (intersectLength <= 1)
                    continue;

                if (*PMETHOD == 0 /* history */) {
                    // get idx
                    int         windowIDX = -1;
                    const auto& HISTORY   = Desktop::History::windowTracker()->fullHistory();
                    for (int64_t i = HISTORY.size() - 1; i >= 0; --i) {
                        if (HISTORY[i] == w) {
                            windowIDX = i;
                            break;
                        }
                    }

                    if (windowIDX > leaderValue || OVERRIDE_MIN_REQ) {
                        leaderValue  = windowIDX;
                        leaderWindow = w;
                    }
                } else /* length */ {
                    if (intersectLength > leaderValue || OVERRIDE_MIN_REQ) {
                        leaderValue  = intersectLength;
                        leaderWindow = w;
                    }
                }
            }
        };

        // Find the window, then if we don't find one with preferred
        // float status, try the opposite.
        find();

        if (!leaderWindow) {
            floatingPreference = !floatingPreference;
            find();
        }

    } else {
        static const std::unordered_map<Math::eDirection, Vector2D> VECTORS = {
            {Math::DIRECTION_RIGHT, {1, 0}}, {Math::DIRECTION_UP, {0, -1}}, {Math::DIRECTION_DOWN, {0, 1}}, {Math::DIRECTION_LEFT, {-1, 0}}};

        auto vectorAngles = [](const Vector2D& a, const Vector2D& b) -> double {
            double dot = (a.x * b.x) + (a.y * b.y);
            double ang = std::acos(dot / (a.size() * b.size()));
            return ang;
        };

        float           bestAngleAbs = 2.0 * M_PI;
        constexpr float THRESHOLD    = 0.3 * M_PI;

        for (auto const& w : m_state.windows()) {
            if (w == query.ignoreWindow || !w->m_isMapped || !w->m_workspace || !w->acceptsInput() || (!w->isFullscreen() && !w->m_isFloating) || !w->m_workspace->isVisible())
                continue;

            if (query.workspace->m_monitor == w->m_monitor && query.workspace != w->m_workspace)
                continue;

            if (query.workspace->m_hasFullscreenWindow && !w->isAllowedOverFullscreen())
                continue;

            if (!*PMONITORFALLBACK && query.workspace->m_monitor != w->m_monitor)
                continue;

            const auto DIST  = w->middle().distance(query.origin.middle());
            const auto ANGLE = vectorAngles(Vector2D{w->middle() - query.origin.middle()}, VECTORS.at(query.direction));

            if (ANGLE > M_PI_2)
                continue; // if the angle is over 90 degrees, ignore. Wrong direction entirely.

            if ((bestAngleAbs < THRESHOLD && DIST < leaderValue && ANGLE < THRESHOLD) || (ANGLE < bestAngleAbs && bestAngleAbs > THRESHOLD) || leaderValue == -1) {
                leaderValue  = DIST;
                bestAngleAbs = ANGLE;
                leaderWindow = w;
            }
        }

        if (!leaderWindow && query.workspace->m_hasFullscreenWindow)
            leaderWindow = query.workspace->getFullscreenWindow();
    }

    if (leaderValue != -1)
        return leaderWindow;

    return nullptr;
}

template <typename WINDOWPTR>
static bool isWorkspaceMatches(WINDOWPTR pWindow, const WINDOWPTR w, bool anyWorkspace) {
    return anyWorkspace ? w->m_workspace && w->m_workspace->isVisible() : w->m_workspace == pWindow->m_workspace;
}

template <typename WINDOWPTR>
static bool isFloatingMatches(WINDOWPTR w, std::optional<bool> floating) {
    return !floating.has_value() || w->m_isFloating == floating.value();
}

template <typename WINDOWPTR>
static bool acceptsInputForCycle(WINDOWPTR w, bool allowFullscreenBlocked) {
    if (w->acceptsInput())
        return true;

    return allowFullscreenBlocked && !w->isHidden() && w->noInputBlockedReasonsBesides(INPUT_BLOCK_BELOW_FULLSCREEN);
}

template <typename WINDOWPTR>
static bool isWindowAvailableForCycle(WINDOWPTR pWindow, WINDOWPTR w, const SWindowCycleOptions& options) {
    return isFloatingMatches(w, options.floating) &&
        (w != pWindow && isWorkspaceMatches(pWindow, w, options.visible) && w->m_isMapped && acceptsInputForCycle(w, options.allowFullscreenBlocked) &&
         (!options.focusableOnly || !w->m_ruleApplicator->noFocus().valueOrDefault()));
}

template <typename Iterator>
static PHLWINDOW getWindowPred(Iterator cur, Iterator end, Iterator begin, const std::function<bool(const PHLWINDOW&)> PRED) {
    const auto IN_ONE_SIDE = std::find_if(cur, end, PRED);
    if (IN_ONE_SIDE != end)
        return *IN_ONE_SIDE;
    const auto IN_OTHER_SIDE = std::find_if(begin, cur, PRED);
    return *IN_OTHER_SIDE;
}

template <typename Iterator>
static PHLWINDOW getWeakWindowPred(Iterator cur, Iterator end, Iterator begin, const std::function<bool(const PHLWINDOWREF&)> PRED) {
    const auto IN_ONE_SIDE = std::find_if(cur, end, PRED);
    if (IN_ONE_SIDE != end)
        return IN_ONE_SIDE->lock();
    const auto IN_OTHER_SIDE = std::find_if(begin, cur, PRED);
    return IN_OTHER_SIDE->lock();
}

PHLWINDOW CWindowQuery::cycleHistory(PHLWINDOWREF current, const SWindowCycleOptions& options) const {
    const auto FINDER = [&](const PHLWINDOWREF& w) { return isWindowAvailableForCycle(current, w, options); };
    // also m_vWindowFocusHistory has reverse order, so when it is next - we need to reverse again
    const auto& HISTORY = Desktop::History::windowTracker()->fullHistory();
    return !options.previous ? getWeakWindowPred(std::ranges::find(HISTORY, current), HISTORY.end(), HISTORY.begin(), FINDER) :
                               getWeakWindowPred(std::ranges::find(HISTORY | std::views::reverse, current), HISTORY.rend(), HISTORY.rbegin(), FINDER);
}

PHLWINDOW CWindowQuery::cycle(PHLWINDOW current, const SWindowCycleOptions& options) const {
    const auto FINDER = [&](const PHLWINDOW& w) { return isWindowAvailableForCycle(current, w, options); };
    return options.previous ? getWindowPred(std::ranges::find(m_state.windows() | std::views::reverse, current), m_state.windows().rend(), m_state.windows().rbegin(), FINDER) :
                              getWindowPred(std::ranges::find(m_state.windows(), current), m_state.windows().end(), m_state.windows().begin(), FINDER);
}
