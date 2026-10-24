#include "Manager.hpp"
#include "../Compositor.hpp"
#include "../event/EventBus.hpp"
#include "../keybinds/Manager.hpp"
#include "../managers/SessionLockManager.hpp"
#include "../managers/input/InputManager.hpp"

#include <format>

using namespace Hotkeys;

static constexpr Input::ModifierMask RELEVANT_MODS = Input::HL_MODIFIER_SHIFT | Input::HL_MODIFIER_CTRL | Input::HL_MODIFIER_ALT | Input::HL_MODIFIER_META;

static constexpr uint32_t            PROTO_MOD_SHIFT = 1;
static constexpr uint32_t            PROTO_MOD_CTRL  = 2;
static constexpr uint32_t            PROTO_MOD_ALT   = 4;
static constexpr uint32_t            PROTO_MOD_SUPER = 8;

static std::string                   keybindLabel(const Keybinds::PBind& bind) {
    const auto& METADATA = bind->metadata();
    if (METADATA.description && !METADATA.description->empty())
        return *METADATA.description;
    if (!METADATA.argument.empty())
        return std::format("{}, {}", METADATA.handler, METADATA.argument);
    return METADATA.handler;
}

Input::ModifierMask Hotkeys::modsFromProtocol(uint32_t mods) {
    Input::ModifierMask out = Input::HL_MODIFIER_NONE;
    if (mods & PROTO_MOD_SHIFT)
        out |= Input::HL_MODIFIER_SHIFT;
    if (mods & PROTO_MOD_CTRL)
        out |= Input::HL_MODIFIER_CTRL;
    if (mods & PROTO_MOD_ALT)
        out |= Input::HL_MODIFIER_ALT;
    if (mods & PROTO_MOD_SUPER)
        out |= Input::HL_MODIFIER_META;
    return out;
}

bool Hotkeys::validProtocolMods(uint32_t mods) {
    return (mods & ~(PROTO_MOD_SHIFT | PROTO_MOD_CTRL | PROTO_MOD_ALT | PROTO_MOD_SUPER)) == 0;
}

CHotkeyManager::CHotkeyManager() {
    m_reloadListener = Event::bus()->m_events.config.reloaded.listen([this] { revokeConflicting(); });
}

SP<CHotkey> CHotkeyManager::create() {
    prune();
    auto hk = makeShared<CHotkey>();
    m_hotkeys.emplace_back(hk);
    return hk;
}

void CHotkeyManager::prune() {
    std::erase_if(m_hotkeys, [](const auto& hk) { return hk.expired(); });
}

SBindResult CHotkeyManager::check(const CHotkey& self, const STrigger& trigger) {
    if (!trigger.permitted()) {
        const auto MESSAGE =
            trigger.type == eTriggerType::BUTTON ? "an unmodified button trigger must be an auxiliary button" : "an unmodified key trigger must not be a key that produces text";
        return {.denied = eDenyReason::NOT_PERMITTED, .message = MESSAGE};
    }

    for (const auto& other : m_hotkeys) {
        const auto HK = other.lock();
        if (!HK || HK.get() == &self || !HK->m_bound)
            continue;
        if (HK->m_trigger == trigger)
            return {.denied = eDenyReason::ALREADY_BOUND, .message = "the trigger is already bound by another hotkey"};
    }

    if (trigger.type == eTriggerType::KEY) {
        if (const auto CONFLICT = Keybinds::mgr()->findConflictingBind(trigger.code, trigger.mods))
            return {.denied = eDenyReason::ALREADY_BOUND, .message = std::format("the trigger is reserved by a compositor keybind ({})", keybindLabel(CONFLICT))};
    }

    return {};
}

void CHotkeyManager::revokeConflicting() {
    prune();

    for (const auto& weak : m_hotkeys) {
        const auto HK = weak.lock();
        if (!HK->m_bound || HK->m_trigger.type != eTriggerType::KEY)
            continue;

        const auto CONFLICT = Keybinds::mgr()->findConflictingBind(HK->m_trigger.code, HK->m_trigger.mods);
        if (!CONFLICT)
            continue;

        HK->revoke(std::format("the trigger is now reserved by a compositor keybind ({})", keybindLabel(CONFLICT)));
    }
}

void CHotkeyManager::cancelTaps(uint32_t exceptCode, const WP<IHID>& exceptDevice) {
    for (const auto& weak : m_hotkeys) {
        if (const auto HK = weak.lock())
            HK->cancelTap(exceptCode, exceptDevice);
    }
}

bool CHotkeyManager::onKey(const IKeyboard::SKeyEvent& event, SP<IKeyboard> keyboard) {
    if (!g_pCompositor->m_sessionActive || !keyboard->m_allowBinds)
        return false;

    prune();

    const auto     KEYCODE  = event.keycode + 8;
    const bool     PRESSED  = event.state == WL_KEYBOARD_KEY_STATE_PRESSED;
    const auto     MODS     = g_pInputManager->getModsFromAllKBs() & RELEVANT_MODS;
    const bool     LOCKED   = g_pSessionLockManager->isSessionLocked();
    const WP<IHID> DEVICE   = keyboard;
    bool           consumed = false;

    cancelTaps(LOCKED ? 0 : KEYCODE, DEVICE);

    for (const auto& weak : m_hotkeys) {
        const auto HK = weak.lock();
        if (HK->onKey(KEYCODE, keyboard, PRESSED, MODS, event.timeMs, LOCKED))
            consumed = true;
    }

    return consumed;
}

bool CHotkeyManager::onButton(const IPointer::SButtonEvent& event, SP<IPointer> pointer) {
    if (!g_pCompositor->m_sessionActive)
        return false;

    prune();

    const bool PRESSED  = event.state == WL_POINTER_BUTTON_STATE_PRESSED;
    const auto MODS     = g_pInputManager->getModsFromAllKBs() & RELEVANT_MODS;
    const bool LOCKED   = g_pSessionLockManager->isSessionLocked();
    bool       consumed = false;

    cancelTaps();

    for (const auto& weak : m_hotkeys) {
        const auto HK = weak.lock();
        if (HK->onButton(event.button, pointer, PRESSED, MODS, event.timeMs, LOCKED))
            consumed = true;
    }

    return consumed;
}

void CHotkeyManager::onAxis() {
    cancelTaps();
}

std::vector<SP<CHotkey>> CHotkeyManager::bound() {
    prune();

    std::vector<SP<CHotkey>> out;
    for (const auto& weak : m_hotkeys) {
        const auto HK = weak.lock();
        if (HK->m_bound)
            out.emplace_back(HK);
    }
    return out;
}

UP<CHotkeyManager>& Hotkeys::mgr() {
    static auto p = makeUnique<CHotkeyManager>();
    return p;
}
