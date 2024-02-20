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

    return m_pOwner->m_vReportedSize.x > m_pWLRSurface->current.buffer_width + 1 || m_pOwner->m_vReportedSize.y > m_pWLRSurface->current.buffer_height + 1;
}

Vector2D CWLSurface::correctSmallVec() const {
    if (!m_pOwner || !exists() || !small() || m_bFillIgnoreSmall)
        return {};

    const auto SIZE = getViewporterCorrectedSize();

    return Vector2D{(m_pOwner->m_vReportedSize.x - SIZE.x) / 2, (m_pOwner->m_vReportedSize.y - SIZE.y) / 2}.clamp({}, {INFINITY, INFINITY}) *
        (m_pOwner->m_vRealSize.vec() / m_pOwner->m_vReportedSize);
}

Vector2D CWLSurface::getViewporterCorrectedSize() const {
    if (!exists())
        return {};

    return m_pWLRSurface->current.viewport.has_dst ? Vector2D{m_pWLRSurface->current.viewport.dst_width, m_pWLRSurface->current.viewport.dst_height} :
                                                     Vector2D{m_pWLRSurface->current.buffer_width, m_pWLRSurface->current.buffer_height};
}

CRegion CWLSurface::logicalDamage() const {
    CRegion damage{&m_pWLRSurface->buffer_damage};
    damage.transform(m_pWLRSurface->current.transform, m_pWLRSurface->current.buffer_width, m_pWLRSurface->current.buffer_height);
    damage.scale(1.0 / m_pWLRSurface->current.scale);

    const auto VPSIZE     = getViewporterCorrectedSize();
    const auto CORRECTVEC = correctSmallVec();

    if (m_pWLRSurface->current.viewport.has_src) {
        damage.intersect(CBox{std::floor(m_pWLRSurface->current.viewport.src.x), std::floor(m_pWLRSurface->current.viewport.src.y),
                              std::ceil(m_pWLRSurface->current.viewport.src.width), std::ceil(m_pWLRSurface->current.viewport.src.height)});
    }

    const auto SCALEDSRCSIZE = m_pWLRSurface->current.viewport.has_src ?
        Vector2D{m_pWLRSurface->current.viewport.src.width, m_pWLRSurface->current.viewport.src.height} * m_pWLRSurface->current.scale :
        Vector2D{m_pWLRSurface->current.buffer_width, m_pWLRSurface->current.buffer_height};

    damage.scale({VPSIZE.x / SCALEDSRCSIZE.x, VPSIZE.y / SCALEDSRCSIZE.y});
    damage.translate(CORRECTVEC);

    return damage;
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
    if (g_pHyprRenderer->m_sLastCursorData.surf == m_pWLRSurface)
        g_pHyprRenderer->m_sLastCursorData.surf.reset();

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