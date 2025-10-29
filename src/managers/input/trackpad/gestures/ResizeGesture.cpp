#include "ResizeGesture.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../managers/LayoutManager.hpp"
#include "../../../../render/Renderer.hpp"

void CResizeTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_window = g_pCompositor->m_lastWindow;
}

void CResizeTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_window)
        return;

    g_pHyprRenderer->damageWindow(m_window.lock());

    g_pLayoutManager->getCurrentLayout()->resizeActiveWindow(
        (e.swipe ? e.swipe->delta : e.pinch->delta), Interactive::cornerFromBox(m_window->getWindowMainSurfaceBox(), g_pInputManager->getMouseCoordsInternal()), m_window.lock());
    m_window->m_realSize->warp();
    m_window->m_realPosition->warp();

    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CResizeTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    m_window.reset();
}
