#include "Window.hpp"
#include "Compositor.hpp"

CWindow::~CWindow() {
    if ((this->m_uSurface.xdg || this->m_uSurface.xwayland) && g_pCompositor->m_pLastFocus == g_pXWaylandManager->getWindowSurface(this))
        g_pCompositor->m_pLastFocus = nullptr;
}