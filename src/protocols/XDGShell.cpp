#include "XDGShell.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
#include "core/Compositor.hpp"
#include <cstring>

#define LOGM PROTO::xdgShell->protoLog

CXDGPopupResource::CXDGPopupResource(SP<CXdgPopup> resource_, SP<CXDGSurfaceResource> owner_, SP<CXDGSurfaceResource> surface_, SP<CXDGPositionerResource> positioner) :
    surface(surface_), parent(owner_), resource(resource_), positionerRules(positioner) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CXdgPopup* r) {
        if (surface && surface->mapped)
            surface->events.unmap.emit();
        PROTO::xdgShell->onPopupDestroy(self);
        events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });
    resource->setOnDestroy([this](CXdgPopup* r) {
        if (surface && surface->mapped)
            surface->events.unmap.emit();
        PROTO::xdgShell->onPopupDestroy(self);
        events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });

    resource->setReposition([this](CXdgPopup* r, wl_resource* positionerRes, uint32_t token) {
        LOGM(LOG, "Popup {:x} asks for reposition", (uintptr_t)this);
        lastRepositionToken = token;
        auto pos            = CXDGPositionerResource::fromResource(positionerRes);
        if (!pos)
            return;
        positionerRules = CXDGPositionerRules{pos};
        events.reposition.emit();
    });

    resource->setGrab([this](CXdgPopup* r, wl_resource* seat, uint32_t serial) {
        LOGM(LOG, "xdg_popup {:x} requests grab", (uintptr_t)this);
        PROTO::xdgShell->addOrStartGrab(self.lock());
    });

    if (parent)
        taken = true;
}

CXDGPopupResource::~CXDGPopupResource() {
    PROTO::xdgShell->onPopupDestroy(self);
    events.destroy.emit();
}

void CXDGPopupResource::applyPositioning(const CBox& box, const Vector2D& t1coord) {
    CBox constraint = box.copy().translate(surface->pending.geometry.pos());

    geometry = positionerRules.getPosition(constraint, accumulateParentOffset() + t1coord);

    LOGM(LOG, "Popup {:x} gets unconstrained to {} {}", (uintptr_t)this, geometry.pos(), geometry.size());

    configure(geometry);

    if (lastRepositionToken)
        repositioned();
}

Vector2D CXDGPopupResource::accumulateParentOffset() {
    SP<CXDGSurfaceResource> current = parent.lock();
    Vector2D                off;
    while (current) {
        off += current->current.geometry.pos();
        if (current->popup) {
            off += current->popup->geometry.pos();
            current = current->popup->parent.lock();
        } else
            break;
    }
    return off;
}

