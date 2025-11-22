#include "TrackpadGestures.hpp"

#include "../InputManager.hpp"

#include <ranges>

void CTrackpadGestures::clearGestures() {
    m_gestures.clear();
}

eTrackpadGestureDirection CTrackpadGestures::dirForString(const std::string_view& s) {
    std::string lc = std::string{s};
    std::ranges::transform(lc, lc.begin(), ::tolower);

    if (lc == "swipe")
        return TRACKPAD_GESTURE_DIR_SWIPE;
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
    if (lc == "pinch")
        return TRACKPAD_GESTURE_DIR_PINCH;
    if (lc == "pinchin" || lc == "zoomin")
        return TRACKPAD_GESTURE_DIR_PINCH_IN;
    if (lc == "pinchout" || lc == "zoomout")
        return TRACKPAD_GESTURE_DIR_PINCH_OUT;

    return TRACKPAD_GESTURE_DIR_NONE;
}

const char* CTrackpadGestures::stringForDir(eTrackpadGestureDirection dir) {
    switch (dir) {
        case TRACKPAD_GESTURE_DIR_HORIZONTAL: return "HORIZONTAL";
        case TRACKPAD_GESTURE_DIR_VERTICAL: return "VERTICAL";
        case TRACKPAD_GESTURE_DIR_LEFT: return "LEFT";
        case TRACKPAD_GESTURE_DIR_RIGHT: return "RIGHT";
        case TRACKPAD_GESTURE_DIR_UP: return "UP";
        case TRACKPAD_GESTURE_DIR_DOWN: return "DOWN";
        case TRACKPAD_GESTURE_DIR_SWIPE: return "SWIPE";
        case TRACKPAD_GESTURE_DIR_PINCH: return "PINCH";
        case TRACKPAD_GESTURE_DIR_PINCH_IN: return "PINCH_IN";
        case TRACKPAD_GESTURE_DIR_PINCH_OUT: return "PINCH_OUT";
        default: return "ERROR";
    }
    return "ERROR";
}

std::expected<void, std::string> CTrackpadGestures::addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, uint32_t modMask,
                                                               float deltaScale) {
    for (const auto& g : m_gestures) {
        if (g->fingerCount != fingerCount)
            continue;

        if (g->modMask != modMask)
            continue;

        eTrackpadGestureDirection axis = TRACKPAD_GESTURE_DIR_NONE;
        switch (direction) {
            case TRACKPAD_GESTURE_DIR_UP:
            case TRACKPAD_GESTURE_DIR_DOWN:
            case TRACKPAD_GESTURE_DIR_VERTICAL: axis = TRACKPAD_GESTURE_DIR_VERTICAL; break;
            case TRACKPAD_GESTURE_DIR_LEFT:
            case TRACKPAD_GESTURE_DIR_RIGHT:
            case TRACKPAD_GESTURE_DIR_HORIZONTAL: axis = TRACKPAD_GESTURE_DIR_HORIZONTAL; break;
            case TRACKPAD_GESTURE_DIR_SWIPE: axis = TRACKPAD_GESTURE_DIR_SWIPE; break;
            case TRACKPAD_GESTURE_DIR_PINCH:
            case TRACKPAD_GESTURE_DIR_PINCH_IN:
            case TRACKPAD_GESTURE_DIR_PINCH_OUT: axis = TRACKPAD_GESTURE_DIR_PINCH; break;
            default: TRACKPAD_GESTURE_DIR_NONE; break;
        }

        if (g->direction == axis || g->direction == direction ||
            ((axis == TRACKPAD_GESTURE_DIR_VERTICAL || axis == TRACKPAD_GESTURE_DIR_HORIZONTAL) && g->direction == TRACKPAD_GESTURE_DIR_SWIPE)) {
            return std::unexpected(
                std::format("Gesture will be overshadowed by a previous gesture. Previous {} shadows new {}", stringForDir(g->direction), stringForDir(direction)));
        }
    }

    m_gestures.emplace_back(makeShared<CTrackpadGestures::SGestureData>(std::move(gesture), fingerCount, modMask, direction, deltaScale));

    return {};
}

std::expected<void, std::string> CTrackpadGestures::removeGesture(size_t fingerCount, eTrackpadGestureDirection direction, uint32_t modMask, float deltaScale) {
    const auto IT = std::ranges::find_if(
        m_gestures, [&](const auto& g) { return g->fingerCount == fingerCount && g->direction == direction && g->modMask == modMask && g->deltaScale == deltaScale; });

    if (IT == m_gestures.end())
        return std::unexpected("Can't remove a non-existent gesture");

    std::erase(m_gestures, *IT);

    return {};
}

void CTrackpadGestures::gestureBegin(const IPointer::SSwipeBeginEvent& e) {
    if (m_activeGesture) {
        Debug::log(ERR, "CTrackpadGestures::gestureBegin (swipe) but m_activeGesture is already present");
        return;
    }

    m_gestureFindFailed = false;
    m_currentTotalDelta = {};

    // nothing here. We need to wait for the first update to determine the delta.
}

