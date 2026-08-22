#include "WaylandBackend.hpp"

#include <limits>

#include "Window.hpp"
#include "../popup/WaylandPopupBackend.hpp"
#include "../../../protocols/XDGDialog.hpp"
#include "../../../protocols/XDGShell.hpp"
#include "../../../protocols/core/Compositor.hpp"

using namespace Desktop::View;

static SBackendMetadata metadataFrom(const SP<CXDGToplevelResource>& toplevel) {
    if (!toplevel)
        return {};

    return {
        .title       = toplevel->m_state.title,
        .appID       = toplevel->m_state.appid,
        .tag         = toplevel->m_toplevelTag,
        .description = toplevel->m_toplevelDescription,
    };
}

static SBackendTraits traitsFrom(const SP<CXDGToplevelResource>& toplevel) {
    if (!toplevel)
        return {};

    const bool FIXED_SIZE = toplevel->m_current.minSize.x != 0 && toplevel->m_current.minSize.y != 0 &&
        (toplevel->m_current.minSize.x == toplevel->m_current.maxSize.x || toplevel->m_current.minSize.y == toplevel->m_current.maxSize.y);

    return {
        .modal         = toplevel->m_dialog && toplevel->m_dialog->modal,
        .hasModalChild = toplevel->anyChildModal(),
        .transient     = !!toplevel->m_parent,
        .suggestsFloat = toplevel->m_parent || FIXED_SIZE,
    };
}

static SGeometryHints geometryHintsFrom(const SP<CXDGSurfaceResource>& resource, eBackendState state) {
    const auto TOPLEVEL = resource ? resource->m_toplevel.lock() : nullptr;
    if (!TOPLEVEL)
        return {};

    const auto& TOPLEVEL_STATE = state == eBackendState::BACKEND_STATE_PENDING ? TOPLEVEL->m_pending : TOPLEVEL->m_current;
    const auto& XDG_STATE      = state == eBackendState::BACKEND_STATE_PENDING ? resource->m_pending : resource->m_current;

    Vector2D    minSize;
    if (TOPLEVEL_STATE.minSize.x > 1)
        minSize.x = TOPLEVEL_STATE.minSize.x + XDG_STATE.geometry.pos().x;
    if (TOPLEVEL_STATE.minSize.y > 1)
        minSize.y = TOPLEVEL_STATE.minSize.y + XDG_STATE.geometry.pos().y;
    minSize = minSize.clamp({1, 1});

    Vector2D maxSize;
    if (TOPLEVEL_STATE.maxSize.x > 1)
        maxSize.x = TOPLEVEL_STATE.maxSize.x + XDG_STATE.geometry.pos().x;
    if (TOPLEVEL_STATE.maxSize.y > 1)
        maxSize.y = TOPLEVEL_STATE.maxSize.y + XDG_STATE.geometry.pos().y;
    if (maxSize.x < 5)
        maxSize.x = std::numeric_limits<double>::max();
    if (maxSize.y < 5)
        maxSize.y = std::numeric_limits<double>::max();

    return {
        .minSize = minSize,
        .maxSize = maxSize,
    };
}

static bool metadataEqual(const SBackendMetadata& lhs, const SBackendMetadata& rhs) {
    return lhs.title == rhs.title && lhs.appID == rhs.appID && lhs.tag == rhs.tag && lhs.description == rhs.description;
}

static bool traitsEqual(const SBackendTraits& lhs, const SBackendTraits& rhs) {
    return lhs.overrideRedirect == rhs.overrideRedirect && lhs.modal == rhs.modal && lhs.hasModalChild == rhs.hasModalChild && lhs.transient == rhs.transient &&
        lhs.wantsFocus == rhs.wantsFocus && lhs.suggestsFloat == rhs.suggestsFloat && lhs.suggestsNoInitialFocus == rhs.suggestsNoInitialFocus &&
        lhs.preventsFocus == rhs.preventsFocus && lhs.suggestsNoBorder == rhs.suggestsNoBorder && lhs.fullscreen == rhs.fullscreen;
}

