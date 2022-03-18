#pragma once

#include "../defines.hpp"
#include <deque>
#include "../helpers/WLClasses.hpp"

class CInputManager {
public:

    void            onMouseMoved(wlr_event_pointer_motion*);
    void            onMouseWarp(wlr_event_pointer_motion_absolute*);
    void            onMouseButton(wlr_event_pointer_button*);
    void            onKeyboardKey(wlr_event_keyboard_key*);
    void            onKeyboardMod(void*);

    void            newKeyboard(wlr_input_device*);
    void            newMouse(wlr_input_device*);

    Vector2D        getMouseCoordsInternal();

private:
    Vector2D        m_vMouseCoords      = Vector2D(0,0);
    Vector2D        m_vWLRMouseCoords   = Vector2D(0,0);

    std::deque<SKeyboard> m_dKeyboards;

};

inline std::unique_ptr<CInputManager> g_pInputManager;