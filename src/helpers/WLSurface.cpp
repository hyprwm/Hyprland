#include "WLSurface.hpp"
#include "../Compositor.hpp"

CWLSurface::CWLSurface(wlr_surface* pSurface) {
    m_pWLRSurface = pSurface;
    init();
}

void CWLSurface::assign(wlr_surface* pSurface) {
    m_pWLRSurface = pSurface;
    init();
}

void CWLSurface::unassign() {
    destroy();
}

CWLSurface::~CWLSurface() {
    destroy();
}

bool CWLSurface::exists() const {
    return m_pWLRSurface;
}

wlr_surface* CWLSurface::wlr() const {
    return m_pWLRSurface;
}

void CWLSurface::destroy() {
    if (!m_pWLRSurface)
        return;

    hyprListener_destroy.removeCallback();
    m_pWLRSurface->data = nullptr;

    if (g_pCompositor->m_pLastFocus == m_pWLRSurface)
        g_pCompositor->m_pLastFocus = nullptr;

    m_pWLRSurface = nullptr;

    Debug::log(LOG, "CWLSurface %x called destroy()", this);
}

void CWLSurface::init() {
    if (!m_pWLRSurface)
        return;

    RASSERT(!m_pWLRSurface->data, "Attempted to duplicate CWLSurface ownership!");

    m_pWLRSurface->data = this;

    hyprListener_destroy.initCallback(
        &m_pWLRSurface->events.destroy, [&](void* owner, void* data) { destroy(); }, this, "CWLSurface");

    Debug::log(LOG, "CWLSurface %x called init()", this);
}