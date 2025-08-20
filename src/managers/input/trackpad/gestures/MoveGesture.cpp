#include "MoveGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/LayoutManager.hpp"
#include "../../../../render/Renderer.hpp"

void CMoveTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    m_window    = g_pCompositor->m_lastWindow;
    m_lastDelta = {};
}

void CMoveTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_window)
        return;

    const auto DELTA = e.swipe ? e.swipe->delta : e.pinch->delta;

    if (m_window->m_isFloating) {
        g_pLayoutManager->getCurrentLayout()->moveActiveWindow(DELTA, m_window.lock());
        m_window->m_realSize->warp();
        m_window->m_realPosition->warp();
        return;
    }

    // tiled window -> displace, then execute a move dispatcher on end.

    g_pHyprRenderer->damageWindow(m_window.lock());

    // funny name but works on tiled too lmao
    m_lastDelta += DELTA;
    m_window->m_floatingOffset = m_lastDelta * 0.5F;

    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CMoveTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {

    if (m_window->m_isFloating || m_lastDelta.size() < 0.1F)
        return;

    // tiled: attempt to move window in the given direction

    const auto WINDOWPOS = m_window->m_realPosition->goal() + m_window->m_floatingOffset;

    m_window->m_floatingOffset = {};

    if (std::abs(m_lastDelta.x) > std::abs(m_lastDelta.y)) {
        // horizontal
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(m_window.lock(), m_lastDelta.x > 0 ? "r" : "l");
    } else {
        // vertical
        g_pLayoutManager->getCurrentLayout()->moveWindowTo(m_window.lock(), m_lastDelta.y > 0 ? "b" : "t");
    }

    const auto GOAL = m_window->m_realPosition->goal();

    m_window->m_realPosition->setValueAndWarp(WINDOWPOS);
    *m_window->m_realPosition = GOAL;

    m_window.reset();
}
