#include "LayerShell.hpp"
#include "../Compositor.hpp"
#include "XDGShell.hpp"
#include "core/Compositor.hpp"
#include "core/Output.hpp"

void CLayerShellResource::SState::reset() {
    anchor        = 0;
    exclusive     = 0;
    interactivity = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
    layer         = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
    exclusiveEdge = (zwlrLayerSurfaceV1Anchor)0;
    desiredSize   = {};
    margin        = {0, 0, 0, 0};
}

CLayerShellResource::CLayerShellResource(SP<CZwlrLayerSurfaceV1> resource_, SP<CWLSurfaceResource> surf_, std::string namespace_, CMonitor* pMonitor, zwlrLayerShellV1Layer layer) :
    layerNamespace(namespace_), surface(surf_), resource(resource_) {
    if (!good())
        return;

    current.layer = layer;
    monitor       = pMonitor ? pMonitor->szName : "";

    resource->setDestroy([this](CZwlrLayerSurfaceV1* r) {
        events.destroy.emit();
        PROTO::layerShell->destroyResource(this);
    });
    resource->setOnDestroy([this](CZwlrLayerSurfaceV1* r) {
        events.destroy.emit();
        PROTO::layerShell->destroyResource(this);
    });

    listeners.destroySurface = surf_->events.destroy.registerListener([this](std::any d) {
        events.destroy.emit();
        PROTO::layerShell->destroyResource(this);
    });

    listeners.unmapSurface = surf_->events.unmap.registerListener([this](std::any d) { events.unmap.emit(); });

    listeners.commitSurface = surf_->events.commit.registerListener([this](std::any d) {
        current           = pending;
        pending.committed = 0;

        bool attachedBuffer = surface->current.texture;

        if (attachedBuffer && !configured) {
            surface->error(-1, "layerSurface was not configured, but a buffer was attached");
            return;
        }

        constexpr uint32_t horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        constexpr uint32_t vert  = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

        if (current.desiredSize.x <= 0 && (current.anchor & horiz) != horiz) {
            surface->error(-1, "x == 0 but anchor doesn't have left and right");
            return;
        }

        if (current.desiredSize.y <= 0 && (current.anchor & vert) != vert) {
            surface->error(-1, "y == 0 but anchor doesn't have top and bottom");
            return;
        }

        if (attachedBuffer && !mapped) {
            mapped = true;
            surface->map();
            events.map.emit();
            return;
        }

        if (!attachedBuffer && mapped) {
            mapped = false;
            events.unmap.emit();
            surface->unmap();
            configured = false;
            return;
        }

        events.commit.emit();
    });

    resource->setSetSize([this](CZwlrLayerSurfaceV1* r, uint32_t x, uint32_t y) {
        pending.committed |= STATE_SIZE;
        pending.desiredSize = {(int)x, (int)y};
    });

    resource->setSetAnchor([this](CZwlrLayerSurfaceV1* r, zwlrLayerSurfaceV1Anchor anchor) {
        if (anchor > (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_ANCHOR, "Invalid anchor");
            return;
        }

        pending.committed |= STATE_ANCHOR;
        pending.anchor = anchor;
    });

    resource->setSetExclusiveZone([this](CZwlrLayerSurfaceV1* r, int32_t zone) {
        pending.committed |= STATE_EXCLUSIVE;
        pending.exclusive = zone;
    });

    resource->setSetMargin([this](CZwlrLayerSurfaceV1* r, int32_t top, int32_t right, int32_t bottom, int32_t left) {
        pending.committed |= STATE_MARGIN;
        pending.margin = {left, right, top, bottom};
    });

    resource->setSetKeyboardInteractivity([this](CZwlrLayerSurfaceV1* r, zwlrLayerSurfaceV1KeyboardInteractivity kbi) {
        if (kbi > ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_KEYBOARD_INTERACTIVITY, "Invalid keyboard interactivity");
            return;
        }

        pending.committed |= STATE_INTERACTIVITY;
        pending.interactivity = kbi;
    });

    resource->setGetPopup([this](CZwlrLayerSurfaceV1* r, wl_resource* popup_) {
        auto popup = CXDGPopupResource::fromResource(popup_);

        if (popup->taken) {
            r->error(-1, "Parent already exists!");
            return;
        }

        popup->taken = true;
        events.newPopup.emit(popup);
    });

    resource->setAckConfigure([this](CZwlrLayerSurfaceV1* r, uint32_t serial) {
        auto serialFound = std::find_if(serials.begin(), serials.end(), [serial](const auto& e) { return e.first == serial; });

        if (serialFound == serials.end()) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE, "Serial invalid in ack_configure");
            return;
        }

        configured = true;
        size       = serialFound->second;

        serials.erase(serialFound);
    });

    resource->setSetLayer([this](CZwlrLayerSurfaceV1* r, uint32_t layer) {
        if (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
            r->error(ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER, "Invalid layer");
            return;
        }

        pending.committed |= STATE_LAYER;
        pending.layer = (zwlrLayerShellV1Layer)layer;
    });

    resource->setSetExclusiveEdge([this](CZwlrLayerSurfaceV1* r, zwlrLayerSurfaceV1Anchor anchor) {
        if (anchor > (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_EXCLUSIVE_EDGE, "Invalid exclusive edge");
            return;
        }

        if (anchor && (!pending.anchor || !(pending.anchor & anchor))) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_EXCLUSIVE_EDGE, "Exclusive edge doesn't align with anchor");
            return;
        }

        pending.committed |= STATE_EDGE;
        pending.exclusiveEdge = anchor;
    });
}

