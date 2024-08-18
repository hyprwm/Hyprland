#include "XDGShell.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "../managers/SeatManager.hpp"
#include "core/Seat.hpp"
#include "core/Compositor.hpp"
#include <cstring>

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

        if (initialCommit && surface->pending.texture) {
            resource->error(-1, "Buffer attached before initial commit");
            return;
        }

        if (surface->current.texture && !mapped) {
            // this forces apps to not draw CSD.
            if (toplevel)
                toplevel->setMaximized(true);

            mapped = true;
            surface->map();
            events.map.emit();
            return;
        }

        if (!surface->current.texture && mapped) {
            mapped = false;
            events.unmap.emit();
            surface->unmap();
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

    resource->setSetAnchor([this](CXdgPositioner* r, xdgPositionerAnchor a) { state.setAnchor(a); });

    resource->setSetGravity([this](CXdgPositioner* r, xdgPositionerGravity g) { state.setGravity(g); });

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

CBox CXDGPositionerRules::getPosition(CBox constraint, const Vector2D& parentCoord) {
    Debug::log(LOG, "GetPosition with constraint {} {} and parent {}", constraint.pos(), constraint.size(), parentCoord);

    // padding
    constraint.expand(-4);

    auto anchorRect = state.anchorRect.copy().translate(parentCoord);

    auto width   = state.requestedSize.x;
    auto height  = state.requestedSize.y;
    auto gravity = state.gravity;

    auto anchorX = state.anchor.left() ? anchorRect.x : state.anchor.right() ? anchorRect.extent().x : anchorRect.middle().x;
    auto anchorY = state.anchor.top() ? anchorRect.y : state.anchor.bottom() ? anchorRect.extent().y : anchorRect.middle().y;

    auto calcEffectiveX = [&](CEdges anchorGravity, double anchorX) { return anchorGravity.left() ? anchorX - width : anchorGravity.right() ? anchorX : anchorX - width / 2; };
    auto calcEffectiveY = [&](CEdges anchorGravity, double anchorY) { return anchorGravity.top() ? anchorY - height : anchorGravity.bottom() ? anchorY : anchorY - height / 2; };

    auto calcRemainingWidth = [&](double effectiveX) {
        auto width = state.requestedSize.x;
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
        auto height = state.requestedSize.y;
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

    if (state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X) {
        auto flip = (gravity.left() && effectiveX + state.offset.x < constraint.x) || (gravity.right() && effectiveX + state.offset.x + width > constraint.extent().x);

        if (flip) {
            auto newGravity    = gravity ^ (CEdges::LEFT | CEdges::RIGHT);
            auto newAnchorX    = state.anchor.left() ? anchorRect.extent().x : state.anchor.right() ? anchorRect.x : anchorX;
            auto newEffectiveX = calcEffectiveX(newGravity, newAnchorX);

            if (calcRemainingWidth(newEffectiveX).second > calcRemainingWidth(effectiveX).second) {
                gravity    = newGravity;
                anchorX    = newAnchorX;
                effectiveX = newEffectiveX;
            }
        }
    }

    if (state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y) {
        auto flip = (state.gravity.top() && effectiveY + state.offset.y < constraint.y) || (state.gravity.bottom() && effectiveY + state.offset.y + height > constraint.extent().y);

        if (flip) {
            auto newGravity    = gravity ^ (CEdges::TOP | CEdges::BOTTOM);
            auto newAnchorY    = state.anchor.top() ? anchorRect.extent().y : state.anchor.bottom() ? anchorRect.y : anchorY;
            auto newEffectiveY = calcEffectiveY(newGravity, newAnchorY);

            if (calcRemainingHeight(newEffectiveY).second > calcRemainingHeight(effectiveY).second) {
                gravity    = newGravity;
                anchorY    = newAnchorY;
                effectiveY = newEffectiveY;
            }
        }
    }

    effectiveX += state.offset.x;
    effectiveY += state.offset.y;

    // Slide order is important for the case where the window is too large to fit on screen.

    if (state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X) {
        if (effectiveX + width > constraint.extent().x)
            effectiveX = constraint.extent().x - width;

        if (effectiveX < constraint.x)
            effectiveX = constraint.x;
    }

    if (state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y) {
        if (effectiveY + height > constraint.extent().y)
            effectiveY = constraint.extent().y - height;

        if (effectiveY < constraint.y)
            effectiveY = constraint.y;
    }

    if (state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_X) {
        auto [newX, newWidth] = calcRemainingWidth(effectiveX);
        effectiveX            = newX;
        width                 = newWidth;
    }

    if (state.constraintAdjustment & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_RESIZE_Y) {
        auto [newY, newHeight] = calcRemainingHeight(effectiveY);
        effectiveY             = newY;
        height                 = newHeight;
    }

    return {effectiveX - parentCoord.x, effectiveY - parentCoord.y, width, height};
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

        RESOURCE->self    = RESOURCE;
        RESOURCE->surface = SURF;
        SURF->role        = makeShared<CXDGSurfaceRole>(RESOURCE);

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

CXDGSurfaceRole::CXDGSurfaceRole(SP<CXDGSurfaceResource> xdg) : xdgSurface(xdg) {
    ;
}
