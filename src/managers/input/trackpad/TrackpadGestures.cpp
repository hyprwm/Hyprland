#include "TrackpadGestures.hpp"

#include "../InputManager.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../config/shared/actions/ConfigActions.hpp"
#include "../../../keybinds/Manager.hpp"
#include "../../../protocols/ShortcutsInhibit.hpp"

#include <ranges>

static std::string triggerBindID(const std::string& trigger, Input::ModifierMask modMask) {
    return std::format("{}:{}", sc<uint32_t>(modMask), trigger);
}

static std::vector<std::string> keysForTrigger(const std::string& trigger, Input::ModifierMask modMask) {
    static constexpr std::pair<Input::eKeyboardModifiers, std::string_view> MODIFIERS[] = {
        {Input::HL_MODIFIER_SHIFT, "SHIFT"}, {Input::HL_MODIFIER_CAPS, "CAPS"}, {Input::HL_MODIFIER_MOD2, "MOD2"}, {Input::HL_MODIFIER_MOD3, "MOD3"},
        {Input::HL_MODIFIER_MOD5, "MOD5"},   {Input::HL_MODIFIER_CTRL, "CTRL"}, {Input::HL_MODIFIER_ALT, "ALT"},   {Input::HL_MODIFIER_META, "SUPER"},
    };

    std::vector<std::string> keys;
    for (const auto& [mask, name] : MODIFIERS) {
        if (modMask & mask)
            keys.emplace_back(name);
    }
    keys.emplace_back(trigger);
    return keys;
}

