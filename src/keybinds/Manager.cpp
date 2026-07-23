#include "Manager.hpp"
#include "MatchResolver.hpp"

#include "../Compositor.hpp"
#include "../config/ConfigManager.hpp"
#include "../config/ConfigValue.hpp"
#include "../config/shared/actions/ConfigActions.hpp"
#include "../debug/log/Logger.hpp"
#include "../devices/IKeyboard.hpp"
#include "../errorOverlay/Overlay.hpp"
#include "../layout/LayoutManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/SessionLockManager.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../protocols/Hotkey.hpp"
#include "../protocols/InputCapture.hpp"
#include "../protocols/ShortcutsInhibit.hpp"
#include "../protocols/core/DataDevice.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>

#include <algorithm>
#include <cmath>
#include <fcntl.h>
#include <format>
#include <linux/input-event-codes.h>

#if defined(__linux__)
#include <linux/vt.h>
#elif defined(__NetBSD__) || defined(__OpenBSD__)
#include <dev/wscons/wsdisplay_usl_io.h>
#elif defined(__DragonFly__) || defined(__FreeBSD__)
#include <sys/consio.h>
#endif

using namespace Keybinds;

bool CKeybindManager::STimedBatch::empty() const {
    return binds.empty();
}

void CKeybindManager::STimedBatch::clear() {
    binds.clear();
    trigger = {};
    device.reset();
    keyboard.reset();
    hasDevice         = false;
    requiresHeldInput = false;
}

CKeybindManager::CKeybindManager() {
    m_scrollTimer.reset();

    m_longPressTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer>, void*) {
            if (m_longPress.empty())
                return;

            auto keyboard = m_longPress.keyboard.lock();
            if (keyboard && !keyboard->m_allowBinds) {
                m_longPress.clear();
                return;
            }

            const auto device = m_longPress.device.lock();
            if ((m_longPress.hasDevice && !device) || (m_longPress.requiresHeldInput && !m_inputState.find(m_longPress.trigger, device))) {
                m_longPress.clear();
                return;
            }

            const auto BINDS               = m_longPress.binds;
            const auto TRIGGER             = m_longPress.trigger;
            const bool REQUIRES_HELD_INPUT = m_longPress.requiresHeldInput;
            m_longPress.clear();
            for (const auto& weak : BINDS) {
                const auto bind         = weak.lock();
                auto*      pressedInput = m_inputState.find(TRIGGER, device);
                if (bind && canInvokeNow(bind) && (pressedInput || !REQUIRES_HELD_INPUT))
                    invokeBind(bind, true, pressedInput);
            }
        },
        nullptr);

    m_repeatTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void*) {
            if (m_repeat.empty() || m_repeatRate == 0)
                return;

            auto keyboard = m_repeat.keyboard.lock();
            if (keyboard && !keyboard->m_allowBinds) {
                m_repeat.clear();
                return;
            }

            const auto device = m_repeat.device.lock();
            if ((m_repeat.hasDevice && !device) || (m_repeat.requiresHeldInput && !m_inputState.find(m_repeat.trigger, device))) {
                m_repeat.clear();
                return;
            }

            std::erase_if(m_repeat.binds, [](const auto& weak) { return !weak || !weak->enabled(); });
            const auto BINDS               = m_repeat.binds;
            const auto TRIGGER             = m_repeat.trigger;
            const auto GENERATION          = m_repeatGeneration;
            const bool REQUIRES_HELD_INPUT = m_repeat.requiresHeldInput;
            for (const auto& weak : BINDS) {
                auto* pressedInput = m_inputState.find(TRIGGER, device);
                if (!pressedInput && REQUIRES_HELD_INPUT)
                    break;

                if (const auto bind = weak.lock(); bind && canInvokeNow(bind))
                    invokeBind(bind, true, pressedInput);
            }

            if (!m_repeat.empty() && GENERATION == m_repeatGeneration)
                self->updateTimeout(std::chrono::milliseconds(1000 / m_repeatRate));
        },
        nullptr);

    if (g_pEventLoopManager) {
        g_pEventLoopManager->addTimer(m_longPressTimer);
        g_pEventLoopManager->addTimer(m_repeatTimer);
        m_timersRegistered = true;
    }
}

CKeybindManager::~CKeybindManager() {
    if (m_xkbTranslationState)
        xkb_state_unref(m_xkbTranslationState);

    if (!g_pEventLoopManager || !m_timersRegistered)
        return;

    if (m_longPressTimer)
        g_pEventLoopManager->removeTimer(m_longPressTimer);
    if (m_repeatTimer)
        g_pEventLoopManager->removeTimer(m_repeatTimer);
}

PBind CKeybindManager::addBind(CBind&& bind) {
    cancelTimedBinds();
    return m_registry.add(std::move(bind));
}

