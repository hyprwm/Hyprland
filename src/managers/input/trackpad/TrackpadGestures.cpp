#include "TrackpadGestures.hpp"

#include "../InputManager.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../protocols/ShortcutsInhibit.hpp"

#include <ranges>

void CTrackpadGestures::clearGestures() {
    m_activeGesture.reset();
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

std::expected<void, std::string> CTrackpadGestures::addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask,
                                                               float deltaScale, bool disableInhibit) {
    for (const auto& g : m_gestures) {
        if (g->fingerCount != fingerCount)
            continue;

        if (g->modMask != modMask)
            continue;

        if (gestureDirectionOvershadows(g->direction, direction)) {
            return std::unexpected(std::format("Gesture will be overshadowed by a previous gesture. Previous {} shadows new {}", gestureDirectionToString(g->direction),
                                               gestureDirectionToString(direction)));
        }
    }

    m_gestures.emplace_back(makeShared<CTrackpadGestures::SGestureData>(std::move(gesture), fingerCount, modMask, direction, deltaScale, disableInhibit));

    return {};
}

std::expected<void, std::string> CTrackpadGestures::removeGesture(size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask, float deltaScale,
                                                                  bool disableInhibit) {
    const auto IT = std::ranges::find_if(m_gestures, [&](const auto& g) {
        return g->fingerCount == fingerCount && g->direction == direction && g->modMask == modMask && g->deltaScale == deltaScale && g->disableInhibit == disableInhibit;
    });

    if (IT == m_gestures.end())
        return std::unexpected("Can't remove a non-existent gesture");

    if (m_activeGesture == *IT)
        m_activeGesture.reset();

    std::erase(m_gestures, *IT);

    return {};
}

void CTrackpadGestures::gestureBegin(const IPointer::SSwipeBeginEvent& e) {
    if (m_activeGesture) {
        LOG(Log::ERR, "CTrackpadGestures::gestureBegin (swipe) but m_activeGesture is already present");
        return;
    }

    m_gestureFindFailed = false;
    m_currentTotalDelta = {};

    // nothing here. We need to wait for the first update to determine the delta.
}

void CTrackpadGestures::gestureUpdate(const IPointer::SSwipeUpdateEvent& e) {
    static auto PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");

    if (m_gestureFindFailed)
        return;

    m_currentTotalDelta += e.delta;

    // 5 was chosen because I felt like that's a good number.
    if (!m_activeGesture && (std::abs(m_currentTotalDelta.x) < 5 && std::abs(m_currentTotalDelta.y) < 5)) {
        LOG(Log::TRACE, "CTrackpadGestures::gestureUpdate (swipe): gesture delta too small to start considering, waiting");
        return;
    }

    if (!m_activeGesture) {
        // try to find a gesture that matches our current state

        const auto [axis, direction] = gestureSwipeDirectionForDelta(m_currentTotalDelta);

        const auto MODS = g_pInputManager->getModsFromAllKBs();

        for (const auto& g : m_gestures) {
            if (g->direction != axis && g->direction != direction && g->direction != TRACKPAD_GESTURE_DIR_SWIPE)
                continue;

            if (g->fingerCount != e.fingers)
                continue;

            if (g->modMask != MODS)
                continue;

            if (PROTO::shortcutsInhibit->isInhibited() && !*PDISABLEINHIBIT && !g->disableInhibit)
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
        LOG(Log::ERR, "CTrackpadGestures::gestureBegin (pinch) but m_activeGesture is already present");
        return;
    }

    m_gestureFindFailed = false;

    // nothing here. We need to wait for the first update to determine the delta.
}

void CTrackpadGestures::gestureUpdate(const IPointer::SPinchUpdateEvent& e) {
    static auto PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");

    if (m_gestureFindFailed)
        return;

    // 0.1 was chosen because I felt like that's a good number.
    if (!m_activeGesture && std::abs(e.scale - 1.F) < 0.1) {
        LOG(Log::TRACE, "CTrackpadGestures::gestureUpdate (pinch): gesture delta too small to start considering, waiting");
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

            if (PROTO::shortcutsInhibit->isInhibited() && !*PDISABLEINHIBIT && !g->disableInhibit)
                continue;

            m_activeGesture     = g;
            g->currentDirection = g->gesture->isDirectionSensitive() ? g->direction : direction;
            m_activeGesture->gesture->begin({.pinch = &e, .direction = direction, .scale = g->deltaScale});
            break;
        }

        if (!m_activeGesture) {
            m_gestureFindFailed = true;
            return;
        }
    }

    m_activeGesture->gesture->update({.pinch = &e, .direction = m_activeGesture->currentDirection, .scale = m_activeGesture->deltaScale});
}

void CTrackpadGestures::gestureEnd(const IPointer::SPinchEndEvent& e) {
    if (!m_activeGesture)
        return;

    m_activeGesture->gesture->end({.pinch = &e, .direction = m_activeGesture->direction, .scale = m_activeGesture->deltaScale});

    m_activeGesture.reset();
}
