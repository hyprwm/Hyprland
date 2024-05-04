#include "Keyboard.hpp"
#include "../defines.hpp"

SP<CKeyboard> CKeyboard::create(wlr_keyboard* keeb) {
    SP<CKeyboard> pKeeb = SP<CKeyboard>(new CKeyboard(keeb));

    pKeeb->self = pKeeb;

    return pKeeb;
}

bool CKeyboard::isVirtual() {
    return false;
}

wlr_keyboard* CKeyboard::wlr() {
    return keyboard;
}

CKeyboard::CKeyboard(wlr_keyboard* keeb) : keyboard(keeb) {
    if (!keeb)
        return;

    // clang-format off
    hyprListener_destroy.initCallback(&keeb->base.events.destroy, [this] (void* owner, void* data) {
        disconnectCallbacks();
        keyboard = nullptr;
	events.destroy.emit();
    }, this, "CKeyboard");

    hyprListener_key.initCallback(&keeb->events.key, [this] (void* owner, void* data) {
        auto E = (wlr_keyboard_key_event*)data;

        keyboardEvents.key.emit(SKeyEvent{
            .timeMs     = E->time_msec,
            .keycode    = E->keycode,
            .updateMods = E->update_state,
            .state      = E->state,
        });
    }, this, "CKeyboard");

    hyprListener_keymap.initCallback(&keeb->events.keymap, [this] (void* owner, void* data) {
        keyboardEvents.keymap.emit();
    }, this, "CKeyboard");

    hyprListener_modifiers.initCallback(&keeb->events.modifiers, [this] (void* owner, void* data) {
        keyboardEvents.modifiers.emit();
    }, this, "CKeyboard");

    hyprListener_repeatInfo.initCallback(&keeb->events.repeat_info, [this] (void* owner, void* data) {
        keyboardEvents.repeatInfo.emit();
    }, this, "CKeyboard");
    // clang-format on

    deviceName = keeb->base.name ? keeb->base.name : "UNKNOWN";
}

void CKeyboard::disconnectCallbacks() {
    hyprListener_destroy.removeCallback();
    hyprListener_key.removeCallback();
    hyprListener_keymap.removeCallback();
    hyprListener_repeatInfo.removeCallback();
    hyprListener_modifiers.removeCallback();
}