static eBackendResizeEdge normalizeResizeEdge(xdgToplevelResizeEdge edge) {
    switch (edge) {
        case XDG_TOPLEVEL_RESIZE_EDGE_TOP: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_TOP;
        case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_BOTTOM;
        case XDG_TOPLEVEL_RESIZE_EDGE_LEFT: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_LEFT;
        case XDG_TOPLEVEL_RESIZE_EDGE_RIGHT: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_RIGHT;
        case XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_TOP_LEFT;
        case XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_TOP_RIGHT;
        case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_BOTTOM_LEFT;
        case XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_BOTTOM_RIGHT;
        case XDG_TOPLEVEL_RESIZE_EDGE_NONE: return eBackendResizeEdge::BACKEND_RESIZE_EDGE_NONE;
    }

    return eBackendResizeEdge::BACKEND_RESIZE_EDGE_NONE;
}

CWaylandBackend::CWaylandBackend(SP<CXDGSurfaceResource> resource) : m_resource(resource) {
    if (!resource)
        return;

    m_surface       = resource->m_surface;
    m_mapped        = resource->m_mapped;
    m_initialCommit = resource->m_initialCommit;

    if (const auto SURFACE = m_surface.lock()) {
        m_reportedSize = SURFACE->m_current.ackedSize;
    }

    if (const auto OWNER = resource->m_owner.lock()) {
        m_clientID = {
            .type = eBackendType::WINDOW_BACKEND_WAYLAND,
            .id   = rc<uintptr_t>(OWNER.get()),
        };
        wl_client_get_credentials(OWNER->client(), &m_pid, nullptr, nullptr);
    }

    updateGeometry(false);
    updateGeometryHints();
    updateMetadata(false);
    updateTraits(false);

    m_listeners.map     = resource->m_events.map.listen([this] {
        m_mapped = true;
        updateGeometry(true);
        updateGeometryHints();
        updateTraits(true);
        m_events.map.emit();
    });
    m_listeners.unmap   = resource->m_events.unmap.listen([this] {
        m_mapped = false;
        m_events.unmap.emit();
    });
    m_listeners.commit  = resource->m_events.commit.listen([this] {
        const bool INITIAL = m_initialCommit;

        updateGeometry(true);
        updateGeometryHints();
        updateMetadata(true);
        updateTraits(true);

        if (const auto SURFACE = surface())
            m_reportedSize = SURFACE->m_current.ackedSize;

        m_events.commit.emit(INITIAL);
        m_initialCommit = false;
    });
    m_listeners.destroy = resource->m_events.destroy.listen([this] {
        updateGeometry(false);
        updateGeometryHints();
        updateMetadata(false);
        updateTraits(false);

        m_destroyed = true;
        m_mapped    = false;
        m_listeners = {};
        m_events.destroy.emit();
    });
    m_listeners.ack     = resource->m_events.ack.listen([this](uint32_t serial) { onAck(serial); });

    const auto TOPLEVEL = resource->m_toplevel.lock();
    if (TOPLEVEL) {
        m_listeners.metadata   = TOPLEVEL->m_events.metadataChanged.listen([this] { updateMetadata(true); });
        m_listeners.state      = TOPLEVEL->m_events.stateChanged.listen([this] {
            const auto RESOURCE = m_resource.lock();
            const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
            if (!TOPLEVEL)
                return;

            updateTraits(true);
            m_events.stateRequest.emit({
                .fullscreen        = TOPLEVEL->m_state.requestsFullscreen,
                .fullscreenMonitor = TOPLEVEL->m_state.requestsFullscreenMonitor,
                .maximized         = TOPLEVEL->m_state.requestsMaximize,
                .minimized         = TOPLEVEL->m_state.requestsMinimize,
            });
        });
        m_listeners.sizeLimits = TOPLEVEL->m_events.sizeLimitsChanged.listen([this] {
            updateGeometryHints();
            updateTraits(true);
        });
        m_listeners.move       = TOPLEVEL->m_events.requestMove.listen([this](const SXDGToplevelMoveRequest&) { m_events.moveRequest.emit(); });
        m_listeners.resize =
            TOPLEVEL->m_events.requestResize.listen([this](const SXDGToplevelResizeRequest& request) { m_events.resizeRequest.emit(normalizeResizeEdge(request.edges)); });
    }

    if (const auto OWNER = resource->m_owner.lock())
        m_listeners.pong = OWNER->m_events.pong.listen([this] { m_events.pong.emit(); });

    m_listeners.newPopup = resource->m_events.newPopup.listen([this](const auto& resource) { m_events.newPopup.emit(makeWaylandPopupBackend(resource)); });
}