void CTrackpadGestures::gestureUpdate(const IPointer::SSwipeUpdateEvent& e) {
    if (m_gestureFindFailed)
        return;

    m_currentTotalDelta += e.delta;

    // 5 was chosen because I felt like that's a good number.
    if (!m_activeGesture && (std::abs(m_currentTotalDelta.x) < 5 && std::abs(m_currentTotalDelta.y) < 5)) {
        Debug::log(TRACE, "CTrackpadGestures::gestureUpdate (swipe): gesture delta too small to start considering, waiting");
        return;
    }

    if (!m_activeGesture) {
        // try to find a gesture that matches our current state

        auto direction = TRACKPAD_GESTURE_DIR_NONE;
        auto axis      = std::abs(m_currentTotalDelta.x) > std::abs(m_currentTotalDelta.y) ? TRACKPAD_GESTURE_DIR_HORIZONTAL : TRACKPAD_GESTURE_DIR_VERTICAL;

        if (axis == TRACKPAD_GESTURE_DIR_HORIZONTAL)
            direction = m_currentTotalDelta.x < 0 ? TRACKPAD_GESTURE_DIR_LEFT : TRACKPAD_GESTURE_DIR_RIGHT;
        else
            direction = m_currentTotalDelta.y < 0 ? TRACKPAD_GESTURE_DIR_UP : TRACKPAD_GESTURE_DIR_DOWN;

        const auto MODS = g_pInputManager->getModsFromAllKBs();

        for (const auto& g : m_gestures) {
            if (g->direction != axis && g->direction != direction && g->direction != TRACKPAD_GESTURE_DIR_SWIPE)
                continue;

            if (g->fingerCount != e.fingers)
                continue;

            if (g->modMask != MODS)
                continue;

            m_activeGesture     = g;
            g->currentDirection = g->gesture->isDirectionSensitive() ? g->direction : direction;
            m_activeGesture->gesture->begin({.swipe = &e, .direction = direction, .scale = g->deltaScale});
            break;
        }

        if (!m_activeGesture) {
            m_gestureFindFailed = true;
            return;
        }
    }

    m_activeGesture->gesture->update({.swipe = &e, .direction = m_activeGesture->currentDirection, .scale = m_activeGesture->deltaScale});
}

void CTrackpadGestures::gestureEnd(const IPointer::SSwipeEndEvent& e) {
    if (!m_activeGesture)
        return;

    m_activeGesture->gesture->end({.swipe = &e, .direction = m_activeGesture->direction, .scale = m_activeGesture->deltaScale});

    m_activeGesture.reset();
}

void CTrackpadGestures::gestureBegin(const IPointer::SPinchBeginEvent& e) {
    if (m_activeGesture) {
        Debug::log(ERR, "CTrackpadGestures::gestureBegin (pinch) but m_activeGesture is already present");
        return;
    }

    m_gestureFindFailed = false;

    // nothing here. We need to wait for the first update to determine the delta.
}

void CTrackpadGestures::gestureUpdate(const IPointer::SPinchUpdateEvent& e) {
    if (m_gestureFindFailed)
        return;

    // 0.1 was chosen because I felt like that's a good number.
    if (!m_activeGesture && std::abs(e.scale - 1.F) < 0.1) {
        Debug::log(TRACE, "CTrackpadGestures::gestureUpdate (pinch): gesture delta too small to start considering, waiting");
        return;
    }

    if (!m_activeGesture) {
        // try to find a gesture that matches our current state

        auto       direction = e.scale < 1.F ? TRACKPAD_GESTURE_DIR_PINCH_OUT : TRACKPAD_GESTURE_DIR_PINCH_IN;
        auto       axis      = TRACKPAD_GESTURE_DIR_PINCH;

        const auto MODS = g_pInputManager->getModsFromAllKBs();

        for (const auto& g : m_gestures) {
            if (g->direction != axis && g->direction != direction)
                continue;

            if (g->fingerCount != e.fingers)
                continue;

            if (g->modMask != MODS)
                continue;

            m_activeGesture     = g;
            g->currentDirection = g->gesture->isDirectionSensitive() ? g->direction : direction;
            m_activeGesture->gesture->begin({.pinch = &e, .direction = direction});
            break;
        }

        if (!m_activeGesture) {
            m_gestureFindFailed = true;
            return;
        }
    }

    m_activeGesture->gesture->update({.pinch = &e, .direction = m_activeGesture->currentDirection});
}

void CTrackpadGestures::gestureEnd(const IPointer::SPinchEndEvent& e) {
    if (!m_activeGesture)
        return;

    m_activeGesture->gesture->end({.pinch = &e, .direction = m_activeGesture->direction});

    m_activeGesture.reset();
}
