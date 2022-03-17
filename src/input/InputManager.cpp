#include "InputManager.hpp"
#include "../Compositor.hpp"

void CInputManager::onMouseMoved(wlr_event_pointer_motion* e) {
    // TODO: sensitivity

    float sensitivity = 0.25f;

    m_vMouseCoords = m_vMouseCoords + Vector2D(e->delta_x * sensitivity, e->delta_y * sensitivity);

    if (m_vMouseCoords.floor() != m_vWLRMouseCoords) {
        Vector2D delta = m_vMouseCoords - m_vWLRMouseCoords;
        m_vWLRMouseCoords = m_vMouseCoords.floor();

        wlr_cursor_move(g_pCompositor->m_sWLRCursor, e->device, delta.floor().x, delta.floor().y);
    }
}

void CInputManager::onMouseWarp(wlr_event_pointer_motion_absolute* e) {
    wlr_cursor_warp_absolute(g_pCompositor->m_sWLRCursor, e->device, e->x, e->y);
}

Vector2D CInputManager::getMouseCoordsInternal() {
    return m_vMouseCoords;
}