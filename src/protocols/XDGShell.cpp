#include "XDGShell.hpp"
#include "XDGDialog.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/ANRManager.hpp"
#include "../helpers/Monitor.hpp"
#include "core/Seat.hpp"
#include "core/Compositor.hpp"
#include "protocols/core/Output.hpp"
#include <cstring>
#include <ranges>

void SXDGPositionerState::setAnchor(xdgPositionerAnchor edges) {
    anchor.setTop(edges == XDG_POSITIONER_ANCHOR_TOP || edges == XDG_POSITIONER_ANCHOR_TOP_LEFT || edges == XDG_POSITIONER_ANCHOR_TOP_RIGHT);
    anchor.setLeft(edges == XDG_POSITIONER_ANCHOR_LEFT || edges == XDG_POSITIONER_ANCHOR_TOP_LEFT || edges == XDG_POSITIONER_ANCHOR_BOTTOM_LEFT);
    anchor.setBottom(edges == XDG_POSITIONER_ANCHOR_BOTTOM || edges == XDG_POSITIONER_ANCHOR_BOTTOM_LEFT || edges == XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT);
    anchor.setRight(edges == XDG_POSITIONER_ANCHOR_RIGHT || edges == XDG_POSITIONER_ANCHOR_TOP_RIGHT || edges == XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT);
}

void SXDGPositionerState::setGravity(xdgPositionerGravity edges) {
    gravity.setTop(edges == XDG_POSITIONER_GRAVITY_TOP || edges == XDG_POSITIONER_GRAVITY_TOP_LEFT || edges == XDG_POSITIONER_GRAVITY_TOP_RIGHT);
    gravity.setLeft(edges == XDG_POSITIONER_GRAVITY_LEFT || edges == XDG_POSITIONER_GRAVITY_TOP_LEFT || edges == XDG_POSITIONER_GRAVITY_BOTTOM_LEFT);
    gravity.setBottom(edges == XDG_POSITIONER_GRAVITY_BOTTOM || edges == XDG_POSITIONER_GRAVITY_BOTTOM_LEFT || edges == XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
    gravity.setRight(edges == XDG_POSITIONER_GRAVITY_RIGHT || edges == XDG_POSITIONER_GRAVITY_TOP_RIGHT || edges == XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
}

CXDGPopupResource::CXDGPopupResource(SP<CXdgPopup> resource_, SP<CXDGSurfaceResource> owner_, SP<CXDGSurfaceResource> surface_, SP<CXDGPositionerResource> positioner) :
    m_surface(surface_), m_parent(owner_), m_resource(resource_), m_positionerRules(positioner) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CXdgPopup* r) {
        if (m_surface && m_surface->m_mapped)
            m_surface->m_events.unmap.emit();
        PROTO::xdgShell->onPopupDestroy(m_self);
        m_events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CXdgPopup* r) {
        if (m_surface && m_surface->m_mapped)
            m_surface->m_events.unmap.emit();
        PROTO::xdgShell->onPopupDestroy(m_self);
        m_events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });

    m_resource->setReposition([this](CXdgPopup* r, wl_resource* positionerRes, uint32_t token) {
        LOGM(LOG, "Popup {:x} asks for reposition", (uintptr_t)this);
        m_lastRepositionToken = token;
        auto pos              = CXDGPositionerResource::fromResource(positionerRes);
        if (!pos)
            return;
        m_positionerRules = CXDGPositionerRules{pos};
        m_events.reposition.emit();
    });

    m_resource->setGrab([this](CXdgPopup* r, wl_resource* seat, uint32_t serial) {
        LOGM(LOG, "xdg_popup {:x} requests grab", (uintptr_t)this);
        PROTO::xdgShell->addOrStartGrab(m_self.lock());
    });

    if (m_parent)
        m_taken = true;
}

CXDGPopupResource::~CXDGPopupResource() {
    PROTO::xdgShell->onPopupDestroy(m_self);
    m_events.destroy.emit();
}