CLayerShellResource::~CLayerShellResource() {
    events.destroy.emit();
    if (surface)
        surface->resetRole();
}

bool CLayerShellResource::good() {
    return resource->resource();
}

void CLayerShellResource::sendClosed() {
    if (closed)
        return;
    closed = true;
    resource->sendClosed();
}

void CLayerShellResource::configure(const Vector2D& size_) {
    size = size_;

    auto serial = wl_display_next_serial(g_pCompositor->m_sWLDisplay);

    serials.push_back({serial, size_});

    resource->sendConfigure(serial, size_.x, size_.y);
}

CLayerShellProtocol::CLayerShellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CLayerShellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_vManagers.emplace_back(std::make_unique<CZwlrLayerShellV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrLayerShellV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwlrLayerShellV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetLayerSurface([this](CZwlrLayerShellV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* output, zwlrLayerShellV1Layer layer, std::string namespace_) {
        this->onGetLayerSurface(pMgr, id, surface, output, layer, namespace_);
    });
}

void CLayerShellProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_vManagers, [&](const auto& other) { return other->resource() == res; });
}

void CLayerShellProtocol::destroyResource(CLayerShellResource* surf) {
    std::erase_if(m_vLayers, [&](const auto& other) { return other.get() == surf; });
}

void CLayerShellProtocol::onGetLayerSurface(CZwlrLayerShellV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* output, zwlrLayerShellV1Layer layer, std::string namespace_) {
    const auto CLIENT   = pMgr->client();
    const auto PMONITOR = output ? CWLOutputResource::fromResource(output)->monitor.get() : nullptr;
    auto       SURF     = CWLSurfaceResource::fromResource(surface);

    if (!SURF) {
        pMgr->error(-1, "Invalid surface");
        return;
    }

    if (SURF->role->role() != SURFACE_ROLE_UNASSIGNED) {
        pMgr->error(-1, "Surface already has a different role");
        return;
    }

    const auto RESOURCE = m_vLayers.emplace_back(makeShared<CLayerShellResource>(makeShared<CZwlrLayerSurfaceV1>(CLIENT, pMgr->version(), id), SURF, namespace_, PMONITOR, layer));

    if (!RESOURCE->good()) {
        pMgr->noMemory();
        m_vLayers.pop_back();
        return;
    }

    SURF->role = makeShared<CLayerShellRole>(RESOURCE);
    g_pCompositor->m_vLayers.emplace_back(CLayerSurface::create(RESOURCE));

    LOGM(LOG, "New wlr_layer_surface {:x}", (uintptr_t)RESOURCE.get());
}

CLayerShellRole::CLayerShellRole(SP<CLayerShellResource> ls) : layerSurface(ls) {
    ;
}
