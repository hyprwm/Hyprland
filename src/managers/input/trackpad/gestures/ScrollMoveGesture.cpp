#include "ScrollMoveGesture.hpp"

#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../desktop/Workspace.hpp"
#include "../../../../helpers/Monitor.hpp"
#include "../../../../layout/LayoutManager.hpp"
#include "../../../../layout/algorithm/Algorithm.hpp"
#include "../../../../layout/algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "../../../../layout/space/Space.hpp"
#include "../../../../config/ConfigValue.hpp"
#include "../../../../Compositor.hpp"

#include <algorithm>
#include <cmath>

constexpr double                           SCROLL_GESTURE_VELOCITY_DECAY  = 4.0;
constexpr double                           SCROLL_GESTURE_MAX_PROJECTION  = 2.0;
constexpr double                           SCROLL_GESTURE_VELOCITY_SMOOTH = 0.35;

static Layout::Tiled::CScrollingAlgorithm* currentScrollingLayout() {
    const auto PMONITOR = Desktop::focusState()->monitor();
    if (!PMONITOR)
        return nullptr;

    const auto PWORKSPACE = PMONITOR->m_activeSpecialWorkspace ? PMONITOR->m_activeSpecialWorkspace : PMONITOR->m_activeWorkspace;
    if (!PWORKSPACE || !PWORKSPACE->m_space)
        return nullptr;

    const auto ALGORITHM = PWORKSPACE->m_space->algorithm();
    if (!ALGORITHM || !ALGORITHM->tiledAlgo())
        return nullptr;

    return dynamic_cast<Layout::Tiled::CScrollingAlgorithm*>(ALGORITHM->tiledAlgo().get());
}

static float deltaForUpdate(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!e.swipe)
        return 0.F;

    switch (e.direction) {
        case TRACKPAD_GESTURE_DIR_LEFT:
        case TRACKPAD_GESTURE_DIR_RIGHT:
        case TRACKPAD_GESTURE_DIR_HORIZONTAL: return e.swipe->delta.x * e.scale;
        case TRACKPAD_GESTURE_DIR_UP:
        case TRACKPAD_GESTURE_DIR_DOWN:
        case TRACKPAD_GESTURE_DIR_VERTICAL: return e.swipe->delta.y * e.scale;
        default: return std::abs(e.swipe->delta.x) > std::abs(e.swipe->delta.y) ? e.swipe->delta.x * e.scale : e.swipe->delta.y * e.scale;
    }
}

void CScrollMoveTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    const auto LAYOUT = currentScrollingLayout();

    m_wasScrollingLayout = !!LAYOUT;
    m_hasLastUpdate      = false;
    m_lastUpdateTimeMs   = 0;
    m_velocity           = 0.0;

    m_startedOffset = LAYOUT ? LAYOUT->normalizedTapeOffset() : 0.0;
    m_startedColumn = LAYOUT ? LAYOUT->currentColumn() : nullptr;
}

void CScrollMoveTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_wasScrollingLayout)
        return;

    const auto SCROLLING = currentScrollingLayout();
    if (!SCROLLING)
        return;

    const float  DELTA   = deltaForUpdate(e);
    const double PRIMARY = SCROLLING->primaryViewportSize();

    if (DELTA == 0.F || PRIMARY <= 0.0)
        return;

    const double NORMALIZED_DELTA        = DELTA / PRIMARY;
    const double NORMALIZED_OFFSET_DELTA = -NORMALIZED_DELTA;

    SCROLLING->moveTapeNormalized(NORMALIZED_DELTA);

    if (!e.swipe)
        return;

    if (m_hasLastUpdate && e.swipe->timeMs > m_lastUpdateTimeMs) {
        const double DT = (e.swipe->timeMs - m_lastUpdateTimeMs) / 1000.0;

        if (DT > 0.0) {
            const double INSTANT_VELOCITY = NORMALIZED_OFFSET_DELTA / DT;

            if (std::isfinite(INSTANT_VELOCITY))
                m_velocity = m_velocity * (1.0 - SCROLL_GESTURE_VELOCITY_SMOOTH) + INSTANT_VELOCITY * SCROLL_GESTURE_VELOCITY_SMOOTH;
        }
    }

    m_hasLastUpdate    = true;
    m_lastUpdateTimeMs = e.swipe->timeMs;
}

void CScrollMoveTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    static auto PSNAP       = CConfigValue<Config::BOOL>("gestures:scrolling:move_snap_to_grid");
    static auto PSNAPCURSOR = CConfigValue<Config::BOOL>("gestures:scrolling:move_snap_cursor");

    const auto  SCROLLING = currentScrollingLayout();
    if (!m_wasScrollingLayout || !SCROLLING) {
        m_wasScrollingLayout = false;
        m_hasLastUpdate      = false;
        m_velocity           = 0.0;
        return;
    }

    const auto   CURRENT_FOCUS = Desktop::focusState()->window();

    const bool   CANCELLED = e.swipe && e.swipe->cancelled;
    const double Δ         = (CANCELLED ? 0.0 : std::clamp(m_velocity / SCROLL_GESTURE_VELOCITY_DECAY, -SCROLL_GESTURE_MAX_PROJECTION, SCROLL_GESTURE_MAX_PROJECTION));
    const double PROJECTED = SCROLLING->normalizedTapeOffset() + Δ;

    if (*PSNAP) {
        const auto LANDED = SCROLLING->snapToProjectedOffset(PROJECTED);

        // if we land on the same thing we started, move the tape back
        if (LANDED == m_startedColumn && LANDED)
            SCROLLING->moveTapeNormalized(SCROLLING->normalizedTapeOffset() - m_startedOffset);
        else
            SCROLLING->focusColumn(LANDED);
    } else {
        SCROLLING->moveTape(Δ);
        const auto COL = SCROLLING->getColumnAtViewportCenter();
        if (!COL->targetDatas.empty() && COL->targetDatas.front()->target) {
            if (const auto W = COL->targetDatas.front()->target->window(); W)
                Desktop::focusState()->fullWindowFocus(W, Desktop::FOCUS_REASON_FFM);
        }
    }

    const auto NEW_FOCUS = Desktop::focusState()->window();

    if (*PSNAPCURSOR && CURRENT_FOCUS != NEW_FOCUS && NEW_FOCUS)
        g_pCompositor->warpCursorTo(NEW_FOCUS->middle());

    m_wasScrollingLayout = false;
    m_hasLastUpdate      = false;
    m_velocity           = 0.0;
}
