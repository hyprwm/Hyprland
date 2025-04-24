#include "WLSurface.hpp"
#include "LayerSurface.hpp"
#include "../desktop/Window.hpp"
#include "../protocols/core/Compositor.hpp"
#include "../protocols/LayerShell.hpp"
#include "../render/Renderer.hpp"

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface) {
    m_resource = pSurface;
    init();
    m_inert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, PHLWINDOW pOwner) {
    m_windowOwner = pOwner;
    m_resource    = pSurface;
    init();
    m_inert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, PHLLS pOwner) {
    m_layerOwner = pOwner;
    m_resource   = pSurface;
    init();
    m_inert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, CSubsurface* pOwner) {
    m_subsurfaceOwner = pOwner;
    m_resource        = pSurface;
    init();
    m_inert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, CPopup* pOwner) {
    m_popupOwner = pOwner;
    m_resource   = pSurface;
    init();
    m_inert = false;
}

void CWLSurface::unassign() {
    destroy();
}

CWLSurface::~CWLSurface() {
    destroy();
}

bool CWLSurface::exists() const {
    return m_resource;
}

SP<CWLSurfaceResource> CWLSurface::resource() const {
    return m_resource.lock();
}

bool CWLSurface::small() const {
    if (!validMapped(m_windowOwner) || !exists())
        return false;

    if (!m_resource->current.texture)
        return false;

    const auto O = m_windowOwner.lock();

    return O->m_vReportedSize.x > m_resource->current.size.x + 1 || O->m_vReportedSize.y > m_resource->current.size.y + 1;
}

Vector2D CWLSurface::correctSmallVec() const {
    if (!validMapped(m_windowOwner) || !exists() || !small() || m_fillIgnoreSmall)
        return {};

    const auto SIZE = getViewporterCorrectedSize();
    const auto O    = m_windowOwner.lock();

    return Vector2D{(O->m_vReportedSize.x - SIZE.x) / 2, (O->m_vReportedSize.y - SIZE.y) / 2}.clamp({}, {INFINITY, INFINITY}) * (O->m_vRealSize->value() / O->m_vReportedSize);
}

Vector2D CWLSurface::correctSmallVecBuf() const {
    if (!exists() || !small() || m_fillIgnoreSmall || !m_resource->current.texture)
        return {};

    const auto SIZE = getViewporterCorrectedSize();
    const auto BS   = m_resource->current.bufferSize;

    return Vector2D{(BS.x - SIZE.x) / 2, (BS.y - SIZE.y) / 2}.clamp({}, {INFINITY, INFINITY});
}

Vector2D CWLSurface::getViewporterCorrectedSize() const {
    if (!exists() || !m_resource->current.texture)
        return {};

    return m_resource->current.viewport.hasDestination ? m_resource->current.viewport.destination : m_resource->current.bufferSize;
}

CRegion CWLSurface::computeDamage() const {
    if (!m_resource->current.texture)
        return {};

    CRegion damage = m_resource->current.accumulateBufferDamage();
    damage.transform(wlTransformToHyprutils(m_resource->current.transform), m_resource->current.bufferSize.x, m_resource->current.bufferSize.y);

    const auto BUFSIZE    = m_resource->current.bufferSize;
    const auto CORRECTVEC = correctSmallVecBuf();

    if (m_resource->current.viewport.hasSource)
        damage.intersect(m_resource->current.viewport.source);

    const auto SCALEDSRCSIZE = m_resource->current.viewport.hasSource ? m_resource->current.viewport.source.size() * m_resource->current.scale : m_resource->current.bufferSize;

    damage.scale({BUFSIZE.x / SCALEDSRCSIZE.x, BUFSIZE.y / SCALEDSRCSIZE.y});
    damage.translate(CORRECTVEC);

    // go from buffer coords in the damage to hl logical

    const auto     BOX   = getSurfaceBoxGlobal();
    const Vector2D SCALE = BOX.has_value() ? BOX->size() / m_resource->current.bufferSize :
                                             Vector2D{1.0 / m_resource->current.scale, 1.0 / m_resource->current.scale /* Wrong... but we can't really do better */};

    damage.scale(SCALE);

    if (m_windowOwner)
        damage.scale(m_windowOwner->m_fX11SurfaceScaledBy); // fix xwayland:force_zero_scaling stuff that will be fucked by the above a bit

    return damage;
}

