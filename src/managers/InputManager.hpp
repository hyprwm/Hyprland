#pragma once

#include "../defines.hpp"
#include <list>
#include "../helpers/WLClasses.hpp"

class CInputManager {
public:

    void            onMouseMoved(wlr_event_pointer_motion*);
    void            onMouseWarp(wlr_event_pointer_motion_absolute*);
    void            onMouseButton(wlr_event_pointer_button*);
    void            onKeyboardKey(wlr_event_keyboard_key*, SKeyboard*);
    void            onKeyboardMod(void*, SKeyboard*);

    void            newKeyboard(wlr_input_device*);
    void            newMouse(wlr_input_device*);
    void            destroyKeyboard(SKeyboard*);
    void            destroyMouse(wlr_input_device*);

    Vector2D        getMouseCoordsInternal();

private:

    std::list<SKeyboard> m_lKeyboards;

    void            mouseMoveUnified(uint32_t);

};

inline std::unique_ptr<CInputManager> g_pInputManager;