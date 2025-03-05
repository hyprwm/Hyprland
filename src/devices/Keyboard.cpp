#include "Keyboard.hpp"
#include "../defines.hpp"

#include <aquamarine/input/Input.hpp>

SP<CKeyboard> CKeyboard::create(SP<Aquamarine::IKeyboard> keeb) {
    SP<CKeyboard> pKeeb = SP<CKeyboard>(new CKeyboard(keeb));

    pKeeb->self = pKeeb;

    return pKeeb;
}

bool CKeyboard::isVirtual() {
    return false;
}

SP<Aquamarine::IKeyboard> CKeyboard::aq() {
    return keyboard.lock();
}

CKeyboard::CKeyboard(SP<Aquamarine::IKeyboard> keeb) : keyboard(keeb) {
    if (!keeb)
        return;

    m_listeners.destroy = keeb->events.destroy.registerListener([this](std::any d) {
        keyboard.reset();
        events.destroy.emit();
    });

    m_listeners.key = keeb->events.key.registerListener([this](std::any d) {
        auto E = std::any_cast<Aquamarine::IKeyboard::SKeyEvent>(d);

        keyboardEvents.key.emit(SKeyEvent{
            .timeMs  = E.timeMs,
            .keycode = E.key,
            .state   = E.pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED,
        });

        updateXkbStateWithKey(E.key + 8, E.pressed);
    });

    m_listeners.modifiers = keeb->events.modifiers.registerListener([this](std::any d) {
        updateModifiersState();

        keyboardEvents.modifiers.emit(SModifiersEvent{
            .depressed = modifiersState.depressed,
            .latched   = modifiersState.latched,
            .locked    = modifiersState.locked,
            .group     = modifiersState.group,
        });
    });

    deviceName = keeb->getName();
}