void CTrackpadGestures::clearGestures() {
    pointerGestureEnd(true);

    for (const auto& bind : m_triggerBinds)
        Keybinds::mgr()->removeBind(bind);

    m_triggerBinds.clear();
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

std::expected<void, std::string> CTrackpadGestures::addGesture(UP<ITrackpadGesture>&& gesture, size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask,
                                                               float deltaScale, bool disableInhibit, std::string trigger, eGestureDeltaSource source) {
    for (const auto& g : m_gestures) {
        if (g->trigger != trigger)
            continue;

        if (g->fingerCount != fingerCount)
            continue;

        if (g->modMask != modMask || g->source != source)
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

    if (!trigger.empty()) {
        const auto bindResult = addTriggerBind(trigger, modMask);
        if (!bindResult)
            return bindResult;
    }

    m_gestures.emplace_back(makeShared<CTrackpadGestures::SGestureData>(std::move(gesture), fingerCount, trigger, source, modMask, direction, deltaScale, disableInhibit));
    return {};
}

std::expected<void, std::string> CTrackpadGestures::addTriggerBind(const std::string& trigger, Input::ModifierMask modMask) {
    const auto BIND_ID = triggerBindID(trigger, modMask);
    if (std::ranges::any_of(m_triggerBinds, [&BIND_ID](const auto& bind) { return bind->metadata().displayKey == BIND_ID; }))
        return {};

    // Inhibition is checked per gesture, since directions can share a trigger.
    auto bind = Keybinds::CBind::make(
        keysForTrigger(trigger, modMask), Keybinds::BIND_FLAG_SUBMAP_UNIVERSAL | Keybinds::BIND_FLAG_DONT_INHIBIT,
        [this, trigger, modMask] {
            if (Config::Actions::state()->m_passPressed == 0) {
                if (m_activeTrigger == trigger && m_activeModMask == modMask)
                    pointerGestureEnd();
                return Keybinds::SBindResult{};
            }

            if (!pointerGestureBegin(trigger, modMask))
                return Keybinds::SBindResult{.passEvent = true};

            return Keybinds::SBindResult{.followUp = Keybinds::BIND_FOLLOW_UP_TRIGGER_RELEASE};
        },
        Keybinds::SExtraBindArgs{.metadata = {.displayKey = BIND_ID}});
    if (!bind)
        return std::unexpected(bind.error());

    m_triggerBinds.emplace_back(Keybinds::mgr()->addBind(std::move(*bind)));
    return {};
}

void CTrackpadGestures::removeTriggerBind(const std::string& trigger, Input::ModifierMask modMask) {
    const auto BIND_ID = triggerBindID(trigger, modMask);
    std::erase_if(m_triggerBinds, [&](const auto& bind) {
        if (bind->metadata().displayKey != BIND_ID)
            return false;

        Keybinds::mgr()->removeBind(bind);
        return true;
    });
}

bool CTrackpadGestures::hasGestureForTrigger(const std::string& trigger, Input::ModifierMask modMask) const {
    return std::ranges::any_of(m_gestures, [&](const auto& gesture) { return gesture->trigger == trigger && gesture->modMask == modMask; });
}

bool CTrackpadGestures::hasGestureForSource(eGestureDeltaSource source) const {
    if (m_activeDeltaSource != GESTURE_DELTA_SOURCE_NONE)
        return m_activeDeltaSource == source;

    return std::ranges::any_of(m_gestures,
                               [&](const auto& gesture) { return gesture->trigger == m_activeTrigger && gesture->modMask == m_activeModMask && gesture->source == source; });
}

std::expected<void, std::string> CTrackpadGestures::removeGesture(size_t fingerCount, eTrackpadGestureDirection direction, Input::ModifierMask modMask, float deltaScale,
                                                                  bool disableInhibit, std::string trigger, eGestureDeltaSource source) {
    const auto IT = std::ranges::find_if(m_gestures, [&](const auto& g) {
        return g->fingerCount == fingerCount && g->trigger == trigger && g->source == source && g->direction == direction && g->modMask == modMask && g->deltaScale == deltaScale &&
            g->disableInhibit == disableInhibit;
    });

    if (IT == m_gestures.end())
        return std::unexpected("Can't remove a non-existent gesture");

    if (m_activeGesture == *IT)
        pointerGestureEnd(true);

    std::erase(m_gestures, *IT);

    if (!trigger.empty() && !hasGestureForTrigger(trigger, modMask))
        removeTriggerBind(trigger, modMask);

    return {};
}

void CTrackpadGestures::gestureBegin(const IPointer::SSwipeBeginEvent& e) {
    if (pointerGestureActive())
        return;

    if (m_activeGesture) {
        LOG(Log::ERR, "CTrackpadGestures::gestureBegin (swipe) but m_activeGesture is already present");
        return;
    }

    m_gestureFindFailed = false;
    m_currentTotalDelta = {};

    // nothing here. We need to wait for the first update to determine the delta.
}

void CTrackpadGestures::gestureUpdate(const IPointer::SSwipeUpdateEvent& e) {
    if (pointerGestureActive())
        return;

    swipeUpdate(e);
}

void CTrackpadGestures::swipeUpdate(const IPointer::SSwipeUpdateEvent& e, eGestureDeltaSource source) {
    static auto PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");

    if (m_gestureFindFailed)
        return;

    if (source != GESTURE_DELTA_SOURCE_NONE && m_activeDeltaSource == GESTURE_DELTA_SOURCE_NONE)
        m_activeDeltaSource = source;

    m_currentTotalDelta += e.delta;

    // 5 was chosen because I felt like that's a good number.
    if (!m_activeGesture && (std::abs(m_currentTotalDelta.x) < 5 && std::abs(m_currentTotalDelta.y) < 5)) {
        LOG(Log::TRACE, "CTrackpadGestures::gestureUpdate (swipe): gesture delta too small to start considering, waiting");
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

        const auto MODS = source == GESTURE_DELTA_SOURCE_NONE ? g_pInputManager->getModsFromAllKBs() : m_activeModMask;

        for (const auto& g : m_gestures) {
            if (source != GESTURE_DELTA_SOURCE_NONE && g->source != source)
                continue;

            if (g->direction != axis && g->direction != direction && g->direction != TRACKPAD_GESTURE_DIR_SWIPE)
                continue;

            if (g->trigger != m_activeTrigger || g->fingerCount != e.fingers)
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
    if (pointerGestureActive() || !m_activeGesture)
        return;

    m_activeGesture->gesture->end({.swipe = &e, .direction = m_activeGesture->direction, .scale = m_activeGesture->deltaScale});

    m_activeGesture.reset();
}

bool CTrackpadGestures::pointerGestureBegin(const std::string& trigger, Input::ModifierMask modMask) {
    static auto PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");

    if (m_activeGesture || pointerGestureActive() || PROTO::inputCapture->isCaptured())
        return false;

    if (!std::ranges::any_of(m_gestures, [&](const auto& g) {
            return g->trigger == trigger && g->modMask == modMask && (!PROTO::shortcutsInhibit->isInhibited() || *PDISABLEINHIBIT || g->disableInhibit);
        }))
        return false;

    gestureBegin(IPointer::SSwipeBeginEvent{});
    m_activeTrigger = trigger;
    m_activeModMask = modMask;
    return true;
}

bool CTrackpadGestures::pointerGestureActive() const {
    return !m_activeTrigger.empty();
}

void CTrackpadGestures::pointerGestureUpdate(const IPointer::SSwipeUpdateEvent& e) {
    if (!pointerGestureActive() || !hasGestureForSource(GESTURE_DELTA_SOURCE_MOUSE))
        return;

    swipeUpdate(e, GESTURE_DELTA_SOURCE_MOUSE);
}

static std::optional<eGestureDeltaSource> deltaSourceForAxis(wl_pointer_axis_source source) {
    switch (source) {
        case WL_POINTER_AXIS_SOURCE_WHEEL: return GESTURE_DELTA_SOURCE_WHEEL;
        case WL_POINTER_AXIS_SOURCE_FINGER: return GESTURE_DELTA_SOURCE_FINGER;
        case WL_POINTER_AXIS_SOURCE_CONTINUOUS: return GESTURE_DELTA_SOURCE_CONTINUOUS;
        case WL_POINTER_AXIS_SOURCE_WHEEL_TILT: return GESTURE_DELTA_SOURCE_WHEEL_TILT;
        default: return std::nullopt;
    }
}

bool CTrackpadGestures::axisGestureUpdate(const IPointer::SAxisEvent& e) {
    const auto SOURCE = deltaSourceForAxis(e.source);
    if (!pointerGestureActive() || !SOURCE || !hasGestureForSource(*SOURCE))
        return false;

    if (e.delta == 0)
        return false;

    swipeUpdate({.timeMs = e.timeMs, .delta = e.axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL ? Vector2D{e.delta, 0.0} : Vector2D{0.0, e.delta}}, *SOURCE);
    return true;
}

void CTrackpadGestures::pointerGestureEnd(bool cancelled) {
    if (m_activeTrigger.empty())
        return;

    m_activeTrigger.clear();
    m_activeDeltaSource = GESTURE_DELTA_SOURCE_NONE;
    gestureEnd(IPointer::SSwipeEndEvent{.cancelled = cancelled});
}

void CTrackpadGestures::gestureBegin(const IPointer::SPinchBeginEvent& e) {
    if (pointerGestureActive())
        return;

    if (m_activeGesture) {
        LOG(Log::ERR, "CTrackpadGestures::gestureBegin (pinch) but m_activeGesture is already present");
        return;
    }

    m_gestureFindFailed = false;

    // nothing here. We need to wait for the first update to determine the delta.
}

void CTrackpadGestures::gestureUpdate(const IPointer::SPinchUpdateEvent& e) {
    if (pointerGestureActive())
        return;

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
    if (pointerGestureActive() || !m_activeGesture)
        return;

    m_activeGesture->gesture->end({.pinch = &e, .direction = m_activeGesture->direction, .scale = m_activeGesture->deltaScale});

    m_activeGesture.reset();
}
