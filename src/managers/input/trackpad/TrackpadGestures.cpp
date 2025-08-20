#include "TrackpadGestures.hpp"

#include <ranges>

void CTrackpadGestures::clearGestures() {
    m_gestures.clear();
}

CTrackpadGestures::eTrackpadGestureDirection CTrackpadGestures::dirForString(const std::string_view& s) {
    std::string lc = std::string{s};
    std::ranges::transform(lc, lc.begin(), ::tolower);

    if (lc == "left" || lc == "l")
        return TRACKPAD_GESTURE_DIR_LEFT;
    if (lc == "right" || lc == "r")
        return TRACKPAD_GESTURE_DIR_RIGHT;
    if (lc == "up" || lc == "u" || lc == "top" || lc == "t")
        return TRACKPAD_GESTURE_DIR_UP;
    if (lc == "down" || lc == "d" || lc == "bottom" || lc == "b")
        return TRACKPAD_GESTURE_DIR_DOWN;
    if (lc == "horizontal" || lc == "horiz")
        return TRACKPAD_GESTURE_DIR_HORIZONTAL;
    if (lc == "vertical" || lc == "vert")
        return TRACKPAD_GESTURE_DIR_VERTICAL;

    return TRACKPAD_GESTURE_DIR_NONE;
}

void CTrackpadGestures::addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction) {
    m_gestures.emplace_back(makeShared<CTrackpadGestures::SGestureData>(std::move(gesture), fingerCount, direction));
}

void CTrackpadGestures::gestureBegin(const IPointer::SSwipeBeginEvent& e) {
    if (m_activeGesture) {
        Debug::log(ERR, "CTrackpadGestures::gestureBegin but m_activeGesture is already present");
        return;
    }

    m_gestureFindFailed = false;

    // nothing here. We need to wait for the first update to determine the delta.
}

void CTrackpadGestures::gestureUpdate(const IPointer::SSwipeUpdateEvent& e) {
    if (m_gestureFindFailed)
        return;

    // 5 was chosen because I felt like that's a good number.
    if (!m_activeGesture && (std::abs(e.delta.x) < 5 && std::abs(e.delta.y) < 5)) {
        Debug::log(TRACE, "CTrackpadGestures::gestureUpdate: gesture delta too small to start considering, waiting");
        return;
    }

    if (!m_activeGesture) {
        // try to find a gesture that matches our current state

        auto direction = TRACKPAD_GESTURE_DIR_NONE;
        auto axis      = std::abs(e.delta.x) > std::abs(e.delta.y) ? TRACKPAD_GESTURE_DIR_HORIZONTAL : TRACKPAD_GESTURE_DIR_VERTICAL;

        if (axis == TRACKPAD_GESTURE_DIR_HORIZONTAL)
            direction = e.delta.x < 0 ? TRACKPAD_GESTURE_DIR_LEFT : TRACKPAD_GESTURE_DIR_RIGHT;
        else
            direction = e.delta.y < 0 ? TRACKPAD_GESTURE_DIR_UP : TRACKPAD_GESTURE_DIR_DOWN;

        for (const auto& g : m_gestures) {
            if (g->direction != axis && g->direction != direction)
                continue;

            if (g->fingerCount != e.fingers)
                continue;

            m_activeGesture = g;
            m_activeGesture->gesture->begin(e);
            break;
        }

        if (!m_activeGesture) {
            m_gestureFindFailed = true;
            return;
        }
    }

    m_activeGesture->gesture->update(e);
}

void CTrackpadGestures::gestureEnd(const IPointer::SSwipeEndEvent& e) {
    if (!m_activeGesture)
        return;

    m_activeGesture->gesture->end(e);

    m_activeGesture.reset();
}
