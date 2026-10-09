#include "TriggeredGestures.hpp"

#include "../InputManager.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../config/shared/actions/ConfigActions.hpp"
#include "../../../keybinds/Manager.hpp"
#include "../../../keybinds/Resolver.hpp"
#include "../../../protocols/InputCapture.hpp"
#include "../../../protocols/ShortcutsInhibit.hpp"

#include <ranges>

static std::string bindDisplayKey(const std::string& trigger, Input::ModifierMask modMask) {
    return std::format("{}:{}", sc<uint32_t>(modMask), trigger);
}

void CTriggeredGestures::clearGestures() {
    pointerGestureEnd(true);

    for (const auto& bind : m_triggerBinds)
        Keybinds::mgr()->removeBind(bind);

    m_triggerBinds.clear();
    m_gestures.clear();
}

std::expected<void, std::string> CTriggeredGestures::addGesture(UP<ITrackpadGesture>&& gesture, std::string trigger, eGestureDeltaSource source,
                                                                eTrackpadGestureDirection direction, Input::ModifierMask modMask, float deltaScale, bool disableInhibit) {
    for (const auto& g : m_gestures) {
        if (g->trigger != trigger || g->modMask != modMask || g->source != source)
            continue;

        if (gestureDirectionOvershadows(g->direction, direction)) {
            return std::unexpected(std::format("Gesture will be overshadowed by a previous gesture. Previous {} shadows new {}", gestureDirectionToString(g->direction),
                                               gestureDirectionToString(direction)));
        }
    }

    const auto bindResult = addTriggerBind(trigger, modMask);
    if (!bindResult)
        return bindResult;

    m_gestures.emplace_back(makeShared<SGestureData>(std::move(gesture), std::move(trigger), source, modMask, direction, deltaScale, disableInhibit));
    return {};
}

std::expected<void, std::string> CTriggeredGestures::addTriggerBind(const std::string& trigger, Input::ModifierMask modMask) {
    const auto BIND_DISPLAY_KEY = bindDisplayKey(trigger, modMask);
    if (std::ranges::any_of(m_triggerBinds, [&BIND_DISPLAY_KEY](const auto& bind) { return bind->metadata().displayKey == BIND_DISPLAY_KEY; }))
        return {};

    auto keys = Keybinds::modMaskToKeyNames(modMask);
    keys.emplace_back(trigger);

    auto bind = Keybinds::CBind::make(
        std::move(keys), Keybinds::BIND_FLAG_SUBMAP_UNIVERSAL | Keybinds::BIND_FLAG_DONT_INHIBIT,
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
        Keybinds::SExtraBindArgs{.metadata = {.displayKey = BIND_DISPLAY_KEY}});
    if (!bind)
        return std::unexpected(bind.error());

    m_triggerBinds.emplace_back(Keybinds::mgr()->addBind(std::move(*bind)));
    return {};
}

void CTriggeredGestures::removeTriggerBind(const std::string& trigger, Input::ModifierMask modMask) {
    const auto BIND_DISPLAY_KEY = bindDisplayKey(trigger, modMask);
    std::erase_if(m_triggerBinds, [&](const auto& bind) {
        if (bind->metadata().displayKey != BIND_DISPLAY_KEY)
            return false;

        Keybinds::mgr()->removeBind(bind);
        return true;
    });
}

bool CTriggeredGestures::hasGestureForTrigger(const std::string& trigger, Input::ModifierMask modMask) const {
    return std::ranges::any_of(m_gestures, [&](const auto& gesture) { return gesture->trigger == trigger && gesture->modMask == modMask; });
}