bool CKeybindManager::removeBind(const PBind& bind) {
    cancelTimedBinds();
    invokeReleaseCallbacks(m_inputState.takeReleaseCallbacks(bind));
    m_shadowed.erase(bind);
    return m_registry.remove(bind);
}

size_t CKeybindManager::removeBinds(std::string_view displayKey) {
    cancelTimedBinds();
    for (const auto& bind : m_registry.findByDisplayKey(displayKey))
        invokeReleaseCallbacks(m_inputState.takeReleaseCallbacks(bind));
    const auto REMOVED = m_registry.removeByDisplayKey(displayKey);
    shadowBinds();
    return REMOVED;
}

void CKeybindManager::clearBinds() {
    cancelTimedBinds();
    invokeReleaseCallbacks(m_inputState.takeAllReleaseCallbacks());
    m_shadowed.clear();
    m_registry.clear();
}

bool CKeybindManager::onKeyEvent(std::any event, SP<IKeyboard> keyboard) {
    if (!g_pCompositor->m_sessionActive) {
        invokeReleaseCallbacks(m_inputState.takeAllReleaseCallbacks());
        m_inputState.clear();
        cancelTimedBinds();
        return true;
    }

    if (!keyboard->m_allowBinds)
        return true;

    if (!m_xkbTranslationState) {
        updateXKBTranslationState();
        if (!m_xkbTranslationState)
            return true;
    }

    const auto KEY_EVENT = std::any_cast<IKeyboard::SKeyEvent>(event);
    const auto KEYCODE   = KEY_EVENT.keycode + 8;
    const auto KEYSYM    = xkb_state_key_get_one_sym(keyboard->m_resolveBindsBySym ? keyboard->m_xkbSymState : m_xkbTranslationState, KEYCODE);
    const auto INTERNAL  = xkb_state_key_get_one_sym(keyboard->m_xkbState, KEYCODE);

    if (KEYSYM == XKB_KEY_Escape || INTERNAL == XKB_KEY_Escape)
        PROTO::data->abortDndIfPresent();

    if (!PROTO::inputCapture->isCaptured() && handleInternalKeybinds(INTERNAL))
        return false;

    const auto MODIFIERS = g_pInputManager->getModsFromAllKBs();
    if (PROTO::hotkey && PROTO::hotkey->onKey(KEYSYM, MODIFIERS, KEYCODE, KEY_EVENT.state == WL_KEYBOARD_KEY_STATE_PRESSED, KEY_EVENT.timeMs))
        return false;

    Config::Actions::state()->m_timeLastMs    = KEY_EVENT.timeMs;
    Config::Actions::state()->m_lastCode      = KEYCODE;
    Config::Actions::state()->m_lastMouseCode = 0;

    const bool DRAG_WAS_ACTIVE = g_layoutManager->endDragTarget();
    cancelTimedBinds();

    const auto MODIFIER = keycodeToModifier(KEYCODE);
    const auto KEY      = SResolvedKey{
             .sym      = KEYSYM,
             .code     = KEYCODE,
             .modifier = MODIFIER == 0 ? std::nullopt : std::optional{sc<eKeyboardModifiers>(MODIFIER)},
    };

    if (KEY_EVENT.state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        if (!m_inputState.press({
                .key              = KEY,
                .modifiersAtPress = MODIFIERS,
                .forwarded        = true,
                .capturedAtPress  = PROTO::inputCapture->isCaptured(),
                .actionCode       = KEYCODE,
                .actionTimeMs     = KEY_EVENT.timeMs,
                .submapAtPress    = std::string{currentSubmap()},
                .positionAtPress  = g_pInputManager->getMouseCoordsInternal(),
                .device           = keyboard,
            })) {
            const auto* existing = m_inputState.find(KEY, keyboard);
            return (!existing || existing->forwarded) && !DRAG_WAS_ACTIVE;
        }

        auto* pressedInput = m_inputState.find(KEY, keyboard);
        auto  result       = processEvent(
            {
                       .heldKeys         = m_inputState.heldKeys(),
                       .trigger          = KEY,
                       .modifiersNow     = MODIFIERS,
                       .modifiersAtPress = MODIFIERS,
                       .pressed          = true,
                       .device           = keyboard,
                       .submap           = std::string{currentSubmap()},
            },
            keyboard, pressedInput);

        const bool FORWARDED = result.passEvent;
        m_inputState.setForwarded(KEY, keyboard, FORWARDED);
        if (!FORWARDED)
            shadowBinds(KEY, keyboard);

        return FORWARDED && !DRAG_WAS_ACTIVE;
    }

    auto* pressedInput = m_inputState.find(KEY, keyboard);
    if (!pressedInput) {
        Log::logger->log(Log::ERR, "Keybind release had no matching press record");
        const auto result = processEvent(
            {
                .heldKeys     = m_inputState.heldKeys(),
                .trigger      = KEY,
                .modifiersNow = MODIFIERS,
                .pressed      = false,
                .device       = keyboard,
                .submap       = std::string{currentSubmap()},
            },
            keyboard);
        return result.passEvent && !DRAG_WAS_ACTIVE;
    }

    const bool FORWARDED = pressedInput->forwarded;
    invokeDeferredBinds(pressedInput->key, keyboard, keyboard);
    pressedInput = m_inputState.find(KEY, keyboard);
    if (!pressedInput) {
        shadowBinds();
        return FORWARDED && !DRAG_WAS_ACTIVE;
    }

    processEvent(
        {
            .heldKeys         = m_inputState.heldKeys(),
            .trigger          = pressedInput->key,
            .modifiersNow     = MODIFIERS,
            .modifiersAtPress = pressedInput->modifiersAtPress,
            .pressed          = false,
            .device           = keyboard,
            .submap           = pressedInput->submapAtPress,
        },
        keyboard, pressedInput);
    m_inputState.release(KEY, keyboard);
    shadowBinds();
    return FORWARDED && !DRAG_WAS_ACTIVE;
}

