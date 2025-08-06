#include "LayerShell.hpp"
#include "../Compositor.hpp"
#include "../desktop/LayerSurface.hpp"
#include "XDGShell.hpp"
#include "core/Compositor.hpp"
#include "core/Output.hpp"
#include "../helpers/Monitor.hpp"

void CLayerShellResource::SState::reset() {
    anchor        = 0;
    exclusive     = 0;
    interactivity = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
    layer         = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
    exclusiveEdge = static_cast<zwlrLayerSurfaceV1Anchor>(0);
    desiredSize   = {};
    margin        = {0, 0, 0, 0};
}

CLayerShellResource::CLayerShellResource(SP<CZwlrLayerSurfaceV1> resource_, SP<CWLSurfaceResource> surf_, std::string namespace_, PHLMONITOR pMonitor,
                                         zwlrLayerShellV1Layer layer) : m_layerNamespace(namespace_), m_surface(surf_), m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_current.layer = layer;
    m_monitor       = pMonitor ? pMonitor->m_name : "";

    m_resource->setDestroy([this](CZwlrLayerSurfaceV1* r) {
        m_events.destroy.emit();
        PROTO::layerShell->destroyResource(this);
    });
    m_resource->setOnDestroy([this](CZwlrLayerSurfaceV1* r) {
        m_events.destroy.emit();
        PROTO::layerShell->destroyResource(this);
    });

    m_listeners.destroySurface = surf_->m_events.destroy.listen([this] {
        m_events.destroy.emit();
        PROTO::layerShell->destroyResource(this);
    });

    m_listeners.unmapSurface = surf_->m_events.unmap.listen([this] { m_events.unmap.emit(); });

    m_listeners.commitSurface = surf_->m_events.commit.listen([this] {
        m_current           = m_pending;
        m_pending.committed = 0;

        bool attachedBuffer = m_surface->m_current.texture;

        if (attachedBuffer && !m_configured) {
            m_surface->error(-1, "layerSurface was not configured, but a buffer was attached");
            return;
        }

        constexpr uint32_t horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        constexpr uint32_t vert  = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;

        if (m_current.desiredSize.x <= 0 && (m_current.anchor & horiz) != horiz) {
            m_surface->error(-1, "x == 0 but anchor doesn't have left and right");
            return;
        }

        if (m_current.desiredSize.y <= 0 && (m_current.anchor & vert) != vert) {
            m_surface->error(-1, "y == 0 but anchor doesn't have top and bottom");
            return;
        }

        if (attachedBuffer && !m_mapped) {
            m_mapped = true;
            m_surface->map();
            m_events.map.emit();
            return;
        }

        if (!attachedBuffer && m_mapped) {
            m_mapped = false;
            m_events.unmap.emit();
            m_surface->unmap();
            m_configured = false;
            return;
        }

        m_events.commit.emit();
    });

    m_resource->setSetSize([this](CZwlrLayerSurfaceV1* r, uint32_t x, uint32_t y) {
        m_pending.committed |= STATE_SIZE;
        m_pending.desiredSize = {static_cast<int>(x), static_cast<int>(y)};
    });

    m_resource->setSetAnchor([this](CZwlrLayerSurfaceV1* r, zwlrLayerSurfaceV1Anchor anchor) {
        if (anchor > (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_ANCHOR, "Invalid anchor");
            return;
        }

        m_pending.committed |= STATE_ANCHOR;
        m_pending.anchor = anchor;
    });

    m_resource->setSetExclusiveZone([this](CZwlrLayerSurfaceV1* r, int32_t zone) {
        m_pending.committed |= STATE_EXCLUSIVE;
        m_pending.exclusive = zone;
    });

    m_resource->setSetMargin([this](CZwlrLayerSurfaceV1* r, int32_t top, int32_t right, int32_t bottom, int32_t left) {
        m_pending.committed |= STATE_MARGIN;
        m_pending.margin = {left, right, top, bottom};
    });

    m_resource->setSetKeyboardInteractivity([this](CZwlrLayerSurfaceV1* r, zwlrLayerSurfaceV1KeyboardInteractivity kbi) {
        if (kbi > ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_KEYBOARD_INTERACTIVITY, "Invalid keyboard interactivity");
            return;
        }

        m_pending.committed |= STATE_INTERACTIVITY;
        m_pending.interactivity = kbi;
    });

    m_resource->setGetPopup([this](CZwlrLayerSurfaceV1* r, wl_resource* popup_) {
        auto popup = CXDGPopupResource::fromResource(popup_);

        if (popup->m_taken) {
            r->error(-1, "Parent already exists!");
            return;
        }

        popup->m_taken = true;
        m_events.newPopup.emit(popup);
    });

    m_resource->setAckConfigure([this](CZwlrLayerSurfaceV1* r, uint32_t serial) {
        auto serialFound = std::ranges::find_if(m_serials, [serial](const auto& e) { return e.first == serial; });

        if (serialFound == m_serials.end()) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_SURFACE_STATE, "Serial invalid in ack_configure");
            return;
        }

        m_configured = true;
        m_size       = serialFound->second;

        m_serials.erase(serialFound);
    });

    m_resource->setSetLayer([this](CZwlrLayerSurfaceV1* r, uint32_t layer) {
        if (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
            r->error(ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER, "Invalid layer");
            return;
        }

        m_pending.committed |= STATE_LAYER;
        m_pending.layer = static_cast<zwlrLayerShellV1Layer>(layer);
    });

    m_resource->setSetExclusiveEdge([this](CZwlrLayerSurfaceV1* r, zwlrLayerSurfaceV1Anchor anchor) {
        if (anchor > (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_EXCLUSIVE_EDGE, "Invalid exclusive edge");
            return;
        }

        if (anchor && (!m_pending.anchor || !(m_pending.anchor & anchor))) {
            r->error(ZWLR_LAYER_SURFACE_V1_ERROR_INVALID_EXCLUSIVE_EDGE, "Exclusive edge doesn't align with anchor");
            return;
        }

        m_pending.committed |= STATE_EDGE;
        m_pending.exclusiveEdge = anchor;
    });
}