SP<CXDGPopupResource> CXDGPopupResource::fromResource(wl_resource* res) {
    auto data = (CXDGPopupResource*)(((CXdgPopup*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CXDGPopupResource::good() {
    return resource->resource();
}

void CXDGPopupResource::configure(const CBox& box) {
    resource->sendConfigure(box.x, box.y, box.w, box.h);
    if (surface)
        surface->scheduleConfigure();
}

void CXDGPopupResource::done() {
    events.dismissed.emit();
    resource->sendPopupDone();
}

void CXDGPopupResource::repositioned() {
    if (!lastRepositionToken)
        return;

    LOGM(LOG, "repositioned: sending reposition token {}", lastRepositionToken);

    resource->sendRepositioned(lastRepositionToken);
    lastRepositionToken = 0;
}

CXDGToplevelResource::CXDGToplevelResource(SP<CXdgToplevel> resource_, SP<CXDGSurfaceResource> owner_) : owner(owner_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CXdgToplevel* r) {
        events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });
    resource->setOnDestroy([this](CXdgToplevel* r) {
        events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });

    if (resource->version() >= 5) {
        wl_array arr;
        wl_array_init(&arr);
        auto p = (uint32_t*)wl_array_add(&arr, sizeof(uint32_t));
        *p     = XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;
        p      = (uint32_t*)wl_array_add(&arr, sizeof(uint32_t));
        *p     = XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;
        resource->sendWmCapabilities(&arr);
        wl_array_release(&arr);
    }

    if (resource->version() >= 2) {
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_LEFT);
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_RIGHT);
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_TOP);
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_TILED_BOTTOM);
    }

    resource->setSetTitle([this](CXdgToplevel* r, const char* t) {
        state.title = t;
        events.metadataChanged.emit();
    });

    resource->setSetAppId([this](CXdgToplevel* r, const char* id) {
        state.appid = id;
        events.metadataChanged.emit();
    });

    resource->setSetMaxSize([this](CXdgToplevel* r, int32_t x, int32_t y) {
        pending.maxSize = {x, y};
        events.sizeLimitsChanged.emit();
    });

    resource->setSetMinSize([this](CXdgToplevel* r, int32_t x, int32_t y) {
        pending.minSize = {x, y};
        events.sizeLimitsChanged.emit();
    });

    resource->setSetMaximized([this](CXdgToplevel* r) {
        state.requestsMaximize = true;
        events.stateChanged.emit();
        state.requestsMaximize.reset();
    });

    resource->setUnsetMaximized([this](CXdgToplevel* r) {
        state.requestsMaximize = false;
        events.stateChanged.emit();
        state.requestsMaximize.reset();
    });

    resource->setSetFullscreen([this](CXdgToplevel* r, wl_resource* output) {
        state.requestsFullscreen = true;
        events.stateChanged.emit();
        state.requestsFullscreen.reset();
    });

    resource->setUnsetFullscreen([this](CXdgToplevel* r) {
        state.requestsFullscreen = false;
        events.stateChanged.emit();
        state.requestsFullscreen.reset();
    });

    resource->setSetMinimized([this](CXdgToplevel* r) {
        state.requestsMinimize = true;
        events.stateChanged.emit();
        state.requestsFullscreen.reset();
    });

    resource->setSetParent([this](CXdgToplevel* r, wl_resource* parentR) {
        auto newp = parentR ? CXDGToplevelResource::fromResource(parentR) : nullptr;
        parent    = newp;

        LOGM(LOG, "Toplevel {:x} sets parent to {:x}", (uintptr_t)this, (uintptr_t)newp.get());
    });
}

CXDGToplevelResource::~CXDGToplevelResource() {
    events.destroy.emit();
}