void CWLSurface::destroy() {
    if (!m_resource)
        return;

    m_events.destroy.emit();

    m_constraint.reset();

    m_listeners.destroy.reset();
    m_resource->hlSurface.reset();
    m_windowOwner.reset();
    m_layerOwner.reset();
    m_popupOwner      = nullptr;
    m_subsurfaceOwner = nullptr;
    m_inert           = true;

    if (g_pHyprRenderer && g_pHyprRenderer->m_sLastCursorData.surf && g_pHyprRenderer->m_sLastCursorData.surf->get() == this)
        g_pHyprRenderer->m_sLastCursorData.surf.reset();

    m_resource.reset();

    Debug::log(LOG, "CWLSurface {:x} called destroy()", (uintptr_t)this);
}

void CWLSurface::init() {
    if (!m_resource)
        return;

    RASSERT(!m_resource->hlSurface, "Attempted to duplicate CWLSurface ownership!");

    m_resource->hlSurface = m_self.lock();

    m_listeners.destroy = m_resource->events.destroy.registerListener([this](std::any d) { destroy(); });

    Debug::log(LOG, "CWLSurface {:x} called init()", (uintptr_t)this);
}

PHLWINDOW CWLSurface::getWindow() const {
    return m_windowOwner.lock();
}

PHLLS CWLSurface::getLayer() const {
    return m_layerOwner.lock();
}

CPopup* CWLSurface::getPopup() const {
    return m_popupOwner;
}

CSubsurface* CWLSurface::getSubsurface() const {
    return m_subsurfaceOwner;
}

bool CWLSurface::desktopComponent() const {
    return !m_layerOwner.expired() || !m_windowOwner.expired() || m_subsurfaceOwner || m_popupOwner;
}

std::optional<CBox> CWLSurface::getSurfaceBoxGlobal() const {
    if (!desktopComponent())
        return {};

    if (!m_windowOwner.expired())
        return m_windowOwner->getWindowMainSurfaceBox();
    if (!m_layerOwner.expired())
        return m_layerOwner->m_geometry;
    if (m_popupOwner)
        return CBox{m_popupOwner->coordsGlobal(), m_popupOwner->size()};
    if (m_subsurfaceOwner)
        return CBox{m_subsurfaceOwner->coordsGlobal(), m_subsurfaceOwner->size()};

    return {};
}

void CWLSurface::appendConstraint(WP<CPointerConstraint> constraint) {
    m_constraint = constraint;
}

SP<CPointerConstraint> CWLSurface::constraint() const {
    return m_constraint.lock();
}

bool CWLSurface::visible() {
    if (!m_windowOwner.expired())
        return g_pHyprRenderer->shouldRenderWindow(m_windowOwner.lock());
    if (!m_layerOwner.expired())
        return true;
    if (m_popupOwner)
        return m_popupOwner->visible();
    if (m_subsurfaceOwner)
        return m_subsurfaceOwner->visible();
    return true; // non-desktop, we don't know much.
}

SP<CWLSurface> CWLSurface::fromResource(SP<CWLSurfaceResource> pSurface) {
    if (!pSurface)
        return nullptr;
    return pSurface->hlSurface.lock();
}

bool CWLSurface::keyboardFocusable() const {
    if (m_windowOwner || m_popupOwner || m_subsurfaceOwner)
        return true;
    if (m_layerOwner && m_layerOwner->m_layerSurface)
        return m_layerOwner->m_layerSurface->current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
    return false;
}