void CXDGPopupResource::applyPositioning(const CBox& box, const Vector2D& t1coord) {
    CBox constraint = box.copy().translate(m_surface->m_pending.geometry.pos());

    m_geometry = m_positionerRules.getPosition(constraint, accumulateParentOffset() + t1coord);

    LOGM(LOG, "Popup {:x} gets unconstrained to {} {}", (uintptr_t)this, m_geometry.pos(), m_geometry.size());

    configure(m_geometry);

    if UNLIKELY (m_lastRepositionToken)
        repositioned();
}

Vector2D CXDGPopupResource::accumulateParentOffset() {
    SP<CXDGSurfaceResource> current = m_parent.lock();
    Vector2D                off;
    while (current) {
        off += current->m_current.geometry.pos();
        if (current->m_popup) {
            off += current->m_popup->m_geometry.pos();
            current = current->m_popup->m_parent.lock();
        } else
            break;
    }
    return off;
}

SP<CXDGPopupResource> CXDGPopupResource::fromResource(wl_resource* res) {
    auto data = (CXDGPopupResource*)(((CXdgPopup*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CXDGPopupResource::good() {
    return m_resource->resource();
}

void CXDGPopupResource::configure(const CBox& box) {
    m_resource->sendConfigure(box.x, box.y, box.w, box.h);
    if (m_surface)
        m_surface->scheduleConfigure();
}

void CXDGPopupResource::done() {
    m_events.dismissed.emit();
    m_resource->sendPopupDone();
}

void CXDGPopupResource::repositioned() {
    if LIKELY (!m_lastRepositionToken)
        return;

    LOGM(LOG, "repositioned: sending reposition token {}", m_lastRepositionToken);

    m_resource->sendRepositioned(m_lastRepositionToken);
    m_lastRepositionToken = 0;
}

CXDGToplevelResource::CXDGToplevelResource(SP<CXdgToplevel> resource_, SP<CXDGSurfaceResource> owner_) : m_owner(owner_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CXdgToplevel* r) {
        m_events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CXdgToplevel* r) {
        m_events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });

    if (m_resource->version() >= 5) {
        wl_array arr;
        wl_array_init(&arr);
        auto p = (uint32_t*)wl_array_add(&arr, sizeof(uint32_t));
        *p     = XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;
        p      = (uint32_t*)wl_array_add(&arr, sizeof(uint32_t));
        *p     = XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;
        m_resource->sendWmCapabilities(&arr);
        wl_array_release(&arr);
    }

    if (m_resource->version() >= 2) {
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_LEFT);
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_RIGHT);
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_TOP);
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_BOTTOM);
    }

    m_resource->setSetTitle([this](CXdgToplevel* r, const char* t) {
        m_state.title = t;
        m_events.metadataChanged.emit();
    });

    m_resource->setSetAppId([this](CXdgToplevel* r, const char* id) {
        m_state.appid = id;
        m_events.metadataChanged.emit();
    });

    m_resource->setSetMaxSize([this](CXdgToplevel* r, int32_t x, int32_t y) {
        m_pending.maxSize = {x, y};
        m_events.sizeLimitsChanged.emit();
    });

    m_resource->setSetMinSize([this](CXdgToplevel* r, int32_t x, int32_t y) {
        m_pending.minSize = {x, y};
        m_events.sizeLimitsChanged.emit();
    });

    m_resource->setSetMaximized([this](CXdgToplevel* r) {
        m_state.requestsMaximize = true;
        m_events.stateChanged.emit();
        m_state.requestsMaximize.reset();
    });

    m_resource->setUnsetMaximized([this](CXdgToplevel* r) {
        m_state.requestsMaximize = false;
        m_events.stateChanged.emit();
        m_state.requestsMaximize.reset();
    });

    m_resource->setSetFullscreen([this](CXdgToplevel* r, wl_resource* output) {
        if (output)
            if (const auto PM = CWLOutputResource::fromResource(output)->m_monitor; PM)
                m_state.requestsFullscreenMonitor = PM->m_id;

        m_state.requestsFullscreen = true;
        m_events.stateChanged.emit();
        m_state.requestsFullscreen.reset();
        m_state.requestsFullscreenMonitor.reset();
    });

    m_resource->setUnsetFullscreen([this](CXdgToplevel* r) {
        m_state.requestsFullscreen = false;
        m_events.stateChanged.emit();
        m_state.requestsFullscreen.reset();
    });

    m_resource->setSetMinimized([this](CXdgToplevel* r) {
        m_state.requestsMinimize = true;
        m_events.stateChanged.emit();
        m_state.requestsMinimize.reset();
    });

    m_resource->setSetParent([this](CXdgToplevel* r, wl_resource* parentR) {
        auto oldParent = m_parent;

        if (m_parent)
            std::erase(m_parent->m_children, m_self);

        auto newp = parentR ? CXDGToplevelResource::fromResource(parentR) : nullptr;
        m_parent  = newp;

        if (m_parent)
            m_parent->m_children.emplace_back(m_self);

        LOGM(LOG, "Toplevel {:x} sets parent to {:x}{}", (uintptr_t)this, (uintptr_t)newp.get(), (oldParent ? std::format(" (was {:x})", (uintptr_t)oldParent.get()) : ""));
    });
}

