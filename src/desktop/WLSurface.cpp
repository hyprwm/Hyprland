#include "WLSurface.hpp"
#include "../Compositor.hpp"
#include "../protocols/core/Compositor.hpp"

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface) {
    m_pResource = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, PHLWINDOW pOwner) {
    m_pWindowOwner = pOwner;
    m_pResource    = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, PHLLS pOwner) {
    m_pLayerOwner = pOwner;
    m_pResource   = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, CSubsurface* pOwner) {
    m_pSubsurfaceOwner = pOwner;
    m_pResource        = pSurface;
    init();
    m_bInert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, CPopup* pOwner) {
    m_pPopupOwner = pOwner;
    m_pResource   = pSurface;
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
    return m_pResource;
}

SP<CWLSurfaceResource> CWLSurface::resource() const {
    return m_pResource.lock();
}

bool CWLSurface::small() const {
    if (!validMapped(m_pWindowOwner) || !exists())
        return false;

    if (!m_pResource->current.buffer)
        return false;

    const auto O = m_pWindowOwner.lock();

    return O->m_vReportedSize.x > m_pResource->current.buffer->size.x + 1 || O->m_vReportedSize.y > m_pResource->current.buffer->size.y + 1;
}

Vector2D CWLSurface::correctSmallVec() const {
    if (!validMapped(m_pWindowOwner) || !exists() || !small() || m_bFillIgnoreSmall)
        return {};

    const auto SIZE = getViewporterCorrectedSize();
    const auto O    = m_pWindowOwner.lock();

    return Vector2D{(O->m_vReportedSize.x - SIZE.x) / 2, (O->m_vReportedSize.y - SIZE.y) / 2}.clamp({}, {INFINITY, INFINITY}) * (O->m_vRealSize.value() / O->m_vReportedSize);
}

Vector2D CWLSurface::getViewporterCorrectedSize() const {
    if (!exists() || !m_pResource->current.buffer)
        return {};

    return m_pResource->current.viewport.hasDestination ? m_pResource->current.viewport.destination : m_pResource->current.buffer->size;
}

CRegion CWLSurface::logicalDamage() const {
    if (!m_pResource->current.buffer)
        return {};

    CRegion damage = m_pResource->accumulateCurrentBufferDamage();
    damage.transform(m_pResource->current.transform, m_pResource->current.buffer->size.x, m_pResource->current.buffer->size.y);
    damage.scale(1.0 / m_pResource->current.scale);

    const auto VPSIZE     = getViewporterCorrectedSize();
    const auto CORRECTVEC = correctSmallVec();

    if (m_pResource->current.viewport.hasSource)
        damage.intersect(m_pResource->current.viewport.source);

    const auto SCALEDSRCSIZE =
        m_pResource->current.viewport.hasSource ? m_pResource->current.viewport.source.size() * m_pResource->current.scale : m_pResource->current.buffer->size;

    damage.scale({VPSIZE.x / SCALEDSRCSIZE.x, VPSIZE.y / SCALEDSRCSIZE.y});
    damage.translate(CORRECTVEC);

    return damage;
}

void CWLSurface::destroy() {
    if (!m_pResource)
        return;

    events.destroy.emit();

    m_pConstraint.reset();

    listeners.destroy.reset();
    m_pResource->hlSurface.reset();
    m_pWindowOwner.reset();
    m_pLayerOwner.reset();
    m_pPopupOwner      = nullptr;
    m_pSubsurfaceOwner = nullptr;
    m_bInert           = true;

    if (g_pHyprRenderer && g_pHyprRenderer->m_sLastCursorData.surf && g_pHyprRenderer->m_sLastCursorData.surf->get() == this)
        g_pHyprRenderer->m_sLastCursorData.surf.reset();

    m_pResource.reset();

    Debug::log(LOG, "CWLSurface {:x} called destroy()", (uintptr_t)this);
}

void CWLSurface::init() {
    if (!m_pResource)
        return;

    RASSERT(!m_pResource->hlSurface, "Attempted to duplicate CWLSurface ownership!");

    m_pResource->hlSurface = self.lock();

    listeners.destroy = m_pResource->events.destroy.registerListener([this](std::any d) { destroy(); });

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

SP<CWLSurface> CWLSurface::fromResource(SP<CWLSurfaceResource> pSurface) {
    if (!pSurface)
        return nullptr;
    return pSurface->hlSurface.lock();
}
