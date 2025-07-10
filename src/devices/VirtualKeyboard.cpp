#include "VirtualKeyboard.hpp"
#include "../defines.hpp"
#include "../protocols/VirtualKeyboard.hpp"

SP<CVirtualKeyboard> CVirtualKeyboard::create(SP<CVirtualKeyboardV1Resource> keeb) {
    SP<CVirtualKeyboard> pKeeb = SP<CVirtualKeyboard>(new CVirtualKeyboard(keeb));

    pKeeb->m_self = pKeeb;

    return pKeeb;
}

CVirtualKeyboard::CVirtualKeyboard(SP<CVirtualKeyboardV1Resource> keeb_) : m_keyboard(keeb_) {
    if (!keeb_)
        return;

    m_listeners.destroy = keeb_->m_events.destroy.listen([this] {
        m_keyboard.reset();
        m_events.destroy.emit();
    });

    m_listeners.key       = keeb_->m_events.key.listen([this](const auto& event) { m_keyboardEvents.key.emit(event); });
    m_listeners.modifiers = keeb_->m_events.modifiers.listen([this](const SModifiersEvent& event) {
        updateModifiers(event.depressed, event.latched, event.locked, event.group);
        m_keyboardEvents.modifiers.emit(SModifiersEvent{
            .depressed = m_modifiersState.depressed,
            .latched   = m_modifiersState.latched,
            .locked    = m_modifiersState.locked,
            .group     = m_modifiersState.group,
        });
    });
    m_listeners.keymap    = keeb_->m_events.keymap.listen([this](const SKeymapEvent& event) {
        if (m_xkbKeymap)
            xkb_keymap_unref(m_xkbKeymap);
        m_xkbKeymap        = xkb_keymap_ref(event.keymap);
        m_keymapOverridden = true;
        updateXKBTranslationState(m_xkbKeymap);
        updateKeymapFD();
        m_keyboardEvents.keymap.emit(event);
    });

    m_deviceName = keeb_->m_name;
}

bool CVirtualKeyboard::isVirtual() {
    return true;
}

SP<Aquamarine::IKeyboard> CVirtualKeyboard::aq() {
    return nullptr;
}

wl_client* CVirtualKeyboard::getClient() {
    if (m_keyboard.expired())
        return nullptr;
    return m_keyboard->client();
}
