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

    m_listeners.destroy = keeb_->m_events.destroy.registerListener([this](std::any d) {
        m_keyboard.reset();
        m_events.destroy.emit();
    });

    m_listeners.key       = keeb_->m_events.key.registerListener([this](std::any d) { m_keyboardEvents.key.emit(d); });
    m_listeners.modifiers = keeb_->m_events.modifiers.registerListener([this](std::any d) {
        auto E = std::any_cast<SModifiersEvent>(d);
        updateModifiers(E.depressed, E.latched, E.locked, E.group);
        m_keyboardEvents.modifiers.emit(SModifiersEvent{
            .depressed = m_modifiersState.depressed,
            .latched   = m_modifiersState.latched,
            .locked    = m_modifiersState.locked,
            .group     = m_modifiersState.group,
        });
    });
    m_listeners.keymap    = keeb_->m_events.keymap.registerListener([this](std::any d) {
        auto E = std::any_cast<SKeymapEvent>(d);
        if (m_xkbKeymap)
            xkb_keymap_unref(m_xkbKeymap);
        m_xkbKeymap        = xkb_keymap_ref(E.keymap);
        m_keymapOverridden = true;
        updateXKBTranslationState(m_xkbKeymap);
        updateKeymapFD();
        m_keyboardEvents.keymap.emit(d);
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
