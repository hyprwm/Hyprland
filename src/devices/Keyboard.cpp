#include "Keyboard.hpp"
#include "../defines.hpp"

#include <aquamarine/input/Input.hpp>

SP<CKeyboard> CKeyboard::create(SP<Aquamarine::IKeyboard> keeb) {
    SP<CKeyboard> pKeeb = SP<CKeyboard>(new CKeyboard(keeb));

    pKeeb->m_self = pKeeb;

    return pKeeb;
}

bool CKeyboard::isVirtual() {
    return false;
}

SP<Aquamarine::IKeyboard> CKeyboard::aq() {
    return m_keyboard.lock();
}

CKeyboard::CKeyboard(SP<Aquamarine::IKeyboard> keeb) : m_keyboard(keeb) {
    if (!keeb)
        return;

    m_listeners.destroy = keeb->events.destroy.registerListener([this](std::any d) {
        m_keyboard.reset();
        m_events.destroy.emit();
    });

    m_listeners.key = keeb->events.key.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IKeyboard::SKeyEvent>(d);

        m_keyboardEvents.key.emit(SKeyEvent{
            .timeMs  = E.timeMs,
            .keycode = E.key,
            .state   = E.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
        });

        updateXkbStateWithKey(E.key + 8, E.pressed);
    });

    m_listeners.modifiers = keeb->events.modifiers.registerListener([this](std::any d) {
        updateModifiersState();

        m_keyboardEvents.modifiers.emit(SModifiersEvent{
            .depressed = m_modifiersState.depressed,
            .latched   = m_modifiersState.latched,
            .locked    = m_modifiersState.locked,
            .group     = m_modifiersState.group,
        });
    });

    m_deviceName = keeb->getName();
}
