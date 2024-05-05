#include "WLSurface.hpp"
#include "../Compositor.hpp"

void CWLSurface::assign(wlr_surface* pSurface) {
    m_pWLRSurface = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(wlr_surface* pSurface, PHLWINDOW pOwner) {
    m_pWindowOwner = pOwner;
    m_pWLRSurface  = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(wlr_surface* pSurface, PHLLS pOwner) {
    m_pLayerOwner = pOwner;
    m_pWLRSurface = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(wlr_surface* pSurface, CSubsurface* pOwner) {
    m_pSubsurfaceOwner = pOwner;
    m_pWLRSurface      = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(wlr_surface* pSurface, CPopup* pOwner) {
    m_pPopupOwner = pOwner;
    m_pWLRSurface = pSurface;
    init();
    m_bInert = false;
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
    if (!validMapped(m_pWindowOwner) || !exists())
        return false;

    const auto O = m_pWindowOwner.lock();

    return O->m_vReportedSize.x > m_pWLRSurface->current.buffer_width + 1 || O->m_vReportedSize.y > m_pWLRSurface->current.buffer_height + 1;
}

Vector2D CWLSurface::correctSmallVec() const {
    if (!validMapped(m_pWindowOwner) || !exists() || !small() || m_bFillIgnoreSmall)
        return {};

    const auto SIZE = getViewporterCorrectedSize();
    const auto O    = m_pWindowOwner.lock();

    return Vector2D{(O->m_vReportedSize.x - SIZE.x) / 2, (O->m_vReportedSize.y - SIZE.y) / 2}.clamp({}, {INFINITY, INFINITY}) * (O->m_vRealSize.value() / O->m_vReportedSize);
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

    events.destroy.emit();

    m_pConstraint.reset();

    hyprListener_destroy.removeCallback();
    hyprListener_commit.removeCallback();
    m_pWLRSurface->data = nullptr;
    m_pWindowOwner.reset();
    m_pLayerOwner.reset();
    m_pPopupOwner      = nullptr;
    m_pSubsurfaceOwner = nullptr;
    m_bInert           = true;

    if (g_pCompositor && g_pCompositor->m_pLastFocus == m_pWLRSurface)
        g_pCompositor->m_pLastFocus = nullptr;
    if (g_pInputManager && g_pInputManager->m_pLastMouseSurface == m_pWLRSurface)
        g_pInputManager->m_pLastMouseSurface = nullptr;
    if (g_pHyprRenderer && g_pHyprRenderer->m_sLastCursorData.surf == this)
        g_pHyprRenderer->m_sLastCursorData.surf.reset();

    m_pWLRSurface = nullptr;

    Debug::log(LOG, "CWLSurface {:x} called destroy()", (uintptr_t)this);
}

static void onCommit(void* owner, void* data) {
    const auto SURF = (CWLSurface*)owner;
    SURF->onCommit();
}

void CWLSurface::init() {
    if (!m_pWLRSurface)
        return;

    RASSERT(!m_pWLRSurface->data, "Attempted to duplicate CWLSurface ownership!");

    m_pWLRSurface->data = this;

    hyprListener_destroy.initCallback(
        &m_pWLRSurface->events.destroy, [&](void* owner, void* data) { destroy(); }, this, "CWLSurface");
    hyprListener_commit.initCallback(&m_pWLRSurface->events.commit, ::onCommit, this, "CWLSurface");

    Debug::log(LOG, "CWLSurface {:x} called init()", (uintptr_t)this);
}

PHLWINDOW CWLSurface::getWindow() {
    return m_pWindowOwner.lock();
}

PHLLS CWLSurface::getLayer() {
    return m_pLayerOwner.lock();
}

CPopup* CWLSurface::getPopup() {
    return m_pPopupOwner;
}

CSubsurface* CWLSurface::getSubsurface() {
    return m_pSubsurfaceOwner;
}

bool CWLSurface::desktopComponent() {
    return !m_pLayerOwner.expired() || !m_pWindowOwner.expired() || m_pSubsurfaceOwner || m_pPopupOwner;
}

std::optional<CBox> CWLSurface::getSurfaceBoxGlobal() {
    if (!desktopComponent())
        return {};

    if (!m_pWindowOwner.expired())
        return m_pWindowOwner->getWindowMainSurfaceBox();
    if (!m_pLayerOwner.expired())
        return m_pLayerOwner->geometry;
    if (m_pPopupOwner)
        return CBox{m_pPopupOwner->coordsGlobal(), m_pPopupOwner->size()};
    if (m_pSubsurfaceOwner)
        return CBox{m_pSubsurfaceOwner->coordsGlobal(), m_pSubsurfaceOwner->size()};

    return {};
}

void CWLSurface::appendConstraint(WP<CPointerConstraint> constraint) {
    m_pConstraint = constraint;
}

void CWLSurface::onCommit() {
    ;
}

SP<CPointerConstraint> CWLSurface::constraint() {
    return m_pConstraint.lock();
}

bool CWLSurface::visible() {
    if (!m_pWindowOwner.expired())
        return g_pHyprRenderer->shouldRenderWindow(m_pWindowOwner.lock());
    if (!m_pLayerOwner.expired())
        return true;
    if (m_pPopupOwner)
        return m_pPopupOwner->visible();
    if (m_pSubsurfaceOwner)
        return m_pSubsurfaceOwner->visible();
    return true; // non-desktop, we don't know much.
}
