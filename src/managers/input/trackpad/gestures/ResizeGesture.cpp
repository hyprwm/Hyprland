#include "ResizeGesture.hpp"

#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../desktop/view/Window.hpp"
#include "../../../../render/Renderer.hpp"
#include "../../../../layout/LayoutManager.hpp"

void CResizeTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ITrackpadGesture::begin(e);

    m_window = Desktop::focusState()->window();
}

void CResizeTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    if (!m_window)
        return;

    g_pHyprRenderer->damageWindow(m_window.lock());

    g_layoutManager->resizeTarget((e.swipe ? e.swipe->delta : e.pinch->delta), m_window->layoutTarget(),
                                  Layout::cornerFromBox(m_window->getWindowMainSurfaceBox(), g_pInputManager->getMouseCoordsInternal()));
    m_window->m_realSize->warp();
    m_window->m_realPosition->warp();

    g_pHyprRenderer->damageWindow(m_window.lock());
}

void CResizeTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    m_window.reset();
}
