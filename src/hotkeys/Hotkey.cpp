#include "Hotkey.hpp"
#include "Manager.hpp"
#include "../Compositor.hpp"
#include "../devices/IKeyboard.hpp"

#include <array>
#include <format>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>

using namespace Hotkeys;

static bool isModifierKeysym(xkb_keysym_t sym) {
    switch (sym) {
        case XKB_KEY_ISO_Level3_Shift:
        case XKB_KEY_ISO_Level3_Latch:
        case XKB_KEY_ISO_Level3_Lock:
        case XKB_KEY_ISO_Level5_Shift:
        case XKB_KEY_ISO_Level5_Latch:
        case XKB_KEY_ISO_Level5_Lock:
        case XKB_KEY_Mode_switch:
        case XKB_KEY_Num_Lock:
        case XKB_KEY_Scroll_Lock: return true;
        default: break;
    }
    return sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R;
}

static bool isKeypadKeysym(xkb_keysym_t sym) {
    return sym >= XKB_KEY_KP_Space && sym <= XKB_KEY_KP_Equal;
}

static std::string keysymName(xkb_keysym_t sym) {
    std::array<char, 64> name{};
    if (xkb_keysym_get_name(sym, name.data(), name.size()) <= 0)
        return "";
    return name.data();
}

static bool isDeadKeysym(xkb_keysym_t sym) {
    return keysymName(sym).starts_with("dead_");
}

static bool isAuxiliaryButton(uint32_t button) {
    switch (button) {
        case BTN_SIDE:
        case BTN_EXTRA:
        case BTN_FORWARD:
        case BTN_BACK:
        case BTN_TASK: return true;
        default: break;
    }
    return false;
}

static bool keyProducesKeysym(SP<IKeyboard> keyboard, uint32_t keycode, xkb_keysym_t sym) {
    const auto KEYMAP = keyboard->m_xkbKeymap;
    if (!KEYMAP)
        return false;

    const auto LAYOUTS = xkb_keymap_num_layouts_for_key(KEYMAP, keycode);
    for (xkb_layout_index_t layout = 0; layout < LAYOUTS; ++layout) {
        const xkb_keysym_t* syms  = nullptr;
        const int           NSYMS = xkb_keymap_key_get_syms_by_level(KEYMAP, keycode, layout, 0, &syms);
        for (int i = 0; i < NSYMS; ++i) {
            if (syms[i] == sym)
                return true;
        }
    }

    return false;
}

static std::string modsLabel(Input::ModifierMask mods) {
    std::string out;
    if (mods & Input::HL_MODIFIER_META)
        out += "SUPER+";
    if (mods & Input::HL_MODIFIER_CTRL)
        out += "CTRL+";
    if (mods & Input::HL_MODIFIER_ALT)
        out += "ALT+";
    if (mods & Input::HL_MODIFIER_SHIFT)
        out += "SHIFT+";
    return out;
}

static uint32_t nextSerial() {
    return wl_display_next_serial(g_pCompositor->m_wlDisplay);
}

bool STrigger::isTap() const {
    return type == eTriggerType::KEY && isModifierKeysym(code);
}

bool STrigger::permitted() const {
    if (mods & (Input::HL_MODIFIER_CTRL | Input::HL_MODIFIER_ALT | Input::HL_MODIFIER_META))
        return true;

    if (type == eTriggerType::BUTTON)
        return isAuxiliaryButton(code);

    if (isModifierKeysym(code))
        return true;
    if (isKeypadKeysym(code))
        return true;
    if (isDeadKeysym(code))
        return false;
    return xkb_keysym_to_utf32(code) == 0;
}

std::string STrigger::label() const {
    if (type == eTriggerType::BUTTON)
        return modsLabel(mods) + std::format("mouse:{}", code);
    return modsLabel(mods) + keysymName(code);
}

SBindResult CHotkey::commit(const STrigger& trigger, const std::string& appId, const std::string& description) {
    auto normalized = trigger;
    if (normalized.type == eTriggerType::KEY)
        normalized.code = xkb_keysym_to_lower(normalized.code);

    const auto RESULT = mgr()->check(*this, normalized);
    if (RESULT.denied)
        return RESULT;

    if (m_trigger != normalized)
        resetHeld();

    m_trigger     = normalized;
    m_appId       = appId;
    m_description = description;
    m_bound       = true;
    return RESULT;
}

bool CHotkey::bound() const {
    return m_bound;
}

const STrigger& CHotkey::trigger() const {
    return m_trigger;
}

const std::string& CHotkey::appId() const {
    return m_appId;
}

const std::string& CHotkey::description() const {
    return m_description;
}

void CHotkey::revoke(const std::string& message) {
    if (!m_bound)
        return;

    m_bound = false;
    resetHeld();
    m_events.revoked.emit(message);
}

void CHotkey::resetHeld() {
    m_held       = false;
    m_tapPending = false;
    m_heldCode   = 0;
    m_heldDevice.reset();
}

void CHotkey::cancelTap(uint32_t exceptCode, const WP<IHID>& exceptDevice) {
    if (!m_held || !m_tapPending)
        return;
    if (exceptCode && m_heldCode == exceptCode && m_heldDevice == exceptDevice)
        return;

    resetHeld();
}

bool CHotkey::press(uint32_t code, WP<IHID> device, uint32_t timeMs) {
    m_held       = true;
    m_heldCode   = code;
    m_heldDevice = device;

    if (m_trigger.isTap()) {
        m_tapPending = true;
        return false;
    }

    m_events.triggered.emit(nextSerial(), timeMs);
    return true;
}

bool CHotkey::release(uint32_t code, const WP<IHID>& device, uint32_t timeMs) {
    if (!m_held || m_heldCode != code || m_heldDevice != device)
        return false;

    const bool TAP = m_tapPending;
    resetHeld();

    if (TAP)
        m_events.triggered.emit(nextSerial(), timeMs);
    m_events.released.emit(nextSerial(), timeMs);
    return !TAP;
}

bool CHotkey::onKey(uint32_t keycode, SP<IKeyboard> keyboard, bool pressed, Input::ModifierMask mods, uint32_t timeMs, bool locked) {
    const WP<IHID> DEVICE = keyboard;

    if (m_held && m_heldDevice.expired())
        resetHeld();

    if (!pressed)
        return release(keycode, DEVICE, timeMs);

    if (locked || !m_bound || m_held || m_trigger.type != eTriggerType::KEY)
        return false;
    if (m_trigger.mods != mods || !keyProducesKeysym(keyboard, keycode, m_trigger.code))
        return false;

    return press(keycode, DEVICE, timeMs);
}

bool CHotkey::onButton(uint32_t button, SP<IHID> pointer, bool pressed, Input::ModifierMask mods, uint32_t timeMs, bool locked) {
    const WP<IHID> DEVICE = pointer;

    if (m_held && m_heldDevice.expired())
        resetHeld();

    if (!pressed)
        return release(button, DEVICE, timeMs);

    if (locked || !m_bound || m_held || m_trigger.type != eTriggerType::BUTTON)
        return false;
    if (m_trigger.mods != mods || m_trigger.code != button)
        return false;

    return press(button, DEVICE, timeMs);
}