void CWaylandBackend::attach(PHLWINDOWREF window) {
    m_window = window;

    const auto RESOURCE = m_resource.lock();
    const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
    if (TOPLEVEL)
        TOPLEVEL->m_window = window;
}

bool CWaylandBackend::valid() const {
    const auto RESOURCE = m_resource.lock();
    return !m_destroyed && RESOURCE && RESOURCE->good() && RESOURCE->m_toplevel;
}

bool CWaylandBackend::isMapped() const {
    return m_mapped;
}

eBackendType CWaylandBackend::type() const {
    return eBackendType::WINDOW_BACKEND_WAYLAND;
}

pid_t CWaylandBackend::pid() const {
    return m_pid;
}

SP<CWLSurfaceResource> CWaylandBackend::surface() const {
    return m_surface.lock();
}

PHLWINDOW CWaylandBackend::parent() const {
    const auto RESOURCE = m_resource.lock();
    const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
    if (!TOPLEVEL || !TOPLEVEL->m_parent || !TOPLEVEL->m_parent->m_window)
        return nullptr;

    const auto PARENT = TOPLEVEL->m_parent->m_window.lock();
    return validMapped(PARENT) ? PARENT : nullptr;
}

SBackendClientID CWaylandBackend::clientID() const {
    return m_clientID;
}

bool CWaylandBackend::initialCommit() const {
    return m_initialCommit;
}

SClientGeometry CWaylandBackend::geometry() const {
    return m_geometry;
}

SGeometryHints CWaylandBackend::geometryHints(eBackendState state) const {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE && RESOURCE->m_toplevel)
        return geometryHintsFrom(RESOURCE, state);

    return state == eBackendState::BACKEND_STATE_PENDING ? m_pendingGeometryHints : m_currentGeometryHints;
}

SBackendMetadata CWaylandBackend::metadata() const {
    const auto RESOURCE = m_resource.lock();
    const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
    return TOPLEVEL ? metadataFrom(TOPLEVEL) : m_metadata;
}

SBackendTraits CWaylandBackend::traits() const {
    const auto RESOURCE = m_resource.lock();
    const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
    return TOPLEVEL ? traitsFrom(TOPLEVEL) : m_traits;
}

double CWaylandBackend::surfaceScale() const {
    return 1.0;
}

Vector2D CWaylandBackend::reportedSize() const {
    if (const auto SURFACE = surface())
        return SURFACE->m_current.ackedSize;

    return m_reportedSize;
}

CBox CWaylandBackend::clientToLogical(const CBox& box, PHLMONITOR) const {
    return box;
}

CBox CWaylandBackend::logicalToClient(const CBox& box, PHLMONITOR) const {
    return box;
}

Vector2D CWaylandBackend::surfaceLocalToBuffer(const Vector2D& local) const {
    return local;
}

Vector2D CWaylandBackend::bufferToSurfaceLocal(const Vector2D& buffer) const {
    return buffer;
}

void CWaylandBackend::configure(const CBox& logicalBox, PHLMONITOR preferredMonitor, bool force) {
    const auto RESOURCE = m_resource.lock();
    const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
    if (!TOPLEVEL)
        return;

    const auto CLIENT_BOX = logicalToClient(logicalBox, preferredMonitor);
    if (!force && m_pendingReportedSize == CLIENT_BOX.size())
        return;

    m_pendingReportedSize = CLIENT_BOX.size();
    recordConfiguredSize(CLIENT_BOX.size());
    m_configureAcks.add(TOPLEVEL->setSize(CLIENT_BOX.size()), CLIENT_BOX.size().floor());
}

