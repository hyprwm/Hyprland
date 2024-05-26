#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "wayland.hpp"
#include "../../helpers/signal/Signal.hpp"

class IWLBuffer;

class CWLBufferResource {
  public:
    static SP<CWLBufferResource> create(SP<CWlBuffer> resource);
    static SP<CWLBufferResource> fromResource(wl_resource* res);

    bool                         good();
    void                         sendRelease();
    wl_resource*                 getResource();

    WP<IWLBuffer>                buffer;

    WP<CWLBufferResource>        self;

  private:
    CWLBufferResource(SP<CWlBuffer> resource_);

    SP<CWlBuffer> resource;

    friend class IWLBuffer;
};