std::expected<void, std::string> CTriggeredGestures::removeGesture(const std::string& trigger, eGestureDeltaSource source, eTrackpadGestureDirection direction,
                                                                   Input::ModifierMask modMask, float deltaScale, bool disableInhibit) {
    const auto IT = std::ranges::find_if(m_gestures, [&](const auto& g) {
        return g->trigger == trigger && g->source == source && g->direction == direction && g->modMask == modMask && g->deltaScale == deltaScale &&
            g->disableInhibit == disableInhibit;
    });

    if (IT == m_gestures.end())
        return std::unexpected("Can't remove a non-existent gesture");

    if (m_activeGesture == *IT)
        pointerGestureEnd(true);

    std::erase(m_gestures, *IT);

    if (!hasGestureForTrigger(trigger, modMask))
        removeTriggerBind(trigger, modMask);

    return {};
}

bool CTriggeredGestures::pointerGestureBegin(const std::string& trigger, Input::ModifierMask modMask) {
    static auto PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");

    if (m_activeGesture || pointerGestureActive() || PROTO::inputCapture->isCaptured())
        return false;

    if (!std::ranges::any_of(m_gestures, [&](const auto& g) {
            return g->trigger == trigger && g->modMask == modMask && (!PROTO::shortcutsInhibit->isInhibited() || *PDISABLEINHIBIT || g->disableInhibit);
        }))
        return false;

    m_gestureFindFailed = false;
    m_currentTotalDelta = {};
    m_activeTrigger     = trigger;
    m_activeModMask     = modMask;
    return true;
}

bool CTriggeredGestures::pointerGestureActive() const {
    return !m_activeTrigger.empty();
}

bool CTriggeredGestures::hasGestureForSource(eGestureDeltaSource source) const {
    if (m_activeDeltaSource != GESTURE_DELTA_SOURCE_NONE)
        return m_activeDeltaSource == source;

    return std::ranges::any_of(m_gestures,
                               [&](const auto& gesture) { return gesture->trigger == m_activeTrigger && gesture->modMask == m_activeModMask && gesture->source == source; });
}

void CTriggeredGestures::pointerGestureUpdate(const IPointer::SSwipeUpdateEvent& e) {
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

bool CTriggeredGestures::axisGestureUpdate(const IPointer::SAxisEvent& e) {
    const auto SOURCE = deltaSourceForAxis(e.source);
    if (!pointerGestureActive() || !SOURCE || !hasGestureForSource(*SOURCE))
        return false;

    if (e.delta == 0)
        return false;

    swipeUpdate({.timeMs = e.timeMs, .delta = e.axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL ? Vector2D{e.delta, 0.0} : Vector2D{0.0, e.delta}}, *SOURCE);
    return true;
}

void CTriggeredGestures::swipeUpdate(const IPointer::SSwipeUpdateEvent& e, eGestureDeltaSource source) {
    static auto PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");

    if (m_gestureFindFailed)
        return;

    if (m_activeDeltaSource == GESTURE_DELTA_SOURCE_NONE)
        m_activeDeltaSource = source;

    m_currentTotalDelta += e.delta;

    if (!m_activeGesture && (std::abs(m_currentTotalDelta.x) < 5 && std::abs(m_currentTotalDelta.y) < 5))
        return;

    if (!m_activeGesture) {
        const auto [axis, direction] = gestureSwipeDirectionForDelta(m_currentTotalDelta);

        for (const auto& g : m_gestures) {
            if (g->trigger != m_activeTrigger || g->modMask != m_activeModMask || g->source != source)
                continue;

            if (g->direction != axis && g->direction != direction && g->direction != TRACKPAD_GESTURE_DIR_SWIPE)
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

void CTriggeredGestures::pointerGestureEnd(bool cancelled) {
    if (!pointerGestureActive())
        return;

    m_activeTrigger.clear();
    m_activeDeltaSource = GESTURE_DELTA_SOURCE_NONE;

    if (m_activeGesture) {
        const IPointer::SSwipeEndEvent event{.cancelled = cancelled};
        m_activeGesture->gesture->end({.swipe = &event, .direction = m_activeGesture->direction, .scale = m_activeGesture->deltaScale});
        m_activeGesture.reset();
    }
}
