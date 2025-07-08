#pragma once

#include <vector>
#include <cstdint>
#include <tuple>
#include "WaylandProtocol.hpp"
#include "wlr-layer-shell-unstable-v1.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/signal/Signal.hpp"
#include "types/SurfaceRole.hpp"

class CMonitor;
class CXDGPopupResource;
class CWLSurfaceResource;
class CLayerShellResource;

class CLayerShellRole : public ISurfaceRole {
  public:
    CLayerShellRole(SP<CLayerShellResource> ls);

    virtual eSurfaceRole role() {
        return SURFACE_ROLE_LAYER_SHELL;
    }

    WP<CLayerShellResource> m_layerSurface;
};

class CLayerShellResource {
  public:
    CLayerShellResource(SP<CZwlrLayerSurfaceV1> resource_, SP<CWLSurfaceResource> surf_, std::string namespace_, PHLMONITOR pMonitor, zwlrLayerShellV1Layer layer);
    ~CLayerShellResource();

    bool good();
    void configure(const Vector2D& size);
    void sendClosed();

    enum eCommittedState : uint8_t {
        STATE_SIZE          = (1 << 0),
        STATE_ANCHOR        = (1 << 1),
        STATE_EXCLUSIVE     = (1 << 2),
        STATE_MARGIN        = (1 << 3),
        STATE_INTERACTIVITY = (1 << 4),
        STATE_LAYER         = (1 << 5),
        STATE_EDGE          = (1 << 6),
    };

    struct {
        CSignalT<>                      destroy;
        CSignalT<>                      commit;
        CSignalT<>                      map;
        CSignalT<>                      unmap;
        CSignalT<SP<CXDGPopupResource>> newPopup;
    } m_events;

    struct SState {
        uint32_t                                anchor    = 0;
        int32_t                                 exclusive = 0;
        Vector2D                                desiredSize;
        zwlrLayerSurfaceV1KeyboardInteractivity interactivity = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
        zwlrLayerShellV1Layer                   layer         = ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM;
        zwlrLayerSurfaceV1Anchor                exclusiveEdge = (zwlrLayerSurfaceV1Anchor)0;
        uint32_t                                committed     = 0;

        struct {
            double left = 0, right = 0, top = 0, bottom = 0;
        } margin;

        void reset();
    } m_current, m_pending;

    Vector2D               m_size;
    std::string            m_layerNamespace;
    std::string            m_monitor = "";
    WP<CWLSurfaceResource> m_surface;
    bool                   m_mapped     = false;
    bool                   m_configured = false;

  private:
    SP<CZwlrLayerSurfaceV1> m_resource;

    struct {
        CHyprSignalListener commitSurface;
        CHyprSignalListener destroySurface;
        CHyprSignalListener unmapSurface;
    } m_listeners;

    bool                                       m_closed = false;

    std::vector<std::pair<uint32_t, Vector2D>> m_serials;
};

class CLayerShellProtocol : public IWaylandProtocol {
  public:
    CLayerShellProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CLayerShellResource* surf);
    void onGetLayerSurface(CZwlrLayerShellV1* pMgr, uint32_t id, wl_resource* surface, wl_resource* output, zwlrLayerShellV1Layer layer, std::string namespace_);

    //
    std::vector<UP<CZwlrLayerShellV1>>   m_managers;
    std::vector<SP<CLayerShellResource>> m_layers;

    friend class CLayerShellResource;
};

namespace PROTO {
    inline UP<CLayerShellProtocol> layerShell;
};
