#pragma once

#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"

class IHLBuffer;
class CWLSurfaceResource;

class CWLBufferResource {
  public:
    static SP<CWLBufferResource> create(WP<CWlBuffer> resource);
    static SP<CWLBufferResource> fromResource(wl_resource* res);

    bool                         good();
    void                         sendRelease();
    wl_resource*                 getResource();

    WP<IHLBuffer>                m_buffer;

    WP<CWLBufferResource>        m_self;

  private:
    CWLBufferResource(WP<CWlBuffer> resource_);

    SP<CWlBuffer> m_resource;

    friend class IHLBuffer;
};
