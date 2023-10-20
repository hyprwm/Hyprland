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

bool CWLSurface::small() const {
    if (!m_pOwner || !exists())
        return false;

    return m_pOwner->m_vReportedSize.x > m_pWLRSurface->current.buffer_width || m_pOwner->m_vReportedSize.y > m_pWLRSurface->current.buffer_height;
}

Vector2D CWLSurface::correctSmallVec() const {
    if (!m_pOwner || !exists() || !small() || m_bFillIgnoreSmall)
        return {};

    return Vector2D{(m_pOwner->m_vReportedSize.x - m_pWLRSurface->current.buffer_width) / 2, (m_pOwner->m_vReportedSize.y - m_pWLRSurface->current.buffer_height) / 2}.clamp(
               {}, {INFINITY, INFINITY}) *
        (m_pOwner->m_vRealSize.vec() / m_pOwner->m_vReportedSize);
}

void CWLSurface::destroy() {
    if (!m_pWLRSurface)
        return;

    hyprListener_destroy.removeCallback();
    m_pWLRSurface->data = nullptr;
    m_pOwner            = nullptr;

    if (g_pCompositor->m_pLastFocus == m_pWLRSurface)
        g_pCompositor->m_pLastFocus = nullptr;
    if (g_pInputManager->m_pLastMouseSurface == m_pWLRSurface)
        g_pInputManager->m_pLastMouseSurface = nullptr;

    m_pWLRSurface = nullptr;

    Debug::log(LOG, "CWLSurface {:x} called destroy()", (uintptr_t)this);
}

void CWLSurface::init() {
    if (!m_pWLRSurface)
        return;

    RASSERT(!m_pWLRSurface->data, "Attempted to duplicate CWLSurface ownership!");

    m_pWLRSurface->data = this;

    hyprListener_destroy.initCallback(
        &m_pWLRSurface->events.destroy, [&](void* owner, void* data) { destroy(); }, this, "CWLSurface");

    Debug::log(LOG, "CWLSurface {:x} called init()", (uintptr_t)this);
}