bool CKeybindManager::onAxisEvent(const IPointer::SAxisEvent& event, SP<IPointer> pointer) {
    static auto PDELAY = CConfigValue<Config::INTEGER>("binds:scroll_event_delay");

    if (m_scrollTimer.getMillis() < *PDELAY)
        return true;

    m_scrollTimer.reset();
    cancelTimedBinds();

    std::optional<std::string> name;
    if (event.source == WL_POINTER_AXIS_SOURCE_WHEEL && event.axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
        name = event.delta > 0 ? "mouse_down" : "mouse_up";
    else if (event.source == WL_POINTER_AXIS_SOURCE_WHEEL && event.axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
        name = event.delta < 0 ? "mouse_left" : "mouse_right";

    if (!name)
        return true;

    const SResolvedKey KEY{.event = name};
    const auto         RESULT = processEvent(
        {
                    .heldKeys     = m_inputState.heldKeys(),
                    .trigger      = KEY,
                    .modifiersNow = sc<ModifierMask>(g_pInputManager->getModsFromAllKBs()),
                    .pressed      = true,
                    .device       = pointer,
                    .submap       = std::string{currentSubmap()},
        },
        nullptr);

    return RESULT.passEvent;
}

bool CKeybindManager::onMouseEvent(const IPointer::SButtonEvent& event, SP<IPointer> pointer, bool captured) {
    const auto MODIFIERS = g_pInputManager->getModsFromAllKBs();
    const auto KEY       = SResolvedKey{.event = "mouse:" + std::to_string(event.button)};

    Config::Actions::state()->m_lastMouseCode = event.button;
    Config::Actions::state()->m_lastCode      = 0;
    Config::Actions::state()->m_timeLastMs    = event.timeMs;

    const bool DRAG_WAS_ACTIVE = captured ? false : g_layoutManager->endDragTarget();
    cancelTimedBinds();

    if (event.state == WL_POINTER_BUTTON_STATE_PRESSED) {
        if (!m_inputState.press({
                .key              = KEY,
                .modifiersAtPress = MODIFIERS,
                .forwarded        = true,
                .capturedAtPress  = captured,
                .actionMouseCode  = event.button,
                .actionTimeMs     = event.timeMs,
                .submapAtPress    = std::string{currentSubmap()},
                .positionAtPress  = g_pInputManager->getMouseCoordsInternal(),
                .device           = pointer,
            })) {
            const auto* existing = m_inputState.find(KEY, pointer);
            return (!existing || existing->forwarded) && !DRAG_WAS_ACTIVE;
        }

        auto* pressedInput = m_inputState.find(KEY, pointer);
        auto  result       = processEvent(
            {
                       .heldKeys         = m_inputState.heldKeys(),
                       .trigger          = KEY,
                       .modifiersNow     = MODIFIERS,
                       .modifiersAtPress = MODIFIERS,
                       .pressed          = true,
                       .device           = pointer,
                       .submap           = std::string{currentSubmap()},
            },
            nullptr, pressedInput);

        m_inputState.setForwarded(KEY, pointer, result.passEvent);
        if (!result.passEvent)
            shadowBinds();
        return result.passEvent && !DRAG_WAS_ACTIVE;
    }

    auto* pressedInput = m_inputState.find(KEY, pointer);
    if (!pressedInput)
        return !DRAG_WAS_ACTIVE;

    const bool FORWARDED = pressedInput->forwarded;
    invokeDeferredBinds(pressedInput->key, pointer);
    pressedInput = m_inputState.find(KEY, pointer);
    if (!pressedInput) {
        shadowBinds();
        return FORWARDED && !DRAG_WAS_ACTIVE;
    }

    processEvent(
        {
            .heldKeys         = m_inputState.heldKeys(),
            .trigger          = KEY,
            .modifiersNow     = MODIFIERS,
            .modifiersAtPress = pressedInput->modifiersAtPress,
            .pressed          = false,
            .device           = pointer,
            .submap           = pressedInput->submapAtPress,
        },
        nullptr, pressedInput);
    m_inputState.release(KEY, pointer);
    shadowBinds();
    return FORWARDED && !DRAG_WAS_ACTIVE;
}

void CKeybindManager::onSwitchEvent(const std::string& switchName) {
    const SResolvedKey KEY{.event = "switch:" + switchName};
    cancelTimedBinds();
    processEvent({.heldKeys = m_inputState.heldKeys(), .trigger = KEY, .pressed = true, .submap = std::string{currentSubmap()}}, nullptr);
}

void CKeybindManager::onSwitchOnEvent(const std::string& switchName) {
    const SResolvedKey KEY{.event = "switch:on:" + switchName};
    cancelTimedBinds();
    processEvent({.heldKeys = m_inputState.heldKeys(), .trigger = KEY, .pressed = true, .submap = std::string{currentSubmap()}}, nullptr);
}

void CKeybindManager::onSwitchOffEvent(const std::string& switchName) {
    const SResolvedKey KEY{.event = "switch:off:" + switchName};
    cancelTimedBinds();
    processEvent({.heldKeys = m_inputState.heldKeys(), .trigger = KEY, .pressed = true, .submap = std::string{currentSubmap()}}, nullptr);
}

void CKeybindManager::onDeviceRemoved(const SP<IHID>& device) {
    cancelTimedBinds();
    invokeReleaseCallbacks(m_inputState.takeReleaseCallbacksForDevice(device));
    m_inputState.clearDevice(device);
    shadowBinds();
}

SBindResult CKeybindManager::processEvent(const SBindEventContext& context, const SP<IKeyboard>& keyboard, SPressedInput* pressedInput) {
    static auto                      PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");
    static auto                      PDRAGTHRESHOLD  = CConfigValue<Config::INTEGER>("binds:drag_threshold");

    const bool                       INHIBITED = !*PDISABLEINHIBIT && PROTO::shortcutsInhibit->isInhibited();
    bool                             claimed   = false;
    SBindResult                      aggregate;
    std::vector<PBind>               hits;
    std::vector<PBind>               longPressHits;
    std::vector<PBind>               catchAlls;
    std::vector<SBindMatchCandidate> candidates;
    std::unordered_set<WP<CBind>>    releaseFollowUps;
    const auto                       INPUT_KEY    = pressedInput ? std::optional{pressedInput->key} : std::nullopt;
    const auto                       INPUT_DEVICE = pressedInput ? pressedInput->device.lock() : nullptr;

    if (!context.pressed && pressedInput) {
        for (const auto& release : m_inputState.takeReleaseCallbacks(pressedInput->key, pressedInput->device.lock())) {
            hits.emplace_back(release.bind);
            releaseFollowUps.emplace(release.bind);
        }
    }

    for (const auto& bind : m_registry.binds()) {
        if (!canInvokeNow(bind))
            continue;
        if (!context.pressed && pressedInput && pressedInput->capturedAtPress && !bind->hasFlag(BIND_FLAG_ALLOW_INPUT_CAPTURE))
            continue;
        if (!context.pressed && pressedInput && isSuppressed(*pressedInput, bind))
            continue;
        if (!context.pressed && bind->hasFlag(BIND_FLAG_RELEASE) && bind->chordSize() > 1 &&
            (!pressedInput || std::ranges::none_of(pressedInput->armedReleaseBinds, [&bind](const auto& weak) { return weak.lock() == bind; })))
            continue;

        if (bind->hasFlag(BIND_FLAG_CATCH_ALL)) {
            if (context.pressed && bind->matchesContext(context))
                catchAlls.emplace_back(bind);
            continue;
        }

        const auto MATCH = bind->matches(context);

        if (MATCH == BIND_MATCH_NONE)
            continue;

        if (MATCH == BIND_MATCH_FULL && !context.pressed && pressedInput) {
            const bool THRESHOLD_REACHED = pressedInput->positionAtPress.distanceSq(g_pInputManager->getMouseCoordsInternal()) > std::pow(*PDRAGTHRESHOLD, 2);
            if (bind->hasFlag(BIND_FLAG_CLICK) && (g_layoutManager->dragController()->dragThresholdReached() || THRESHOLD_REACHED))
                continue;
            if (bind->hasFlag(BIND_FLAG_DRAG) && !g_layoutManager->dragController()->dragThresholdReached() && !THRESHOLD_REACHED)
                continue;
        }

        candidates.emplace_back(bind, MATCH);
    }

    const auto RESOLUTION = resolveChordMatches(candidates, context);

    for (const auto& bind : RESOLUTION.partial) {
        if (context.pressed && pressedInput && bind->hasFlag(BIND_FLAG_RELEASE) && bind->chordSize() > 1 && bind->isFullyHeld(context)) {
            if (std::ranges::none_of(pressedInput->armedReleaseBinds, [&bind](const auto& weak) { return weak.lock() == bind; }))
                pressedInput->armedReleaseBinds.emplace_back(bind);
            suppressSubChords(bind, context);
        }

        if (bind->hasFlag(BIND_FLAG_RELEASE) && context.pressed && (bind->hasFlag(BIND_FLAG_NON_CONSUMING) || bind->hasFlag(BIND_FLAG_AUTO_CONSUMING)))
            continue;

        claimed = true;
    }

    if (context.pressed && pressedInput) {
        for (const auto& bind : RESOLUTION.deferred) {
            if (bind->chordSize() > 1)
                suppressSubChords(bind, context);

            if (std::ranges::none_of(pressedInput->deferredBinds, [&bind](const auto& weak) { return weak.lock() == bind; }))
                pressedInput->deferredBinds.emplace_back(bind);
        }
        claimed |= !RESOLUTION.deferred.empty();
    }

    for (const auto& bind : RESOLUTION.immediate) {
        if (bind->chordSize() > 1)
            suppressSubChords(bind, context);

        if (context.pressed && bind->hasFlag(BIND_FLAG_LONG_PRESS)) {
            longPressHits.emplace_back(bind);
            if (!bind->hasFlag(BIND_FLAG_NON_CONSUMING) && !bind->hasFlag(BIND_FLAG_AUTO_CONSUMING))
                claimed = true;
            continue;
        }

        if (!std::ranges::contains(hits, bind))
            hits.emplace_back(bind);
    }

    if (candidates.empty() && hits.empty()) {
        for (const auto& bind : catchAlls)
            hits.emplace_back(bind);
    }

    if (!longPressHits.empty())
        scheduleLongPress(longPressHits, context, keyboard, pressedInput != nullptr);

    std::vector<PBind> repeatHits;
    for (const auto& bind : hits) {
        if (!releaseFollowUps.contains(bind) && !canInvokeNow(bind))
            continue;

        auto* livePressedInput = INPUT_KEY ? m_inputState.find(*INPUT_KEY, INPUT_DEVICE) : nullptr;
        auto  result           = invokeBind(bind, context.pressed, livePressedInput);
        if (!result.success) {
            aggregate.success = false;
            if (!result.error.empty()) {
                if (!aggregate.error.empty())
                    aggregate.error += "; ";
                aggregate.error += result.error;
            }
        }

        const bool CONSUMES = !bind->hasFlag(BIND_FLAG_NON_CONSUMING) && (!bind->hasFlag(BIND_FLAG_AUTO_CONSUMING) || result.success) && !result.passEvent;
        claimed |= CONSUMES;

        if (context.pressed && bind->hasFlag(BIND_FLAG_REPEAT) && canInvokeNow(bind))
            repeatHits.emplace_back(bind);
    }

    if (!repeatHits.empty())
        scheduleRepeat(repeatHits, context, keyboard, pressedInput != nullptr);

    if (!PROTO::inputCapture->isCaptured())
        g_layoutManager->dragController()->resetDragThresholdReached();
    aggregate.passEvent = !claimed;
    if (!claimed && INHIBITED) {
        aggregate.success = false;
        if (aggregate.error.empty())
            aggregate.error = "Keybind handling is disabled due to an inhibitor";
    }

    return aggregate;
}

SBindResult CKeybindManager::invokeBind(const PBind& bind, bool pressed, SPressedInput* pressedInput) {
    const auto INPUT_KEY    = pressedInput ? std::optional{pressedInput->key} : std::nullopt;
    const auto INPUT_DEVICE = pressedInput ? pressedInput->device.lock() : nullptr;

    auto&      actionState      = *Config::Actions::state();
    const auto PREVIOUS_PRESSED = actionState.m_passPressed;
    const bool OUTERMOST        = actionState.m_bindInvocationDepth++ == 0;
    if (OUTERMOST)
        actionState.m_requestBindRelease = false;
    actionState.m_passPressed = sc<int>(pressed);
    Hyprutils::Utils::CScopeGuard guard([PREVIOUS_PRESSED, OUTERMOST] {
        auto& state         = *Config::Actions::state();
        state.m_passPressed = PREVIOUS_PRESSED;
        --state.m_bindInvocationDepth;
        if (OUTERMOST)
            state.m_requestBindRelease = false;
    });

    const std::string             SUBMAP_BEFORE{currentSubmap()};
    auto                          result = bind->invoke();
    if (actionState.m_requestBindRelease)
        result.followUp = BIND_FOLLOW_UP_TRIGGER_RELEASE;

    if (INPUT_KEY && pressed && result.followUp == BIND_FOLLOW_UP_TRIGGER_RELEASE) {
        if (auto* input = m_inputState.find(*INPUT_KEY, INPUT_DEVICE);
            input && !std::ranges::any_of(input->releaseCallbacks, [&bind](const auto& weak) { return weak.lock() == bind; }))
            input->releaseCallbacks.emplace_back(bind);
    } else if (!INPUT_KEY && pressed && result.followUp == BIND_FOLLOW_UP_TRIGGER_RELEASE)
        invokeBind(bind, false);

    const auto& RESET = bind->metadata().submapReset;
    if (!RESET.empty() && SUBMAP_BEFORE == currentSubmap())
        Config::Actions::setSubmap(RESET);

    return result;
}

void CKeybindManager::invokeReleaseCallbacks(std::vector<SPendingRelease>&& releases) {
    for (const auto& release : releases) {
        auto&      state              = *Config::Actions::state();
        const auto PREVIOUS_CODE      = state.m_lastCode;
        const auto PREVIOUS_MOUSECODE = state.m_lastMouseCode;
        const auto PREVIOUS_TIME      = state.m_timeLastMs;
        state.m_lastCode              = release.actionCode;
        state.m_lastMouseCode         = release.actionMouseCode;
        state.m_timeLastMs            = release.actionTimeMs;
        Hyprutils::Utils::CScopeGuard guard([&state, PREVIOUS_CODE, PREVIOUS_MOUSECODE, PREVIOUS_TIME] {
            state.m_lastCode      = PREVIOUS_CODE;
            state.m_lastMouseCode = PREVIOUS_MOUSECODE;
            state.m_timeLastMs    = PREVIOUS_TIME;
        });
        invokeBind(release.bind, false, m_inputState.find(release.key, release.device.lock()));
    }
}

void CKeybindManager::invokeDeferredBinds(const SResolvedKey& key, const SP<IHID>& device, const SP<IKeyboard>& keyboard) {
    const auto KEY    = key;
    const auto DEVICE = device;
    auto*      input  = m_inputState.find(KEY, DEVICE);
    if (!input)
        return;

    const auto PRESSED_AT = input->actionTimeMs;
    auto       binds      = std::move(input->deferredBinds);
    input->deferredBinds.clear();

    for (const auto& weak : binds) {
        const auto bind = weak.lock();
        if (!bind || !canInvokeNow(bind))
            continue;

        if (bind->hasFlag(BIND_FLAG_LONG_PRESS)) {
            const auto ACTIVE_KEYBOARD = keyboard ? keyboard : g_pSeatManager->m_keyboard.lock();
            const auto ELAPSED         = Config::Actions::state()->m_timeLastMs - PRESSED_AT;
            if (!ACTIVE_KEYBOARD || ELAPSED < sc<uint32_t>(std::max(ACTIVE_KEYBOARD->m_repeatDelay, 0)))
                continue;
        }

        input = m_inputState.find(KEY, DEVICE);
        if (!input)
            return;

        if (bind->chordSize() > 1) {
            const SBindEventContext context{
                .heldKeys = m_inputState.heldKeys(),
                .trigger  = KEY,
            };
            suppressSubChords(bind, context);
            input = m_inputState.find(KEY, DEVICE);
            if (!input)
                return;
        }

        invokeBind(bind, true, input);
    }
}

void CKeybindManager::suppressSubChords(const PBind& completed, const SBindEventContext& context) {
    std::vector<PBind> suppressed;
    for (const auto& bind : m_registry.binds()) {
        if (bind == completed || !bind->isSubChordOf(*completed, context))
            continue;

        suppressed.emplace_back(bind);
    }

    for (auto& input : m_inputState.pressed()) {
        if (!completed->containsKey(input.key))
            continue;

        for (const auto& bind : suppressed) {
            if (!bind->containsKey(input.key))
                continue;

            std::erase_if(input.deferredBinds, [&bind](const auto& weak) { return !weak || weak.lock() == bind; });
            if (std::ranges::none_of(input.suppressedBinds, [&bind](const auto& weak) { return weak.lock() == bind; }))
                input.suppressedBinds.emplace_back(bind);
        }
    }
}

bool CKeybindManager::isSuppressed(const SPressedInput& input, const PBind& bind) const {
    return std::ranges::any_of(input.suppressedBinds, [&bind](const auto& weak) { return weak.lock() == bind; });
}

void CKeybindManager::cancelTimedBinds() {
    ++m_repeatGeneration;
    m_longPress.clear();
    m_repeat.clear();
    if (m_longPressTimer && g_pEventLoopManager)
        m_longPressTimer->updateTimeout(std::nullopt);
    if (m_repeatTimer && g_pEventLoopManager)
        m_repeatTimer->updateTimeout(std::nullopt);
}

void CKeybindManager::scheduleLongPress(const std::vector<PBind>& binds, const SBindEventContext& context, const SP<IKeyboard>& keyboard, bool requiresHeldInput) {
    const auto ACTIVE_KEYBOARD = keyboard ? keyboard : g_pSeatManager->m_keyboard.lock();
    if (!m_timersRegistered || !ACTIVE_KEYBOARD || !context.trigger)
        return;

    m_longPress.clear();
    for (const auto& bind : binds)
        m_longPress.binds.emplace_back(bind);
    m_longPress.trigger           = *context.trigger;
    m_longPress.device            = context.device;
    m_longPress.keyboard          = ACTIVE_KEYBOARD;
    m_longPress.hasDevice         = !!context.device;
    m_longPress.requiresHeldInput = requiresHeldInput;
    m_longPressTimer->updateTimeout(std::chrono::milliseconds(ACTIVE_KEYBOARD->m_repeatDelay));
}

void CKeybindManager::scheduleRepeat(const std::vector<PBind>& binds, const SBindEventContext& context, const SP<IKeyboard>& keyboard, bool requiresHeldInput) {
    const auto ACTIVE_KEYBOARD = keyboard ? keyboard : g_pSeatManager->m_keyboard.lock();
    if (!m_timersRegistered || !ACTIVE_KEYBOARD || !context.trigger || ACTIVE_KEYBOARD->m_repeatRate <= 0)
        return;

    m_repeat.clear();
    for (const auto& bind : binds)
        m_repeat.binds.emplace_back(bind);
    m_repeat.trigger           = *context.trigger;
    m_repeat.device            = context.device;
    m_repeat.keyboard          = ACTIVE_KEYBOARD;
    m_repeat.hasDevice         = !!context.device;
    m_repeat.requiresHeldInput = requiresHeldInput;
    m_repeatRate               = sc<uint32_t>(ACTIVE_KEYBOARD->m_repeatRate);
    ++m_repeatGeneration;
    m_repeatTimer->updateTimeout(std::chrono::milliseconds(ACTIVE_KEYBOARD->m_repeatDelay));
}

void CKeybindManager::shadowBinds(const std::optional<SResolvedKey>& excluded, const SP<IHID>& excludedDevice) {
    m_shadowed.clear();
    if (!g_pInputManager)
        return;

    const auto MODIFIERS = sc<ModifierMask>(g_pInputManager->getModsFromAllKBs());

    for (const auto& bind : m_registry.binds()) {
        if (!bind->enabled() || bind->hasFlag(BIND_FLAG_TRANSPARENT))
            continue;
        if (std::ranges::any_of(m_inputState.pressed(),
                                [&bind](const auto& input) { return std::ranges::any_of(input.deferredBinds, [&bind](const auto& weak) { return weak.lock() == bind; }); }))
            continue;

        for (const auto& input : m_inputState.pressed()) {
            const auto device = input.device.lock();
            if (!device)
                continue;

            if (excluded && device == excludedDevice && input.key.code == excluded->code && input.key.event == excluded->event)
                continue;

            if (bind->matches({
                    .heldKeys         = m_inputState.heldKeys(),
                    .trigger          = input.key,
                    .modifiersNow     = MODIFIERS,
                    .modifiersAtPress = input.modifiersAtPress,
                    .pressed          = true,
                    .device           = device,
                    .submap           = input.submapAtPress,
                }) == BIND_MATCH_FULL) {
                m_shadowed.emplace(bind);
                break;
            }
        }
    }
}

PBind CKeybindManager::findConflictingBind(xkb_keysym_t keysym, ModifierMask modifiers) const {
    return m_registry.findShortcutConflict(keysym, modifiers, m_xkbTranslationState);
}

CRegistry& CKeybindManager::registry() {
    return m_registry;
}

const CRegistry& CKeybindManager::registry() const {
    return m_registry;
}

CInputState& CKeybindManager::inputState() {
    return m_inputState;
}

const CInputState& CKeybindManager::inputState() const {
    return m_inputState;
}

std::string_view CKeybindManager::currentSubmap() const {
    return Config::Actions::state()->m_currentSubmap;
}

uint32_t CKeybindManager::keycodeToModifier(xkb_keycode_t keycode) const {
    if (keycode < 8)
        return 0;

    switch (keycode - 8) {
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA: return HL_MODIFIER_META;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: return HL_MODIFIER_SHIFT;
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: return HL_MODIFIER_CTRL;
        case KEY_LEFTALT:
        case KEY_RIGHTALT: return HL_MODIFIER_ALT;
        case KEY_CAPSLOCK: return HL_MODIFIER_CAPS;
        case KEY_NUMLOCK: return HL_MODIFIER_MOD2;
        default: return 0;
    }
}

void CKeybindManager::updateXKBTranslationState() {
    if (m_xkbTranslationState) {
        xkb_state_unref(m_xkbTranslationState);
        m_xkbTranslationState = nullptr;
    }

    static auto       PFILEPATH = CConfigValue<std::string>("input:kb_file");
    static auto       PRULES    = CConfigValue<std::string>("input:kb_rules");
    static auto       PMODEL    = CConfigValue<std::string>("input:kb_model");
    static auto       PLAYOUT   = CConfigValue<std::string>("input:kb_layout");
    static auto       PVARIANT  = CConfigValue<std::string>("input:kb_variant");
    static auto       POPTIONS  = CConfigValue<std::string>("input:kb_options");

    const std::string FILEPATH = std::string{*PFILEPATH} == STRVAL_EMPTY ? "" : *PFILEPATH;
    const std::string RULES    = std::string{*PRULES} == STRVAL_EMPTY ? "" : *PRULES;
    const std::string MODEL    = std::string{*PMODEL} == STRVAL_EMPTY ? "" : *PMODEL;
    const std::string LAYOUT   = std::string{*PLAYOUT} == STRVAL_EMPTY ? "" : *PLAYOUT;
    const std::string VARIANT  = std::string{*PVARIANT} == STRVAL_EMPTY ? "" : *PVARIANT;
    const std::string OPTIONS  = std::string{*POPTIONS} == STRVAL_EMPTY ? "" : *POPTIONS;

    xkb_rule_names    rules   = {.rules = RULES.c_str(), .model = MODEL.c_str(), .layout = LAYOUT.c_str(), .variant = VARIANT.c_str(), .options = OPTIONS.c_str()};
    const auto        CONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    FILE* const       FILE    = FILEPATH.empty() ? nullptr : fopen(absolutePath(FILEPATH, Config::mgr()->currentConfigPath()).c_str(), "r");
    auto              keymap  = FILE ? xkb_keymap_new_from_file(CONTEXT, FILE, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS) :
                                       xkb_keymap_new_from_names2(CONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (FILE)
        fclose(FILE);

    if (!keymap) {
        ErrorOverlay::overlay()->queueCreate(std::format("[Runtime Error] Invalid keyboard layout: {} ({})", LAYOUT, VARIANT), ErrorOverlay::Colors::ERROR);
        rules  = {};
        keymap = xkb_keymap_new_from_names2(CONTEXT, &rules, XKB_KEYMAP_FORMAT_TEXT_V2, XKB_KEYMAP_COMPILE_NO_FLAGS);
    }

    xkb_context_unref(CONTEXT);
    if (!keymap)
        return;

    m_xkbTranslationState = xkb_state_new(keymap);
    xkb_keymap_unref(keymap);
}

bool CKeybindManager::handleVT(xkb_keysym_t keysym) {
    if (keysym < XKB_KEY_XF86Switch_VT_1 || keysym > XKB_KEY_XF86Switch_VT_12)
        return false;

    if (g_pCompositor->m_aqBackend->hasSession())
        g_pCompositor->m_aqBackend->session->switchVT(keysym - XKB_KEY_XF86Switch_VT_1 + 1);
    return true;
}

bool CKeybindManager::handleInternalKeybinds(xkb_keysym_t keysym) {
    if (handleVT(keysym))
        return true;

    if (g_pInputManager->getClickMode() != CLICKMODE_KILL)
        return false;

    if (keysym != XKB_KEY_Escape)
        return false;

    g_pInputManager->setClickMode(CLICKMODE_DEFAULT);
    return true;
}

bool CKeybindManager::canInvokeNow(const PBind& bind) const {
    static auto PDISABLEINHIBIT = CConfigValue<Config::INTEGER>("binds:disable_keybind_grabbing");

    if (!bind->enabled() || !m_registry.contains(bind) || m_shadowed.contains(bind))
        return false;
    if (PROTO::inputCapture->isCaptured() && !bind->hasFlag(BIND_FLAG_ALLOW_INPUT_CAPTURE))
        return false;
    if (g_pSessionLockManager->isSessionLocked() && !bind->hasFlag(BIND_FLAG_LOCKED))
        return false;
    return *PDISABLEINHIBIT || !PROTO::shortcutsInhibit->isInhibited() || bind->hasFlag(BIND_FLAG_DONT_INHIBIT);
}

UP<CKeybindManager>& Keybinds::mgr() {
    static auto p = makeUnique<CKeybindManager>();
    return p;
}
