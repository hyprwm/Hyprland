#include "X11Backend.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <span>

#include "Window.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../managers/XWaylandManager.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../protocols/core/Compositor.hpp"
#include "../../../xwayland/XSurface.hpp"
#include "../../state/WindowState.hpp"

#ifndef NO_XWAYLAND
#include "../../../xwayland/XWayland.hpp"
#endif

using namespace Desktop::View;

static std::string normalizedX11String(std::string value) {
    if (const auto NULL_POS = value.find('\0'); NULL_POS != std::string::npos)
        value.resize(NULL_POS);

    return value;
}

static SBackendMetadata metadataFrom(const SP<CXWaylandSurface>& surface) {
    if (!surface)
        return {};

    return {
        .title = normalizedX11String(surface->m_state.title),
        .appID = normalizedX11String(surface->m_state.appid),
    };
}

#ifndef NO_XWAYLAND
static bool hasAtom(const SP<CXWaylandSurface>& surface, uint32_t atom) {
    return std::ranges::find(surface->m_atoms, atom) != surface->m_atoms.end();
}

static bool hasAnyAtom(const SP<CXWaylandSurface>& surface, const std::span<const uint32_t> atoms) {
    return std::ranges::any_of(atoms, [&surface](const auto atom) { return hasAtom(surface, atom); });
}
#endif

static SBackendTraits traitsFrom(const SP<CXWaylandSurface>& surface) {
    if (!surface)
        return {};

    bool FLOATING_ATOM         = false;
    bool NO_INITIAL_FOCUS_ATOM = false;
    bool PREVENTS_FOCUS_ATOM   = false;
    bool NO_BORDER_ATOM        = false;

#ifndef NO_XWAYLAND
    static const std::array FLOATING_ATOMS = {
        HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"],       HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"],        HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLBAR"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_UTILITY"],      HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"],       HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_DOCK"],         HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"], HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"],
        HYPRATOMS["_KDE_NET_WM_WINDOW_TYPE_OVERRIDE"],
    };
    static const std::array NO_BORDER_ATOMS = {
        HYPRATOMS["_NET_WM_WINDOW_TYPE_POPUP_MENU"], HYPRATOMS["_NET_WM_WINDOW_TYPE_NOTIFICATION"], HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_COMBO"],      HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"],         HYPRATOMS["_NET_WM_WINDOW_TYPE_SPLASH"],
        HYPRATOMS["_NET_WM_WINDOW_TYPE_TOOLTIP"],
    };

    for (const auto atom : surface->m_atoms) {
        if (std::ranges::find(FLOATING_ATOMS, atom) == FLOATING_ATOMS.end())
            continue;

        FLOATING_ATOM         = true;
        NO_INITIAL_FOCUS_ATOM = atom != HYPRATOMS["_NET_WM_WINDOW_TYPE_DIALOG"];
        PREVENTS_FOCUS_ATOM   = atom == HYPRATOMS["_NET_WM_WINDOW_TYPE_DROPDOWN_MENU"] || atom == HYPRATOMS["_NET_WM_WINDOW_TYPE_MENU"];
        break;
    }
    NO_BORDER_ATOM = hasAnyAtom(surface, NO_BORDER_ATOMS);
#endif

    const auto HINTS      = surface->m_sizeHints.get();
    const bool FIXED_SIZE = HINTS && HINTS->min_width > 0 && HINTS->min_height > 0 && HINTS->max_width > 0 && HINTS->max_height > 0 && HINTS->min_width == HINTS->max_width &&
        HINTS->min_height == HINTS->max_height;
    const bool ROLE_FLOATS     = surface->m_role.contains("task_dialog") || surface->m_role.contains("pop-up");
    const bool HAS_MODAL_CHILD = std::ranges::any_of(surface->m_children, [](const auto& child) { return child && child->m_modal; });

    return {
        .overrideRedirect       = surface->m_overrideRedirect,
        .modal                  = surface->m_modal,
        .hasModalChild          = HAS_MODAL_CHILD,
        .transient              = surface->m_transient,
        .wantsFocus             = surface->wantsFocus(),
        .suggestsFloat          = FLOATING_ATOM || surface->m_modal || surface->m_transient || ROLE_FLOATS || surface->m_overrideRedirect || surface->m_parent || FIXED_SIZE,
        .suggestsNoInitialFocus = NO_INITIAL_FOCUS_ATOM,
        .preventsFocus          = PREVENTS_FOCUS_ATOM,
        .suggestsNoBorder       = NO_BORDER_ATOM || surface->m_overrideRedirect,
        .fullscreen             = surface->m_fullscreen,
    };
}

