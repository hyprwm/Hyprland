#pragma once

#include "../defines.hpp"

class CInputManager {
public:

    void            onMouseMoved(wlr_event_pointer_motion*);
    void            onMouseWarp(wlr_event_pointer_motion_absolute*);

private:
    Vector2D        m_vMouseCoords      = Vector2D(0,0);
    Vector2D        m_vWLRMouseCoords   = Vector2D(0,0);

};

inline std::unique_ptr<CInputManager> g_pInputManager;