SP<CXDGToplevelResource> CXDGToplevelResource::fromResource(wl_resource* res) {
    auto data = (CXDGToplevelResource*)(((CXdgToplevel*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CXDGToplevelResource::good() {
    return resource->resource();
}

uint32_t CXDGToplevelResource::setSize(const Vector2D& size) {
    pendingApply.size = size;
    applyState();
    return owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setMaximized(bool maximized) {
    bool set = std::find(pendingApply.states.begin(), pendingApply.states.end(), XDG_TOPLEVEL_STATE_MAXIMIZED) != pendingApply.states.end();

    if (maximized == set)
        return owner->scheduledSerial;

    if (maximized && !set)
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
    else if (!maximized && set)
        std::erase(pendingApply.states, XDG_TOPLEVEL_STATE_MAXIMIZED);
    applyState();
    return owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setFullscreen(bool fullscreen) {
    bool set = std::find(pendingApply.states.begin(), pendingApply.states.end(), XDG_TOPLEVEL_STATE_FULLSCREEN) != pendingApply.states.end();

    if (fullscreen == set)
        return owner->scheduledSerial;

    if (fullscreen && !set)
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
    else if (!fullscreen && set)
        std::erase(pendingApply.states, XDG_TOPLEVEL_STATE_FULLSCREEN);
    applyState();
    return owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setActive(bool active) {
    bool set = std::find(pendingApply.states.begin(), pendingApply.states.end(), XDG_TOPLEVEL_STATE_ACTIVATED) != pendingApply.states.end();

    if (active == set)
        return owner->scheduledSerial;

    if (active && !set)
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    else if (!active && set)
        std::erase(pendingApply.states, XDG_TOPLEVEL_STATE_ACTIVATED);
    applyState();
    return owner->scheduleConfigure();
}

uint32_t CXDGToplevelResource::setSuspeneded(bool sus) {
    if (resource->version() < 6)
        return owner->scheduleConfigure(); // SUSPENDED is since 6

    bool set = std::find(pendingApply.states.begin(), pendingApply.states.end(), XDG_TOPLEVEL_STATE_SUSPENDED) != pendingApply.states.end();

    if (sus == set)
        return owner->scheduledSerial;

    if (sus && !set)
        pendingApply.states.push_back(XDG_TOPLEVEL_STATE_SUSPENDED);
    else if (!sus && set)
        std::erase(pendingApply.states, XDG_TOPLEVEL_STATE_SUSPENDED);
    applyState();
    return owner->scheduleConfigure();
}

void CXDGToplevelResource::applyState() {
    wl_array arr;
    wl_array_init(&arr);
    wl_array_add(&arr, pendingApply.states.size() * sizeof(int));
    memcpy(arr.data, pendingApply.states.data(), pendingApply.states.size() * sizeof(int));

    resource->sendConfigure(pendingApply.size.x, pendingApply.size.y, &arr);

    wl_array_release(&arr);
}

void CXDGToplevelResource::close() {
    resource->sendClose();
}

CXDGSurfaceResource::CXDGSurfaceResource(SP<CXdgSurface> resource_, SP<CXDGWMBase> owner_, SP<CWLSurfaceResource> surface_) :
    owner(owner_), surface(surface_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CXdgSurface* r) {
        if (mapped)
            events.unmap.emit();
        events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });
    resource->setOnDestroy([this](CXdgSurface* r) {
        if (mapped)
            events.unmap.emit();
        events.destroy.emit();
        PROTO::xdgShell->destroyResource(this);
    });

    listeners.surfaceDestroy = surface->events.destroy.registerListener([this](std::any d) {
        LOGM(WARN, "wl_surface destroyed before its xdg_surface role object");
        listeners.surfaceDestroy.reset();
        listeners.surfaceCommit.reset();

        if (mapped)
            events.unmap.emit();

        mapped = false;
        surface.reset();
        events.destroy.emit();
    });

    listeners.surfaceCommit = surface->events.commit.registerListener([this](std::any d) {
        current = pending;
        if (toplevel)
            toplevel->current = toplevel->pending;

        if (initialCommit && surface->pending.buffer) {
            resource->error(-1, "Buffer attached before initial commit");
            return;
        }

        if (surface->current.buffer && !mapped) {
            // this forces apps to not draw CSD.
            if (toplevel)
                toplevel->setMaximized(true);

            mapped = true;
            surface->map();
            events.map.emit();
            return;
        }

        if (!surface->current.buffer && mapped) {
            mapped = false;
            surface->unmap();
            events.unmap.emit();
            return;
        }

        events.commit.emit();
        initialCommit = false;
    });

    resource->setGetToplevel([this](CXdgSurface* r, uint32_t id) {
        const auto RESOURCE = PROTO::xdgShell->m_vToplevels.emplace_back(makeShared<CXDGToplevelResource>(makeShared<CXdgToplevel>(r->client(), r->version(), id), self.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_vToplevels.pop_back();
            return;
        }

        toplevel       = RESOURCE;
        toplevel->self = RESOURCE;

        LOGM(LOG, "xdg_surface {:x} gets a toplevel {:x}", (uintptr_t)owner.get(), (uintptr_t)RESOURCE.get());

        g_pCompositor->m_vWindows.emplace_back(CWindow::create(self.lock()));

        for (auto& p : popups) {
            if (!p)
                continue;
            events.newPopup.emit(p);
        }
    });

    resource->setGetPopup([this](CXdgSurface* r, uint32_t id, wl_resource* parentXDG, wl_resource* positionerRes) {
        auto       parent     = parentXDG ? CXDGSurfaceResource::fromResource(parentXDG) : nullptr;
        auto       positioner = CXDGPositionerResource::fromResource(positionerRes);
        const auto RESOURCE =
            PROTO::xdgShell->m_vPopups.emplace_back(makeShared<CXDGPopupResource>(makeShared<CXdgPopup>(r->client(), r->version(), id), parent, self.lock(), positioner));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_vPopups.pop_back();
            return;
        }

        popup          = RESOURCE;
        RESOURCE->self = RESOURCE;

        LOGM(LOG, "xdg_surface {:x} gets a popup {:x} owner {:x}", (uintptr_t)self.get(), (uintptr_t)RESOURCE.get(), (uintptr_t)parent.get());

        if (!parent)
            return;

        parent->popups.emplace_back(RESOURCE);
        if (parent->mapped)
            parent->events.newPopup.emit(RESOURCE);
    });

    resource->setAckConfigure([this](CXdgSurface* r, uint32_t serial) {
        if (serial < lastConfigureSerial)
            return;
        lastConfigureSerial = serial;
        events.ack.emit(serial);
    });

    resource->setSetWindowGeometry([this](CXdgSurface* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        LOGM(LOG, "xdg_surface {:x} requests geometry {}x{} {}x{}", (uintptr_t)this, x, y, w, h);
        pending.geometry = {x, y, w, h};
    });
}

CXDGSurfaceResource::~CXDGSurfaceResource() {
    events.destroy.emit();
    if (configureSource)
        wl_event_source_remove(configureSource);
    if (surface)
        surface->resetRole();
}

eSurfaceRole CXDGSurfaceResource::role() {
    return SURFACE_ROLE_XDG_SHELL;
}

bool CXDGSurfaceResource::good() {
    return resource->resource();
}

SP<CXDGSurfaceResource> CXDGSurfaceResource::fromResource(wl_resource* res) {
    auto data = (CXDGSurfaceResource*)(((CXdgSurface*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

static void onConfigure(void* data) {
    ((CXDGSurfaceResource*)data)->configure();
}

uint32_t CXDGSurfaceResource::scheduleConfigure() {
    if (configureSource)
        return scheduledSerial;

    configureSource = wl_event_loop_add_idle(g_pCompositor->m_sWLEventLoop, onConfigure, this);
    scheduledSerial = wl_display_next_serial(g_pCompositor->m_sWLDisplay);

    return scheduledSerial;
}

void CXDGSurfaceResource::configure() {
    configureSource = nullptr;
    resource->sendConfigure(scheduledSerial);
}

CXDGPositionerResource::CXDGPositionerResource(SP<CXdgPositioner> resource_, SP<CXDGWMBase> owner_) : owner(owner_), resource(resource_) {
    if (!good())
        return;

    resource->setData(this);

    resource->setDestroy([this](CXdgPositioner* r) { PROTO::xdgShell->destroyResource(this); });
    resource->setOnDestroy([this](CXdgPositioner* r) { PROTO::xdgShell->destroyResource(this); });

    resource->setSetSize([this](CXdgPositioner* r, int32_t x, int32_t y) {
        if (x <= 0 || y <= 0) {
            r->error(XDG_POSITIONER_ERROR_INVALID_INPUT, "Invalid size");
            return;
        }

        state.requestedSize = {x, y};
    });

    resource->setSetAnchorRect([this](CXdgPositioner* r, int32_t x, int32_t y, int32_t w, int32_t h) {
        if (w <= 0 || h <= 0) {
            r->error(XDG_POSITIONER_ERROR_INVALID_INPUT, "Invalid box");
            return;
        }

        state.anchorRect = {x, y, w, h};
    });

    resource->setSetOffset([this](CXdgPositioner* r, int32_t x, int32_t y) { state.offset = {x, y}; });

    resource->setSetAnchor([this](CXdgPositioner* r, xdgPositionerAnchor a) { state.anchor = a; });

    resource->setSetGravity([this](CXdgPositioner* r, xdgPositionerGravity g) { state.gravity = g; });

    resource->setSetConstraintAdjustment([this](CXdgPositioner* r, xdgPositionerConstraintAdjustment a) { state.constraintAdjustment = (uint32_t)a; });

    // TODO: support this shit better. The current impl _works_, but is lacking and could be wrong in a few cases.
    // doesn't matter _that_ much for now, though.
}

SP<CXDGPositionerResource> CXDGPositionerResource::fromResource(wl_resource* res) {
    auto data = (CXDGPositionerResource*)(((CXdgPositioner*)wl_resource_get_user_data(res))->data());
    return data ? data->self.lock() : nullptr;
}

bool CXDGPositionerResource::good() {
    return resource->resource();
}

CXDGPositionerRules::CXDGPositionerRules(SP<CXDGPositionerResource> positioner) {
    state = positioner->state;
}

static Vector2D pointForAnchor(const CBox& box, const Vector2D& predictionSize, xdgPositionerAnchor anchor) {
    switch (anchor) {
        case XDG_POSITIONER_ANCHOR_TOP: return box.pos() + Vector2D{box.size().x / 2.0 - predictionSize.x / 2.0, 0.0};
        case XDG_POSITIONER_ANCHOR_BOTTOM: return box.pos() + Vector2D{box.size().x / 2.0 - predictionSize.x / 2.0, box.size().y};
        case XDG_POSITIONER_ANCHOR_LEFT: return box.pos() + Vector2D{0.0, box.size().y / 2.0 - predictionSize.y / 2.0};
        case XDG_POSITIONER_ANCHOR_RIGHT: return box.pos() + Vector2D{box.size().x, box.size().y / 2.F - predictionSize.y / 2.0};
        case XDG_POSITIONER_ANCHOR_TOP_LEFT: return box.pos();
        case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT: return box.pos() + Vector2D{0.0, box.size().y};
        case XDG_POSITIONER_ANCHOR_TOP_RIGHT: return box.pos() + Vector2D{box.size().x, 0.0};
        case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT: return box.pos() + Vector2D{box.size().x, box.size().y};
        default: return box.pos();
    }

    return {};
}

CBox CXDGPositionerRules::getPosition(const CBox& constraint, const Vector2D& parentCoord) {

    Debug::log(LOG, "GetPosition with constraint {} {} and parent {}", constraint.pos(), constraint.size(), parentCoord);

    CBox predictedBox = {parentCoord + constraint.pos() + pointForAnchor(state.anchorRect, state.requestedSize, state.anchor) + state.offset, state.requestedSize};

    bool success = predictedBox.inside(constraint);

    if (success)
        return predictedBox.translate(-parentCoord - constraint.pos());

    CBox test = predictedBox;

    if (state.constraintAdjustment & (XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y)) {
        // attempt to flip
        const bool flipX      = state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X;
        const bool flipY      = state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y;
        auto       countEdges = [constraint](const CBox& test) -> int {
            int edgeCount = 0;
            edgeCount += test.x < constraint.x ? 1 : 0;
            edgeCount += test.x + test.w > constraint.x + constraint.w ? 1 : 0;
            edgeCount += test.y < constraint.y ? 1 : 0;
            edgeCount += test.y + test.h > constraint.y + constraint.h ? 1 : 0;
            return edgeCount;
        };
        int edgeCount = countEdges(test);

        if (flipX && edgeCount > countEdges(test.copy().translate(Vector2D{-predictedBox.w - state.anchorRect.w, 0.0})))
            test.translate(Vector2D{-predictedBox.w - state.anchorRect.w, 0.0});
        if (flipY && edgeCount > countEdges(test.copy().translate(Vector2D{0.0, -predictedBox.h - state.anchorRect.h})))
            test.translate(Vector2D{0.0, -predictedBox.h - state.anchorRect.h});

        success = test.copy().expand(-1).inside(constraint);

        if (success)
            return test.translate(-parentCoord - constraint.pos());
    }

    // for slide and resize, defines the padding around the edge for the positioned
    // surface.
    constexpr int EDGE_PADDING = 4;

    if (state.constraintAdjustment & (XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y)) {
        // attempt to slide
        const bool slideX = state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X;
        const bool slideY = state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y;

        //const bool gravityLeft = state.gravity == XDG_POSITIONER_GRAVITY_NONE || state.gravity == XDG_POSITIONER_GRAVITY_LEFT || state.gravity == XDG_POSITIONER_GRAVITY_TOP_LEFT || state.gravity == XDG_POSITIONER_GRAVITY_BOTTOM_LEFT;
        //const bool gravityTop = state.gravity == XDG_POSITIONER_GRAVITY_NONE || state.gravity == XDG_POSITIONER_GRAVITY_TOP || state.gravity == XDG_POSITIONER_GRAVITY_TOP_LEFT || state.gravity == XDG_POSITIONER_GRAVITY_TOP_RIGHT;

        const bool leftEdgeOut   = test.x < constraint.x;
        const bool topEdgeOut    = test.y < constraint.y;
        const bool rightEdgeOut  = test.x + test.w > constraint.x + constraint.w;
        const bool bottomEdgeOut = test.y + test.h > constraint.y + constraint.h;

        // TODO: this isn't truly conformant.
        if (leftEdgeOut && slideX)
            test.x = constraint.x + EDGE_PADDING;
        if (rightEdgeOut && slideX)
            test.x = std::clamp((double)(constraint.x + constraint.w - test.w), (double)(constraint.x + EDGE_PADDING), (double)INFINITY);
        if (topEdgeOut && slideY)
            test.y = constraint.y + EDGE_PADDING;
        if (bottomEdgeOut && slideY)
            test.y = std::clamp((double)(constraint.y + constraint.h - test.h), (double)(constraint.y + EDGE_PADDING), (double)INFINITY);

        success = test.copy().expand(-1).inside(constraint);

        if (success)
            return test.translate(-parentCoord - constraint.pos());
    }

    if (state.constraintAdjustment & (XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y)) {
        const bool resizeX = state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X;
        const bool resizeY = state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y;

        const bool leftEdgeOut   = test.x < constraint.x;
        const bool topEdgeOut    = test.y < constraint.y;
        const bool rightEdgeOut  = test.x + test.w > constraint.x + constraint.w;
        const bool bottomEdgeOut = test.y + test.h > constraint.y + constraint.h;

        // TODO: this isn't truly conformant.
        if (leftEdgeOut && resizeX) {
            test.w = test.x + test.w - constraint.x - EDGE_PADDING;
            test.x = constraint.x + EDGE_PADDING;
        }
        if (rightEdgeOut && resizeX)
            test.w = constraint.w - (test.x - constraint.w) - EDGE_PADDING;
        if (topEdgeOut && resizeY) {
            test.h = test.y + test.h - constraint.y - EDGE_PADDING;
            test.y = constraint.y + EDGE_PADDING;
        }
        if (bottomEdgeOut && resizeY)
            test.h = constraint.h - (test.y - constraint.y) - EDGE_PADDING;

        success = test.copy().expand(-1).inside(constraint);

        if (success)
            return test.translate(-parentCoord - constraint.pos());
    }

    LOGM(WARN, "Compositor/client bug: xdg_positioner couldn't find a place");

    return test.translate(-parentCoord - constraint.pos());
}

CXDGWMBase::CXDGWMBase(SP<CXdgWmBase> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CXdgWmBase* r) { PROTO::xdgShell->destroyResource(this); });
    resource->setOnDestroy([this](CXdgWmBase* r) { PROTO::xdgShell->destroyResource(this); });

    pClient = resource->client();

    resource->setCreatePositioner([this](CXdgWmBase* r, uint32_t id) {
        const auto RESOURCE =
            PROTO::xdgShell->m_vPositioners.emplace_back(makeShared<CXDGPositionerResource>(makeShared<CXdgPositioner>(r->client(), r->version(), id), self.lock()));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_vPositioners.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;

        positioners.emplace_back(RESOURCE);

        LOGM(LOG, "New xdg_positioner at {:x}", (uintptr_t)RESOURCE.get());
    });

    resource->setGetXdgSurface([this](CXdgWmBase* r, uint32_t id, wl_resource* surf) {
        auto SURF = CWLSurfaceResource::fromResource(surf);

        if (!SURF) {
            r->error(-1, "Invalid surface passed");
            return;
        }

        if (SURF->role->role() != SURFACE_ROLE_UNASSIGNED) {
            r->error(-1, "Surface already has a different role");
            return;
        }

        const auto RESOURCE = PROTO::xdgShell->m_vSurfaces.emplace_back(makeShared<CXDGSurfaceResource>(makeShared<CXdgSurface>(r->client(), r->version(), id), self.lock(), SURF));

        if (!RESOURCE->good()) {
            r->noMemory();
            PROTO::xdgShell->m_vSurfaces.pop_back();
            return;
        }

        RESOURCE->self = RESOURCE;
        SURF->role     = RESOURCE;

        surfaces.emplace_back(RESOURCE);

        LOGM(LOG, "New xdg_surface at {:x}", (uintptr_t)RESOURCE.get());
    });
}

bool CXDGWMBase::good() {
    return resource->resource();
}

wl_client* CXDGWMBase::client() {
    return pClient;
}

CXDGShellProtocol::CXDGShellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    grab           = makeShared<CSeatGrab>();
    grab->keyboard = true;
    grab->pointer  = true;
    grab->setCallback([this]() {
        for (auto& g : grabbed) {
            g->done();
        }
        grabbed.clear();
    });
}

void CXDGShellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vWMBases.emplace_back(makeShared<CXDGWMBase>(makeShared<CXdgWmBase>(client, ver, id)));

    if (!RESOURCE->good()) {
        wl_client_post_no_memory(client);
        m_vWMBases.pop_back();
        return;
    }

    RESOURCE->self = RESOURCE;

    LOGM(LOG, "New xdg_wm_base at {:x}", (uintptr_t)RESOURCE.get());
}

void CXDGShellProtocol::destroyResource(CXDGWMBase* resource) {
    std::erase_if(m_vWMBases, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGPositionerResource* resource) {
    std::erase_if(m_vPositioners, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGSurfaceResource* resource) {
    std::erase_if(m_vSurfaces, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGToplevelResource* resource) {
    std::erase_if(m_vToplevels, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::destroyResource(CXDGPopupResource* resource) {
    std::erase_if(m_vPopups, [&](const auto& other) { return other.get() == resource; });
}

void CXDGShellProtocol::addOrStartGrab(SP<CXDGPopupResource> popup) {
    if (!grabOwner) {
        grabOwner = popup;
        grabbed.clear();
        grab->clear();
        grab->add(popup->surface->surface.lock());
        if (popup->parent)
            grab->add(popup->parent->surface.lock());
        g_pSeatManager->setGrab(grab);
        grabbed.emplace_back(popup);
        return;
    }

    grabbed.emplace_back(popup);

    grab->add(popup->surface->surface.lock());

    if (popup->parent)
        grab->add(popup->parent->surface.lock());
}

void CXDGShellProtocol::onPopupDestroy(WP<CXDGPopupResource> popup) {
    if (popup == grabOwner) {
        g_pSeatManager->setGrab(nullptr);
        for (auto& g : grabbed) {
            g->done();
        }
        grabbed.clear();
        return;
    }

    std::erase(grabbed, popup);
    if (popup->surface)
        grab->remove(popup->surface->surface.lock());
}
