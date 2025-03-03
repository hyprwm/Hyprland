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

    m_listeners.destroy = keeb_->events.destroy.registerListener([this](std::any d) {
        keyboard.reset();
        events.destroy.emit();
    });

    m_listeners.key       = keeb_->events.key.registerListener([this](std::any d) { keyboardEvents.key.emit(d); });
    m_listeners.modifiers = keeb_->events.modifiers.registerListener([this](std::any d) {
        auto E = std::any_cast<SModifiersEvent>(d);
        updateModifiers(E.depressed, E.latched, E.locked, E.group);
        keyboardEvents.modifiers.emit(SModifiersEvent{
            .depressed = modifiersState.depressed,
            .latched   = modifiersState.latched,
            .locked    = modifiersState.locked,
            .group     = modifiersState.group,
        });
    });
    m_listeners.keymap    = keeb_->events.keymap.registerListener([this](std::any d) {
        auto E = std::any_cast<SKeymapEvent>(d);
        if (xkbKeymap)
            xkb_keymap_unref(xkbKeymap);
        xkbKeymap        = xkb_keymap_ref(E.keymap);
        keymapOverridden = true;
        updateXKBTranslationState(xkbKeymap);
        updateKeymapFD();
        keyboardEvents.keymap.emit(d);
    });

    deviceName = keeb_->name;
}

bool CVirtualKeyboard::isVirtual() {
    return true;
}

SP<Aquamarine::IKeyboard> CVirtualKeyboard::aq() {
    return nullptr;
}

wl_client* CVirtualKeyboard::getClient() {
    if (keyboard.expired())
        return nullptr;
    return keyboard->client();
}