CXDGToplevelResource::~CXDGToplevelResource() {
    m_events.destroy.emit();
    if (m_parent)
        std::erase_if(m_parent->m_children, [this](const auto& other) { return !other || other.get() == this; });
}

SP<CXDGToplevelResource> CXDGToplevelResource::fromResource(wl_resource* res) {
    auto data = (CXDGToplevelResource*)(((CXdgToplevel*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CXDGToplevelResource::good() {
    return m_resource->resource();
}

bool CXDGToplevelResource::anyChildModal() {
    return std::ranges::any_of(m_children, [](const auto& child) { return child && child->m_dialog && child->m_dialog->modal; });
}

uint32_t CXDGToplevelResource::setSize(const Vector2D& size) {
    m_pendingApply.size = size;
    applyState();
    return m_owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setMaximized(bool maximized) {
    bool set = std::ranges::find(m_pendingApply.states, XDG_TOPLEVEL_STATE_MAXIMIZED) != m_pendingApply.states.end();

    if (maximized == set)
        return m_owner->m_scheduledSerial;

    if (maximized && !set)
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
    else if (!maximized && set)
        std::erase(m_pendingApply.states, XDG_TOPLEVEL_STATE_MAXIMIZED);
    applyState();
    return m_owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setFullscreen(bool fullscreen) {
    bool set = std::ranges::find(m_pendingApply.states, XDG_TOPLEVEL_STATE_FULLSCREEN) != m_pendingApply.states.end();

    if (fullscreen == set)
        return m_owner->m_scheduledSerial;

    if (fullscreen && !set)
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
    else if (!fullscreen && set)
        std::erase(m_pendingApply.states, XDG_TOPLEVEL_STATE_FULLSCREEN);
    applyState();
    return m_owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setActive(bool active) {
    bool set = std::ranges::find(m_pendingApply.states, XDG_TOPLEVEL_STATE_ACTIVATED) != m_pendingApply.states.end();

    if (active == set)
        return m_owner->m_scheduledSerial;

    if (active && !set)
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    else if (!active && set)
        std::erase(m_pendingApply.states, XDG_TOPLEVEL_STATE_ACTIVATED);
    applyState();
    return m_owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setSuspeneded(bool sus) {
    if (m_resource->version() < 6)
        return m_owner->scheduleConfigure(); // SUSPENDED is since 6

    bool set = std::ranges::find(m_pendingApply.states, XDG_TOPLEVEL_STATE_SUSPENDED) != m_pendingApply.states.end();

    if (sus == set)
        return m_owner->m_scheduledSerial;

    if (sus && !set)
        m_pendingApply.states.push_back(XDG_TOPLEVEL_STATE_SUSPENDED);
    else if (!sus && set)
        std::erase(m_pendingApply.states, XDG_TOPLEVEL_STATE_SUSPENDED);
    applyState();
    return m_owner->scheduleConfigure();
}

void CXDGToplevelResource::applyState() {
    wl_array arr;
    wl_array_init(&arr);

    if (!m_pendingApply.states.empty()) {
        wl_array_add(&arr, m_pendingApply.states.size() * sizeof(int));
        memcpy(arr.data, m_pendingApply.states.data(), m_pendingApply.states.size() * sizeof(int));
    }

    m_resource->sendConfigure(m_pendingApply.size.x, m_pendingApply.size.y, &arr);

    wl_array_release(&arr);
}

void CXDGToplevelResource::close() {
    m_resource->sendClose();
}

Vector2D CXDGToplevelResource::layoutMinSize() {
    Vector2D minSize;
    if (m_current.minSize.x > 1)
        minSize.x = m_owner ? m_current.minSize.x + m_owner->m_current.geometry.pos().x : m_current.minSize.x;
    if (m_current.minSize.y > 1)
        minSize.y = m_owner ? m_current.minSize.y + m_owner->m_current.geometry.pos().y : m_current.minSize.y;
    return minSize;
}

Vector2D CXDGToplevelResource::layoutMaxSize() {
    Vector2D maxSize;
    if (m_current.maxSize.x > 1)
        maxSize.x = m_owner ? m_current.maxSize.x + m_owner->m_current.geometry.pos().x : m_current.maxSize.x;
    if (m_current.maxSize.y > 1)
        maxSize.y = m_owner ? m_current.maxSize.y + m_owner->m_current.geometry.pos().y : m_current.maxSize.y;
    return maxSize;
}

CXDGSurfaceResource::CXDGSurfaceResource(SP<CXdgSurface> resource_, SP<CXDGWMBase> owner_, SP<CWLSurfaceResource> surface_) :
    m_owner(owner_), m_surface(surface_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CXdgSurface* r) {
        if (m_mapped)
            m_events.unmap.emit();
        m_events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CXdgSurface* r) {
        if (m_mapped)
            m_events.unmap.emit();
        m_events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });

    m_listeners.surfaceDestroy = m_surface->m_events.destroy.registerListener([this](std::any d) {
        LOGM(WARN, "wl_surface destroyed before its xdg_surface role object");
        m_listeners.surfaceDestroy.reset();
        m_listeners.surfaceCommit.reset();

        if (m_mapped)
            m_events.unmap.emit();

        m_mapped = false;
        m_surface.reset();
        m_events.destroy.emit();
    });

    m_listeners.surfaceCommit = m_surface->m_events.commit.registerListener([this](std::any d) {
        m_current = m_pending;
        if (m_toplevel)
            m_toplevel->m_current = m_toplevel->m_pending;

        if UNLIKELY (m_initialCommit && m_surface->m_pending.buffer) {
            m_resource->error(-1, "Buffer attached before initial commit");
            return;
        }

        if (m_surface->m_current.texture && !m_mapped) {
            // this forces apps to not draw CSD.
            if (m_toplevel)
                m_toplevel->setMaximized(true);

            m_mapped = true;
            m_surface->map();
            m_events.map.emit();
            return;
        }

        if (!m_surface->m_current.texture && m_mapped) {
            m_mapped = false;
            m_events.unmap.emit();
            m_surface->unmap();
            return;
        }

        m_events.commit.emit();
        m_initialCommit = false;
    });

    m_resource->setGetToplevel([this](CXdgSurface* r, uint32_t id) {
        const auto RESOURCE = PROTO::xdgShell->m_toplevels.emplace_back(makeShared<CXDGToplevelResource>(makeShared<CXdgToplevel>(r->client(), r->version(), id), m_self.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_toplevels.pop_back();
            return;
        }

        m_toplevel         = RESOURCE;
        m_toplevel->m_self = RESOURCE;

        LOGM(LOG, "xdg_surface {:x} gets a toplevel {:x}", (uintptr_t)m_owner.get(), (uintptr_t)RESOURCE.get());

        g_pCompositor->m_windowStack.add(CWindow::create(m_self.lock()));

        for (auto const& p : m_popups) {
            if (!p)
                continue;
            m_events.newPopup.emit(p);
        }
    });

    m_resource->setGetPopup([this](CXdgSurface* r, uint32_t id, wl_resource* parentXDG, wl_resource* positionerRes) {
        auto       parent     = parentXDG ? CXDGSurfaceResource::fromResource(parentXDG) : nullptr;
        auto       positioner = CXDGPositionerResource::fromResource(positionerRes);
        const auto RESOURCE =
            PROTO::xdgShell->m_popups.emplace_back(makeShared<CXDGPopupResource>(makeShared<CXdgPopup>(r->client(), r->version(), id), parent, m_self.lock(), positioner));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_popups.pop_back();
            return;
        }

        m_popup          = RESOURCE;
        RESOURCE->m_self = RESOURCE;

        LOGM(LOG, "xdg_surface {:x} gets a popup {:x} owner {:x}", (uintptr_t)m_self.get(), (uintptr_t)RESOURCE.get(), (uintptr_t)parent.get());

        if (!parent)
            return;

        parent->m_popups.emplace_back(RESOURCE);
        if (parent->m_mapped)
            parent->m_events.newPopup.emit(RESOURCE);
    });

    m_resource->setAckConfigure([this](CXdgSurface* r, uint32_t serial) {
        if (serial < m_lastConfigureSerial)
            return;
        m_lastConfigureSerial = serial;
        m_events.ack.emit(serial);
    });

    m_resource->setSetWindowGeometry([this](CXdgSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        LOGM(LOG, "xdg_surface {:x} requests geometry {}x{} {}x{}", (uintptr_t)this, x, y, w, h);
        m_pending.geometry = {x, y, w, h};
    });
}

CXDGSurfaceResource::~CXDGSurfaceResource() {
    m_events.destroy.emit();
    if (m_configureSource)
        wl_event_source_remove(m_configureSource);
    if (m_surface)
        m_surface->resetRole();
}

bool CXDGSurfaceResource::good() {
    return m_resource->resource();
}

SP<CXDGSurfaceResource> CXDGSurfaceResource::fromResource(wl_resource* res) {
    auto data = (CXDGSurfaceResource*)(((CXdgSurface*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

static void onConfigure(void* data) {
    ((CXDGSurfaceResource*)data)->configure();
}

uint32_t CXDGSurfaceResource::scheduleConfigure() {
    if (m_configureSource)
        return m_scheduledSerial;

    m_configureSource = wl_event_loop_add_idle(g_pCompositor->m_wlEventLoop, onConfigure, this);
    m_scheduledSerial = wl_display_next_serial(g_pCompositor->m_wlDisplay);

    return m_scheduledSerial;
}

void CXDGSurfaceResource::configure() {
    m_configureSource = nullptr;
    m_resource->sendConfigure(m_scheduledSerial);
}

CXDGPositionerResource::CXDGPositionerResource(SP<CXdgPositioner> resource_, SP<CXDGWMBase> owner_) : m_owner(owner_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setData(this);

    m_resource->setDestroy([this](CXdgPositioner* r) { PROTO::xdgShell->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgPositioner* r) { PROTO::xdgShell->destroyResource(this); });

    m_resource->setSetSize([this](CXdgPositioner* r, int32_t x, int32_t y) {
        if UNLIKELY (x <= 0 || y <= 0) {
            r->error(XDG_POSITIONER_ERROR_INVALID_INPUT, "Invalid size");
            return;
        }

        m_state.requestedSize = {x, y};
    });

    m_resource->setSetAnchorRect([this](CXdgPositioner* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        if UNLIKELY (w <= 0 || h <= 0) {
            r->error(XDG_POSITIONER_ERROR_INVALID_INPUT, "Invalid box");
            return;
        }

        m_state.anchorRect = {x, y, w, h};
    });

    m_resource->setSetOffset([this](CXdgPositioner* r, int32_t x, int32_t y) { m_state.offset = {x, y}; });

    m_resource->setSetAnchor([this](CXdgPositioner* r, xdgPositionerAnchor a) { m_state.setAnchor(a); });

    m_resource->setSetGravity([this](CXdgPositioner* r, xdgPositionerGravity g) { m_state.setGravity(g); });

    m_resource->setSetConstraintAdjustment([this](CXdgPositioner* r, xdgPositionerConstraintAdjustment a) { m_state.constraintAdjustment = (uint32_t)a; });

    // TODO: support this shit better. The current impl _works_, but is lacking and could be wrong in a few cases.
    // doesn't matter _that_ much for now, though.
}

SP<CXDGPositionerResource> CXDGPositionerResource::fromResource(wl_resource* res) {
    auto data = (CXDGPositionerResource*)(((CXdgPositioner*)wl_resource_get_user_data(res))->data());
    return data ? data->m_self.lock() : nullptr;
}

bool CXDGPositionerResource::good() {
    return m_resource->resource();
}

CXDGPositionerRules::CXDGPositionerRules(SP<CXDGPositionerResource> positioner) : m_state(positioner->m_state) {
    ;
}

CBox CXDGPositionerRules::getPosition(CBox constraint, const Vector2D& parentCoord) {
    Debug::log(LOG, "GetPosition with constraint {} {} and parent {}", constraint.pos(), constraint.size(), parentCoord);

    // padding
    constraint.expand(-4);

    auto anchorRect = m_state.anchorRect.copy().translate(parentCoord);

    auto width   = m_state.requestedSize.x;
    auto height  = m_state.requestedSize.y;
    auto gravity = m_state.gravity;

    auto anchorX = m_state.anchor.left() ? anchorRect.x : m_state.anchor.right() ? anchorRect.extent().x : anchorRect.middle().x;
    auto anchorY = m_state.anchor.top() ? anchorRect.y : m_state.anchor.bottom() ? anchorRect.extent().y : anchorRect.middle().y;

    auto calcEffectiveX = [&](CEdges anchorGravity, double anchorX) { return anchorGravity.left() ? anchorX - width : anchorGravity.right() ? anchorX : anchorX - width / 2; };
    auto calcEffectiveY = [&](CEdges anchorGravity, double anchorY) { return anchorGravity.top() ? anchorY - height : anchorGravity.bottom() ? anchorY : anchorY - height / 2; };

    auto calcRemainingWidth = [&](double effectiveX) {
        auto width = m_state.requestedSize.x;
        if (effectiveX < constraint.x) {
            auto diff  = constraint.x - effectiveX;
            effectiveX = constraint.x;
            width -= diff;
        }

        auto effectiveX2 = effectiveX + width;
        if (effectiveX2 > constraint.extent().x)
            width -= effectiveX2 - constraint.extent().x;

        return std::make_pair(effectiveX, width);
    };

    auto calcRemainingHeight = [&](double effectiveY) {
        auto height = m_state.requestedSize.y;
        if (effectiveY < constraint.y) {
            auto diff  = constraint.y - effectiveY;
            effectiveY = constraint.y;
            height -= diff;
        }

        auto effectiveY2 = effectiveY + height;
        if (effectiveY2 > constraint.extent().y)
            height -= effectiveY2 - constraint.extent().y;

        return std::make_pair(effectiveY, height);
    };

    auto effectiveX = calcEffectiveX(gravity, anchorX);
    auto effectiveY = calcEffectiveY(gravity, anchorY);

    // Note: the usage of offset is a guess which maintains compatibility with other compositors that were tested.
    // It considers the offset when deciding whether or not to flip but does not actually flip the offset, instead
    // applying it after the flip step.

    if (m_state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X) {
        auto flip = (gravity.left() && effectiveX + m_state.offset.x < constraint.x) || (gravity.right() && effectiveX + m_state.offset.x + width > constraint.extent().x);

        if (flip) {
            auto newGravity    = gravity ^ (CEdges::LEFT | CEdges::RIGHT);
            auto newAnchorX    = m_state.anchor.left() ? anchorRect.extent().x : m_state.anchor.right() ? anchorRect.x : anchorX;
            auto newEffectiveX = calcEffectiveX(newGravity, newAnchorX);

            if (calcRemainingWidth(newEffectiveX).second > calcRemainingWidth(effectiveX).second) {
                gravity    = newGravity;
                anchorX    = newAnchorX;
                effectiveX = newEffectiveX;
            }
        }
    }

    if (m_state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y) {
        auto flip =
            (m_state.gravity.top() && effectiveY + m_state.offset.y < constraint.y) || (m_state.gravity.bottom() && effectiveY + m_state.offset.y + height > constraint.extent().y);

        if (flip) {
            auto newGravity    = gravity ^ (CEdges::TOP | CEdges::BOTTOM);
            auto newAnchorY    = m_state.anchor.top() ? anchorRect.extent().y : m_state.anchor.bottom() ? anchorRect.y : anchorY;
            auto newEffectiveY = calcEffectiveY(newGravity, newAnchorY);

            if (calcRemainingHeight(newEffectiveY).second > calcRemainingHeight(effectiveY).second) {
                gravity    = newGravity;
                anchorY    = newAnchorY;
                effectiveY = newEffectiveY;
            }
        }
    }

    effectiveX += m_state.offset.x;
    effectiveY += m_state.offset.y;

    // Slide order is important for the case where the window is too large to fit on screen.

    if (m_state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X) {
        if (effectiveX + width > constraint.extent().x)
            effectiveX = constraint.extent().x - width;

        if (effectiveX < constraint.x)
            effectiveX = constraint.x;
    }

    if (m_state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y) {
        if (effectiveY + height > constraint.extent().y)
            effectiveY = constraint.extent().y - height;

        if (effectiveY < constraint.y)
            effectiveY = constraint.y;
    }

    if (m_state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X) {
        auto [newX, newWidth] = calcRemainingWidth(effectiveX);
        effectiveX            = newX;
        width                 = newWidth;
    }

    if (m_state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y) {
        auto [newY, newHeight] = calcRemainingHeight(effectiveY);
        effectiveY             = newY;
        height                 = newHeight;
    }

    return {effectiveX - parentCoord.x, effectiveY - parentCoord.y, width, height};
}

CXDGWMBase::CXDGWMBase(SP<CXdgWmBase> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CXdgWmBase* r) { PROTO::xdgShell->destroyResource(this); });
    m_resource->setOnDestroy([this](CXdgWmBase* r) { PROTO::xdgShell->destroyResource(this); });

    m_client = m_resource->client();

    m_resource->setCreatePositioner([this](CXdgWmBase* r, uint32_t id) {
        const auto RESOURCE =
            PROTO::xdgShell->m_positioners.emplace_back(makeShared<CXDGPositionerResource>(makeShared<CXdgPositioner>(r->client(), r->version(), id), m_self.lock()));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_positioners.pop_back();
            return;
        }

        RESOURCE->m_self = RESOURCE;

        m_positioners.emplace_back(RESOURCE);

        LOGM(LOG, "New xdg_positioner at {:x}", (uintptr_t)RESOURCE.get());
    });

    m_resource->setGetXdgSurface([this](CXdgWmBase* r, uint32_t id, wl_resource* surf) {
        auto SURF = CWLSurfaceResource::fromResource(surf);

        if UNLIKELY (!SURF) {
            r->error(-1, "Invalid surface passed");
            return;
        }

        if UNLIKELY (SURF->m_role->role() != SURFACE_ROLE_UNASSIGNED) {
            r->error(-1, "Surface already has a different role");
            return;
        }

        const auto RESOURCE =
            PROTO::xdgShell->m_surfaces.emplace_back(makeShared<CXDGSurfaceResource>(makeShared<CXdgSurface>(r->client(), r->version(), id), m_self.lock(), SURF));

        if UNLIKELY (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_surfaces.pop_back();
            return;
        }

        RESOURCE->m_self    = RESOURCE;
        RESOURCE->m_surface = SURF;
        SURF->m_role        = makeShared<CXDGSurfaceRole>(RESOURCE);

        m_surfaces.emplace_back(RESOURCE);

        LOGM(LOG, "New xdg_surface at {:x}", (uintptr_t)RESOURCE.get());
    });

    m_resource->setPong([this](CXdgWmBase* r, uint32_t serial) {
        g_pANRManager->onResponse(m_self.lock());
        m_events.pong.emit();
    });
}

bool CXDGWMBase::good() {
    return m_resource->resource();
}

wl_client* CXDGWMBase::client() {
    return m_client;
}

void CXDGWMBase::ping() {
    m_resource->sendPing(1337);
}

CXDGShellProtocol::CXDGShellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    m_grab             = makeShared<CSeatGrab>();
    m_grab->m_keyboard = true;
    m_grab->m_pointer  = true;
    m_grab->setCallback([this]() {
        for (auto const& g : m_grabbed) {
            g->done();
        }
        m_grabbed.clear();
    });
}

void CXDGShellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_wmBases.emplace_back(makeShared<CXDGWMBase>(makeShared<CXdgWmBase>(client, ver, id)));

    if UNLIKELY (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_wmBases.pop_back();
        return;
    }

    RESOURCE->m_self = RESOURCE;

    LOGM(LOG, "New xdg_wm_base at {:x}", (uintptr_t)RESOURCE.get());
}

void CXDGShellProtocol::destroyResource(CXDGWMBase* resource) {
    std::erase_if(m_wmBases, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGPositionerResource* resource) {
    std::erase_if(m_positioners, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGSurfaceResource* resource) {
    std::erase_if(m_surfaces, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGToplevelResource* resource) {
    std::erase_if(m_toplevels, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGPopupResource* resource) {
    std::erase_if(m_popups, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::addOrStartGrab(SP<CXDGPopupResource> popup) {
    if (!m_grabOwner) {
        m_grabOwner = popup;
        m_grabbed.clear();
        m_grab->clear();
        m_grab->add(popup->m_surface->m_surface.lock());
        if (popup->m_parent)
            m_grab->add(popup->m_parent->m_surface.lock());
        g_pSeatManager->setGrab(m_grab);
        m_grabbed.emplace_back(popup);
        return;
    }

    m_grabbed.emplace_back(popup);

    m_grab->add(popup->m_surface->m_surface.lock());

    if (popup->m_parent)
        m_grab->add(popup->m_parent->m_surface.lock());
}

void CXDGShellProtocol::onPopupDestroy(WP<CXDGPopupResource> popup) {
    if (popup == m_grabOwner) {
        g_pSeatManager->setGrab(nullptr);
        for (auto const& g : m_grabbed) {
            g->done();
        }
        m_grabbed.clear();
        return;
    }

    std::erase(m_grabbed, popup);
    if (popup->m_surface)
        m_grab->remove(popup->m_surface->m_surface.lock());
}

CXDGSurfaceRole::CXDGSurfaceRole(SP<CXDGSurfaceResource> xdg) : m_xdgSurface(xdg) {
    ;
}