CLayerShellResource::~CLayerShellResource() {
    m_events.destroy.emit();
    if (m_surface)
        m_surface->resetRole();
}

bool CLayerShellResource::good() {
    return m_resource->resource();
}

void CLayerShellResource::sendClosed() {
    if (m_closed)
        return;
    m_closed = true;
    m_resource->sendClosed();
}

void CLayerShellResource::configure(const Vector2D& size_) {
    m_size = size_;

    auto serial = wl_display_next_serial(g_pCompositor->m_wlDisplay);

    m_serials.push_back({serial, size_});

    m_resource->sendConfigure(serial, size_.x, size_.y);
}

CLayerShellProtocol::CLayerShellProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CLayerShellProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeUnique<CZwlrLayerShellV1>(client, ver, id)).get();
    RESOURCE->setOnDestroy([this](CZwlrLayerShellV1* p) { this->onManagerResourceDestroy(p->resource()); });

    RESOURCE->setDestroy([this](CZwlrLayerShellV1* pMgr) { this->onManagerResourceDestroy(pMgr->resource()); });
    RESOURCE->setGetLayerSurface([this](CZwlrLayerShellV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* output, zwlrLayerShellV1Layer layer, std::string namespace_) {
        this->onGetLayerSurface(pMgr, id, surface, output, layer, namespace_);
    });
}

void CLayerShellProtocol::onManagerResourceDestroy(wl_resource* res) {
    std::erase_if(m_managers, [&](const auto& other) { return other->resource() == res; });
}

void CLayerShellProtocol::destroyResource(CLayerShellResource* surf) {
    std::erase_if(m_layers, [&](const auto& other) { return other.get() == surf; });
}

void CLayerShellProtocol::onGetLayerSurface(CZwlrLayerShellV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* output, zwlrLayerShellV1Layer layer, std::string namespace_) {
    const auto CLIENT   = pMgr->client();
    const auto PMONITOR = output ? CWLOutputResource::fromResource(output)->m_monitor.lock() : nullptr;
    auto       SURF     = CWLSurfaceResource::fromResource(surface);

    if UNLIKELY (!SURF) {
        pMgr->error(-1, "Invalid surface");
        return;
    }

    if UNLIKELY (SURF->m_role->role() != SURFACE_ROLE_UNASSIGNED) {
        pMgr->error(-1, "Surface already has a different role");
        return;
    }

    if UNLIKELY (layer > ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
        pMgr->error(ZWLR_LAYER_SHELL_V1_ERROR_INVALID_LAYER, "Invalid layer");
        return;
    }

    const auto RESOURCE = m_layers.emplace_back(makeShared<CLayerShellResource>(makeShared<CZwlrLayerSurfaceV1>(CLIENT, pMgr->version(), id), SURF, namespace_, PMONITOR, layer));

    if UNLIKELY (!RESOURCE->good()) {
        pMgr->noMemory();
        m_layers.pop_back();
        return;
    }

    SURF->m_role = makeShared<CLayerShellRole>(RESOURCE);
    g_pCompositor->m_layers.emplace_back(CLayerSurface::create(RESOURCE));

    LOGM(LOG, "New wlr_layer_surface {:x}", (uintptr_t)RESOURCE.get());
}

CLayerShellRole::CLayerShellRole(SP<CLayerShellResource> ls) : m_layerSurface(ls) {
    ;
}
