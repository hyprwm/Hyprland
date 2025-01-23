#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "wayland-drm.hpp"
#include "types/Buffer.hpp"
#include "types/DMABuffer.hpp"

class CMesaDRMBufferResource {
  public:
    CMesaDRMBufferResource(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs attrs);
    ~CMesaDRMBufferResource();

    bool good();

  private:
    SP<CDMABuffer> buffer;

    struct {
        CHyprSignalListener bufferResourceDestroy;
    } listeners;

    friend class CMesaDRMResource;
};

class CMesaDRMResource {
  public:
    CMesaDRMResource(SP<CWlDrm> resource_);

    bool good();

  private:
    SP<CWlDrm> resource;
};

class CMesaDRMProtocol : public IWaylandProtocol {
  public:
    CMesaDRMProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CMesaDRMResource* resource);
    void destroyResource(CMesaDRMBufferResource* resource);

    //
    std::vector<SP<CMesaDRMResource>>       m_vManagers;
    std::vector<SP<CMesaDRMBufferResource>> m_vBuffers;

    std::string                             nodeName = "";

    friend class CMesaDRMResource;
    friend class CMesaDRMBufferResource;
};

namespace PROTO {
    inline UP<CMesaDRMProtocol> mesaDRM;
};
