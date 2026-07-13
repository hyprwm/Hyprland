#include "WLSurface.hpp"
#include "LayerSurface.hpp"
#include "Window.hpp"
#include "../../protocols/core/Compositor.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../protocols/FractionalScale.hpp"
#include "../../render/Renderer.hpp"

#include <cmath>

using namespace Desktop;
using namespace Desktop::View;

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface) {
    m_resource = pSurface;
    init();
    m_inert = false;
}

void CWLSurface::assign(SP<CWLSurfaceResource> pSurface, SP<IView> pOwner) {
    m_view     = pOwner;
    m_resource = pSurface;
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
    return !!m_resource;
}

SP<CWLSurfaceResource> CWLSurface::resource() const {
    return m_resource.lock();
}

bool CWLSurface::small() const {
    if (!m_view || !m_view->aliveAndVisible() || m_view->type() != VIEW_TYPE_WINDOW || !exists())
        return false;

    if (!m_resource->m_current.texture)
        return false;

    const auto O             = dynamicPointerCast<CWindow>(m_view.lock());
    const auto REPORTED_SIZE = O->getReportedSize();

    return REPORTED_SIZE.x > m_resource->m_current.size.x + 1 || REPORTED_SIZE.y > m_resource->m_current.size.y + 1;
}

Vector2D CWLSurface::correctSmallVec() const {
    if (!m_view || !m_view->aliveAndVisible() || m_view->type() != VIEW_TYPE_WINDOW || !exists() || !small() || !m_fillIgnoreSmall)
        return {};

    const auto SIZE = getViewporterCorrectedSize();
    const auto O    = dynamicPointerCast<CWindow>(m_view.lock());
    const auto REP  = O->getReportedSize();

    return Vector2D{(REP.x - SIZE.x) / 2, (REP.y - SIZE.y) / 2}.clamp({}, {INFINITY, INFINITY}) * (O->m_realSize->value() / REP);
}

Vector2D CWLSurface::correctSmallVecBuf() const {
    if (!exists() || !small() || m_fillIgnoreSmall || !m_resource->m_current.texture)
        return {};

    const auto SIZE = getViewporterCorrectedSize();
    const auto BS   = m_resource->m_current.bufferSize;

    return Vector2D{(BS.x - SIZE.x) / 2, (BS.y - SIZE.y) / 2}.clamp({}, {INFINITY, INFINITY});
}

Vector2D CWLSurface::getViewporterCorrectedSize() const {
    if (!exists() || !m_resource->m_current.texture)
        return {};

    return m_resource->m_current.viewport.hasDestination ? m_resource->m_current.viewport.destination : m_resource->m_current.bufferSize;
}

CRegion CWLSurface::computeDamage() const {
    if (!m_resource->m_current.texture)
        return {};

    CRegion damage = m_resource->m_current.accumulateBufferDamage();
    damage.transform(Math::wlTransformToHyprutils(m_resource->m_current.transform), m_resource->m_current.bufferSize.x, m_resource->m_current.bufferSize.y);

    const auto BUFSIZE = m_resource->m_current.bufferSize;
    if (BUFSIZE.x <= 0 || BUFSIZE.y <= 0)
        return {};

    const auto CORRECTVEC = correctSmallVecBuf();

    if (m_resource->m_current.viewport.hasSource)
        damage.intersect(m_resource->m_current.viewport.source);

    const auto SCALEDSRCSIZE =
        m_resource->m_current.viewport.hasSource ? m_resource->m_current.viewport.source.size() * m_resource->m_current.scale : m_resource->m_current.bufferSize;
    if (SCALEDSRCSIZE.x <= 0 || SCALEDSRCSIZE.y <= 0)
        return {};

    damage.scale({BUFSIZE.x / SCALEDSRCSIZE.x, BUFSIZE.y / SCALEDSRCSIZE.y});
    damage.translate(CORRECTVEC);

    // go from buffer coords in the damage to hl logical

    const auto BOX      = getSurfaceBoxGlobal();
    const auto SURFSIZE = m_resource->m_current.size;
    if (SURFSIZE.x <= 0 || SURFSIZE.y <= 0)
        return {};

    const Vector2D SCALE = SURFSIZE / m_resource->m_current.bufferSize;

    damage.scale(SCALE);
    if (BOX.has_value()) {
        auto boxSize = BOX->size();

        if (m_view->type() == VIEW_TYPE_WINDOW) {
            const auto WINDOW = dynamicPointerCast<CWindow>(m_view.lock());
            if (!WINDOW)
                return {};

            boxSize = boxSize * WINDOW->m_X11SurfaceScaledBy;
        }

        if (boxSize.x <= 0 || boxSize.y <= 0)
            return {};

        damage.intersect(CBox{{}, boxSize});
    }

    return damage;
}

void CWLSurface::destroy() {
    if (!m_resource)
        return;

    m_events.destroy.emit();

    m_constraint.reset();

    m_listeners.destroy.reset();
    m_resource->m_hlSurface.reset();
    m_view.reset();
    m_inert = true;

    if (g_pHyprRenderer && g_pHyprRenderer->m_lastCursorData.surf && g_pHyprRenderer->m_lastCursorData.surf->get() == this)
        g_pHyprRenderer->m_lastCursorData.surf.reset();

    m_resource.reset();

    Log::logger->log(Log::DEBUG, "CWLSurface {:x} called destroy()", rc<uintptr_t>(this));
}

void CWLSurface::init() {
    if (!m_resource)
        return;

    RASSERT(!m_resource->m_hlSurface, "Attempted to duplicate CWLSurface ownership!");

    m_resource->m_hlSurface = m_self.lock();

    m_listeners.destroy = m_resource->m_events.destroy.listen([this] { destroy(); });

    Log::logger->log(Log::DEBUG, "CWLSurface {:x} called init()", rc<uintptr_t>(this));
}

SP<IView> CWLSurface::view() const {
    return m_view.lock();
}

bool CWLSurface::desktopComponent() const {
    return m_view && m_view->visible();
}

std::optional<CBox> CWLSurface::getSurfaceBoxGlobal() const {
    if (!desktopComponent())
        return {};

    return m_view->surfaceLogicalBox();
}

void CWLSurface::appendConstraint(WP<CPointerConstraint> constraint) {
    m_constraint = constraint;
}

SP<CPointerConstraint> CWLSurface::constraint() const {
    return m_constraint.lock();
}

SP<Desktop::View::CWLSurface> CWLSurface::fromResource(SP<CWLSurfaceResource> pSurface) {
    if (!pSurface)
        return nullptr;
    return pSurface->m_hlSurface.lock();
}

bool CWLSurface::keyboardFocusable() const {
    if (!m_view)
        return false;
    if (m_view->type() == VIEW_TYPE_WINDOW || m_view->type() == VIEW_TYPE_SUBSURFACE || m_view->type() == VIEW_TYPE_POPUP)
        return true;
    if (const auto LS = CLayerSurface::fromView(m_view.lock()); LS && LS->m_layerSurface)
        return LS->m_layerSurface->m_current.interactivity != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
    return false;
}

void CWLSurface::sendScale(float scale) const {
    // FIXME: huge band-aid for weird ass shit ass behavior where onUnmap can call with m_resource alive but underlying null
    if (m_resource.expired() || !m_resource->good())
        return;

    m_resource->sendPreferredScale(sc<uint32_t>(std::ceil(scale)));
    PROTO::fractional->sendScale(m_resource.lock(), scale);
}

void CWLSurface::sendTransform(wl_output_transform xform) const {
    if (m_resource.expired() || !m_resource->good())
        return;

    m_resource->sendPreferredTransform(xform);
}