void CWaylandBackend::acknowledgeConfigure(const CBox& clientBox) {
    const auto SURFACE = surface();
    if (!SURFACE)
        return;

    SURFACE->m_pending.ackedSize          = clientBox.size();
    SURFACE->m_pending.updated.bits.acked = true;
}

void CWaylandBackend::setActive(bool active) {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE && RESOURCE->m_toplevel)
        RESOURCE->m_toplevel->setActive(active);
}

void CWaylandBackend::setFullscreen(bool fullscreen) {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE && RESOURCE->m_toplevel)
        RESOURCE->m_toplevel->setFullscreen(fullscreen);
}

void CWaylandBackend::setMaximized(bool maximized) {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE && RESOURCE->m_toplevel)
        RESOURCE->m_toplevel->setMaximized(maximized);
}

void CWaylandBackend::setResizing(bool resizing) {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE && RESOURCE->m_toplevel)
        RESOURCE->m_toplevel->setResizing(resizing);
}

bool CWaylandBackend::setSuspended(bool suspended) {
    const auto RESOURCE = m_resource.lock();
    if (!RESOURCE || !RESOURCE->m_toplevel)
        return false;

    RESOURCE->m_toplevel->setSuspeneded(suspended);
    return true;
}

void CWaylandBackend::setMinimized(bool) {
    ;
}

void CWaylandBackend::restackToTop() {
    ;
}

void CWaylandBackend::close() {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE && RESOURCE->m_toplevel)
        RESOURCE->m_toplevel->close();
}

void CWaylandBackend::ping() {
    const auto RESOURCE = m_resource.lock();
    if (RESOURCE && RESOURCE->m_owner)
        RESOURCE->m_owner->ping();
}

void CWaylandBackend::updateGeometry(bool emitEvent) {
    const auto RESOURCE = m_resource.lock();
    if (!RESOURCE)
        return;

    const SClientGeometry GEOMETRY = {
        .box                   = RESOURCE->m_current.geometry,
        .positionAuthoritative = false,
    };
    if (GEOMETRY.box == m_geometry.box && GEOMETRY.positionAuthoritative == m_geometry.positionAuthoritative)
        return;

    m_geometry = GEOMETRY;
    if (emitEvent)
        m_events.geometryChanged.emit(m_geometry.box);
}

void CWaylandBackend::updateGeometryHints() {
    const auto RESOURCE = m_resource.lock();
    if (!RESOURCE)
        return;

    m_currentGeometryHints = geometryHintsFrom(RESOURCE, eBackendState::BACKEND_STATE_CURRENT);
    m_pendingGeometryHints = geometryHintsFrom(RESOURCE, eBackendState::BACKEND_STATE_PENDING);
}

void CWaylandBackend::updateMetadata(bool emitEvent) {
    const auto RESOURCE = m_resource.lock();
    const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
    if (!TOPLEVEL)
        return;

    const auto METADATA = metadataFrom(TOPLEVEL);
    if (metadataEqual(METADATA, m_metadata))
        return;

    m_metadata = METADATA;
    if (emitEvent)
        m_events.metadataChanged.emit(m_metadata);
}

void CWaylandBackend::updateTraits(bool emitEvent) {
    const auto RESOURCE = m_resource.lock();
    const auto TOPLEVEL = RESOURCE ? RESOURCE->m_toplevel.lock() : nullptr;
    if (!TOPLEVEL)
        return;

    const auto TRAITS = traitsFrom(TOPLEVEL);
    if (traitsEqual(TRAITS, m_traits))
        return;

    m_traits = TRAITS;
    if (emitEvent)
        m_events.traitsChanged.emit(m_traits);
}

void CWaylandBackend::onAck(uint32_t serial) {
    const auto ACKED_SIZE = m_configureAcks.acknowledge(serial);
    if (!ACKED_SIZE)
        return;

    acknowledgeConfigure(CBox{{}, *ACKED_SIZE});
}
