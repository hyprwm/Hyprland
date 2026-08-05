#include "WaylandPopupBackend.hpp"

#include "../../../protocols/XDGShell.hpp"
#include "../../../protocols/core/Compositor.hpp"

using namespace Desktop::View;

CWaylandPopupBackend::CWaylandPopupBackend(SP<CXDGPopupResource> resource) : m_resource(resource) {
    if (!resource)
        return;

    const auto XDG_SURFACE = resource->m_surface.lock();
    if (XDG_SURFACE)
        m_surface = XDG_SURFACE->m_surface;

    m_listeners.reposition = resource->m_events.reposition.listen([this] { m_events.reposition.emit(); });
    m_listeners.dismissed  = resource->m_events.dismissed.listen([this] { m_events.dismissed.emit(); });
    m_listeners.destroy    = resource->m_events.destroy.listen([this] { m_events.destroy.emit(); });

    if (!XDG_SURFACE)
        return;

    m_listeners.map      = XDG_SURFACE->m_events.map.listen([this] { m_events.map.emit(); });
    m_listeners.unmap    = XDG_SURFACE->m_events.unmap.listen([this] { m_events.unmap.emit(); });
    m_listeners.commit   = XDG_SURFACE->m_events.commit.listen([this] { m_events.commit.emit(); });
    m_listeners.newPopup = XDG_SURFACE->m_events.newPopup.listen([this](const auto& resource) { m_events.newPopup.emit(makeWaylandPopupBackend(resource)); });
}

bool CWaylandPopupBackend::valid() const {
    const auto RESOURCE = m_resource.lock();
    return RESOURCE && RESOURCE->m_surface;
}

bool CWaylandPopupBackend::mapped() const {
    const auto RESOURCE = m_resource.lock();
    const auto SURFACE  = RESOURCE ? RESOURCE->m_surface.lock() : nullptr;
    return SURFACE && SURFACE->m_mapped;
}

bool CWaylandPopupBackend::initialCommit() const {
    const auto RESOURCE = m_resource.lock();
    const auto SURFACE  = RESOURCE ? RESOURCE->m_surface.lock() : nullptr;
    return SURFACE && SURFACE->m_initialCommit;
}

SP<CWLSurfaceResource> CWaylandPopupBackend::surface() const {
    return m_surface.lock();
}

CBox CWaylandPopupBackend::popupGeometry() const {
    const auto RESOURCE = m_resource.lock();
    return RESOURCE ? RESOURCE->m_geometry : CBox{};
}

CBox CWaylandPopupBackend::surfaceGeometry() const {
    const auto RESOURCE = m_resource.lock();
    const auto SURFACE  = RESOURCE ? RESOURCE->m_surface.lock() : nullptr;
    return SURFACE ? SURFACE->m_current.geometry : CBox{};
}

Vector2D CWaylandPopupBackend::surfaceSize() const {
    const auto SURFACE = m_surface.lock();
    return SURFACE ? SURFACE->m_current.size : Vector2D{};
}

void CWaylandPopupBackend::applyPositioning(const CBox& availableBox, const Vector2D& t1Coord) {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE)
        RESOURCE->applyPositioning(availableBox, t1Coord);
}

void CWaylandPopupBackend::scheduleConfigure() {
    const auto RESOURCE = m_resource.lock();
    const auto SURFACE  = RESOURCE ? RESOURCE->m_surface.lock() : nullptr;
    if (SURFACE)
        SURFACE->scheduleConfigure();
}

SP<IPopupBackend> Desktop::View::makeWaylandPopupBackend(SP<CXDGPopupResource> resource) {
    return makeShared<CWaylandPopupBackend>(resource);
}
