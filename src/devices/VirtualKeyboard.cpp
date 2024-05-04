#include "VirtualKeyboard.hpp"
#include "../defines.hpp"
#include "../protocols/VirtualKeyboard.hpp"

SP<CVirtualKeyboard> CVirtualKeyboard::create(SP<CVirtualKeyboardV1Resource> keeb) {
    SP<CVirtualKeyboard> pKeeb = SP<CVirtualKeyboard>(new CVirtualKeyboard(keeb));

    pKeeb->self = pKeeb;

    return pKeeb;
}

CVirtualKeyboard::CVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keeb_) : keyboard(keeb_) {
    if (!keeb_)
        return;

    auto keeb = keeb_->wlr();

    // clang-format off
    hyprListener_destroy.initCallback(&keeb->base.events.destroy, [this] (void* owner, void* data) {
        disconnectCallbacks();
        keyboard.reset();
	events.destroy.emit();
    }, this, "CVirtualKeyboard");

    hyprListener_key.initCallback(&keeb->events.key, [this] (void* owner, void* data) {
        auto E = (wlr_keyboard_key_event*)data;

        keyboardEvents.key.emit(SKeyEvent{
            .timeMs     = E->time_msec,
            .keycode    = E->keycode,
            .updateMods = E->update_state,
            .state      = E->state,
        });
    }, this, "CVirtualKeyboard");

    hyprListener_keymap.initCallback(&keeb->events.keymap, [this] (void* owner, void* data) {
        keyboardEvents.keymap.emit();
    }, this, "CVirtualKeyboard");

    hyprListener_modifiers.initCallback(&keeb->events.modifiers, [this] (void* owner, void* data) {
        keyboardEvents.modifiers.emit();
    }, this, "CVirtualKeyboard");

    hyprListener_repeatInfo.initCallback(&keeb->events.repeat_info, [this] (void* owner, void* data) {
        keyboardEvents.repeatInfo.emit();
    }, this, "CVirtualKeyboard");
    // clang-format on

    deviceName = keeb->base.name ? keeb->base.name : "UNKNOWN";
}

bool CVirtualKeyboard::isVirtual() {
    return true;
}

wlr_keyboard* CVirtualKeyboard::wlr() {
    if (keyboard.expired())
        return nullptr;
    return keyboard->wlr();
}

void CVirtualKeyboard::disconnectCallbacks() {
    hyprListener_destroy.removeCallback();
    hyprListener_key.removeCallback();
    hyprListener_keymap.removeCallback();
    hyprListener_repeatInfo.removeCallback();
    hyprListener_modifiers.removeCallback();
}

wl_client* CVirtualKeyboard::getClient() {
    if (keyboard.expired())
        return nullptr;
    return keyboard->client();
}