static SGeometryHints geometryHintsFrom(const SP<CXWaylandSurface>& surface, double scale) {
    if (!surface || !surface->m_sizeHints)
        return {};

    const auto HINTS   = surface->m_sizeHints.get();
    Vector2D   minSize = Vector2D{HINTS->min_width, HINTS->min_height}.clamp({1, 1}) / scale;
    Vector2D   maxSize = {HINTS->max_width, HINTS->max_height};

    if (maxSize.x < 5)
        maxSize.x = std::numeric_limits<double>::max();
    else
        maxSize.x /= scale;
    if (maxSize.y < 5)
        maxSize.y = std::numeric_limits<double>::max();
    else
        maxSize.y /= scale;

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

CX11Backend::CX11Backend(SP<CXWaylandSurface> surface) : m_xwaylandSurface(surface) {
    if (!surface)
        return;

    m_pid                 = surface->m_pid;
    m_mapped              = surface->m_mapped;
    m_clientID.id         = surface->m_xID;
    m_reportedPosition    = surface->m_geometry.pos();
    m_reportedSize        = surface->m_geometry.size();
    m_pendingReportedSize = m_reportedSize;
    m_surface             = surface->m_surface;
    m_geometryHints       = geometryHintsFrom(surface, surfaceScale());
    updateGeometry(false);
    updateMetadata(false);
    updateTraits(false);

    m_listeners.map            = surface->m_events.map.listen([this] {
        m_mapped = true;
        updateSurface(true);
        updateGeometry(true);
        updateMetadata(true);
        updateTraits(true);
        m_events.map.emit();
    });
    m_listeners.unmap          = surface->m_events.unmap.listen([this] {
        m_mapped = false;
        m_events.unmap.emit();
    });
    m_listeners.commit         = surface->m_events.commit.listen([this] {
        if (m_pendingReportedSize)
            m_reportedSize = *m_pendingReportedSize;

        updateGeometry(true);
        updateMetadata(true);
        updateTraits(true);
        m_events.commit.emit(false);
    });
    m_listeners.destroy        = surface->m_events.destroy.listen([this] {
        updateGeometry(false);
        updateMetadata(false);
        updateTraits(false);

        m_destroyed = true;
        m_mapped    = false;
        m_listeners = {};
        m_events.destroy.emit();
    });
    m_listeners.resourceChange = surface->m_events.resourceChange.listen([this] {
        updateSurface(true);
        updateMetadata(true);
        updateTraits(true);
    });
    m_listeners.state          = surface->m_events.stateChanged.listen([this] {
        const auto SURFACE = m_xwaylandSurface.lock();
        if (!SURFACE)
            return;

        const SBackendStateRequest REQUEST = {
            .fullscreen = SURFACE->m_state.requestsFullscreen,
            .maximized  = SURFACE->m_state.requestsMaximize,
            .minimized  = SURFACE->m_state.requestsMinimize,
        };

        SURFACE->m_state.requestsFullscreen.reset();
        SURFACE->m_state.requestsMaximize.reset();
        SURFACE->m_state.requestsMinimize.reset();

        updateTraits(true);
        m_events.stateRequest.emit(REQUEST);
    });
    m_listeners.metadata       = surface->m_events.metadataChanged.listen([this] {
        updateMetadata(true);
        updateTraits(true);
    });
    m_listeners.configureRequest =
        surface->m_events.configureRequest.listen([this](const CBox& box) { m_events.configureRequest.emit(clientToLogical(box, preferredMonitor(nullptr))); });
    m_listeners.setGeometry = surface->m_events.setGeometry.listen([this] {
        updateGeometry(true);
        updateTraits(true);
    });
    m_listeners.activate    = surface->m_events.activate.listen([this] { m_events.activationRequest.emit(); });
    m_listeners.pong        = surface->m_events.pong.listen([this] { m_events.pong.emit(); });
}

void CX11Backend::attach(PHLWINDOWREF window) {
    m_window = window;
    updateGeometry(false);
    updateTraits(false);
}

bool CX11Backend::valid() const {
    return !m_destroyed && m_xwaylandSurface;
}

bool CX11Backend::isMapped() const {
    return m_mapped;
}

eBackendType CX11Backend::type() const {
    return eBackendType::WINDOW_BACKEND_X11;
}

pid_t CX11Backend::pid() const {
    return m_pid;
}

SP<CWLSurfaceResource> CX11Backend::surface() const {
    return m_surface.lock();
}

PHLWINDOW CX11Backend::parent() const {
    const auto XSURFACE = m_xwaylandSurface.lock();
    if (!XSURFACE || !XSURFACE->m_parent)
        return nullptr;

    for (const auto& WINDOW : Desktop::windowState()->windows()) {
        if (WINDOW && WINDOW->backend().clientID() == SBackendClientID{.type = eBackendType::WINDOW_BACKEND_X11, .id = XSURFACE->m_parent->m_xID})
            return validMapped(WINDOW) ? WINDOW : nullptr;
    }

    return nullptr;
}

SBackendClientID CX11Backend::clientID() const {
    return m_clientID;
}

bool CX11Backend::initialCommit() const {
    return false;
}

SClientGeometry CX11Backend::geometry() const {
    const auto SURFACE         = m_xwaylandSurface.lock();
    const auto CLIENT_GEOMETRY = SURFACE ? SURFACE->m_geometry : m_geometry.box;
    const auto SCALE           = surfaceScale();

    return {
        .box                   = {g_pXWaylandManager->xwaylandToWaylandCoords(CLIENT_GEOMETRY.pos()), CLIENT_GEOMETRY.size() / SCALE},
        .positionAuthoritative = SURFACE ? SURFACE->m_overrideRedirect : m_geometry.positionAuthoritative,
    };
}

SGeometryHints CX11Backend::geometryHints(eBackendState) const {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        return geometryHintsFrom(SURFACE, surfaceScale());

    return m_geometryHints;
}

SBackendMetadata CX11Backend::metadata() const {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        return metadataFrom(SURFACE);

    return m_metadata;
}

SBackendTraits CX11Backend::traits() const {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        return traitsFrom(SURFACE);

    return m_traits;
}

double CX11Backend::surfaceScale() const {
    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    const auto  MONITOR = preferredMonitor(nullptr);
    return *PXWLFORCESCALEZERO && MONITOR ? MONITOR->m_scale : 1.0;
}

Vector2D CX11Backend::reportedSize() const {
    return m_reportedSize;
}

CBox CX11Backend::clientToLogical(const CBox& box, PHLMONITOR preferredMonitor_) const {
    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    const auto  MONITOR = preferredMonitor(preferredMonitor_);
    const auto  SCALE   = *PXWLFORCESCALEZERO && MONITOR ? MONITOR->m_scale : 1.0;
    return {g_pXWaylandManager->xwaylandToWaylandCoords(box.pos(), MONITOR), box.size() / SCALE};
}

CBox CX11Backend::logicalToClient(const CBox& box, PHLMONITOR preferredMonitor_) const {
    static auto PXWLFORCESCALEZERO = CConfigValue<Config::INTEGER>("xwayland:force_zero_scaling");

    const auto  MONITOR = preferredMonitor(preferredMonitor_);
    const auto  SCALE   = *PXWLFORCESCALEZERO && MONITOR ? MONITOR->m_scale : 1.0;
    return {g_pXWaylandManager->waylandToXWaylandCoords(box.pos(), MONITOR), (box.size().clamp({1, 1}) * SCALE).round()};
}

Vector2D CX11Backend::surfaceLocalToBuffer(const Vector2D& local) const {
    return local * surfaceScale();
}

Vector2D CX11Backend::bufferToSurfaceLocal(const Vector2D& buffer) const {
    return buffer / surfaceScale();
}

void CX11Backend::configure(const CBox& logicalBox, PHLMONITOR preferredMonitor_, bool force) {
    const auto SURFACE = m_xwaylandSurface.lock();
    if (!SURFACE)
        return;

    const auto CLIENT_BOX = logicalToClient(logicalBox, preferredMonitor_);
    if (!force && m_pendingReportedSize == CLIENT_BOX.size() && m_reportedPosition == CLIENT_BOX.pos())
        return;

    m_reportedPosition    = CLIENT_BOX.pos();
    m_pendingReportedSize = CLIENT_BOX.size();
    recordConfiguredSize(CLIENT_BOX.size());
    SURFACE->configure(CLIENT_BOX);
}

void CX11Backend::acknowledgeConfigure(const CBox& clientBox) {
    m_reportedPosition    = clientBox.pos();
    m_reportedSize        = clientBox.size();
    m_pendingReportedSize = clientBox.size();
}

void CX11Backend::setActive(bool active) {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        SURFACE->activate(active);
}

void CX11Backend::setFullscreen(bool fullscreen) {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        SURFACE->setFullscreen(fullscreen);
}

void CX11Backend::setMaximized(bool maximized) {
    const auto SURFACE = m_xwaylandSurface.lock();
    if (!SURFACE || SURFACE->m_maximized == maximized)
        return;

    SURFACE->m_maximized = maximized;
    SURFACE->setFullscreen(SURFACE->m_fullscreen);
}

void CX11Backend::setResizing(bool) {
    ;
}

bool CX11Backend::setSuspended(bool) {
    return false;
}

void CX11Backend::setMinimized(bool minimized) {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        SURFACE->setMinimized(minimized);
}

void CX11Backend::restackToTop() {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        SURFACE->restackToTop();
}

void CX11Backend::close() {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        SURFACE->close();
}

void CX11Backend::ping() {
    if (const auto SURFACE = m_xwaylandSurface.lock())
        SURFACE->ping();
}

PHLMONITOR CX11Backend::preferredMonitor(PHLMONITOR monitor) const {
    if (monitor)
        return monitor;

    const auto WINDOW = m_window.lock();
    return WINDOW ? WINDOW->m_monitor.lock() : nullptr;
}

void CX11Backend::updateGeometry(bool emitEvent) {
    const auto SURFACE = m_xwaylandSurface.lock();
    if (!SURFACE)
        return;

    const SClientGeometry GEOMETRY = {
        .box                   = SURFACE->m_geometry,
        .positionAuthoritative = SURFACE->m_overrideRedirect,
    };
    if (GEOMETRY.box == m_geometry.box && GEOMETRY.positionAuthoritative == m_geometry.positionAuthoritative)
        return;

    m_geometry = GEOMETRY;
    if (emitEvent)
        m_events.geometryChanged.emit(clientToLogical(m_geometry.box, preferredMonitor(nullptr)));
}

void CX11Backend::updateMetadata(bool emitEvent) {
    const auto SURFACE = m_xwaylandSurface.lock();
    if (!SURFACE)
        return;

    const auto METADATA = metadataFrom(SURFACE);
    if (metadataEqual(METADATA, m_metadata))
        return;

    m_metadata = METADATA;
    if (emitEvent)
        m_events.metadataChanged.emit(m_metadata);
}

void CX11Backend::updateTraits(bool emitEvent) {
    const auto SURFACE = m_xwaylandSurface.lock();
    if (!SURFACE)
        return;

    m_geometryHints   = geometryHintsFrom(SURFACE, surfaceScale());
    const auto TRAITS = traitsFrom(SURFACE);
    if (traitsEqual(TRAITS, m_traits))
        return;

    m_traits = TRAITS;
    if (emitEvent)
        m_events.traitsChanged.emit(m_traits);
}

void CX11Backend::updateSurface(bool emitEvent) {
    const auto XSURFACE = m_xwaylandSurface.lock();
    if (!XSURFACE)
        return;

    const auto SURFACE = XSURFACE->m_surface.lock();
    if (m_surface == SURFACE)
        return;

    m_surface = SURFACE;
    if (emitEvent)
        m_events.surfaceChanged.emit(SURFACE);
}
