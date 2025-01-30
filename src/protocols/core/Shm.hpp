#pragma once

/*
    Implementations for:
     - wl_shm
     - wl_shm_pool
     - wl_buffer with shm
*/

#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "wayland.hpp"
#include "../types/Buffer.hpp"
#include "../../helpers/math/Math.hpp"

class CWLSHMPoolResource;

class CSHMPool {
  public:
    CSHMPool(int fd, size_t size);
    ~CSHMPool();

    int    fd   = 0;
    size_t size = 0;
    void*  data = nullptr;

    void   resize(size_t size);
};

class CWLSHMBuffer : public IHLBuffer {
  public:
    CWLSHMBuffer(SP<CWLSHMPoolResource> pool, uint32_t id, int32_t offset, const Vector2D& size, int32_t stride, uint32_t fmt);
    virtual ~CWLSHMBuffer() = default;

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual void                                   update(const CRegion& damage);
    virtual bool                                   isSynchronous();
    virtual Aquamarine::SSHMAttrs                  shm();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();

    bool                                           good();

    int32_t                                        offset = 0, stride = 0;
    uint32_t                                       fmt = 0;
    SP<CSHMPool>                                   pool;

  private:
    bool success = false;

    struct {
        CHyprSignalListener bufferResourceDestroy;
    } listeners;
};

class CWLSHMPoolResource {
  public:
    CWLSHMPoolResource(SP<CWlShmPool> resource_, int fd, size_t size);

    bool                   good();

    SP<CSHMPool>           pool;

    WP<CWLSHMPoolResource> self;

  private:
    SP<CWlShmPool> resource;

    friend class CWLSHMBuffer;
};

class CWLSHMResource {
  public:
    CWLSHMResource(SP<CWlShm> resource_);

    bool good();

  private:
    SP<CWlShm> resource;
};

class CWLSHMProtocol : public IWaylandProtocol {
  public:
    CWLSHMProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CWLSHMResource* resource);
    void destroyResource(CWLSHMPoolResource* resource);
    void destroyResource(CWLSHMBuffer* resource);

    //
    std::vector<SP<CWLSHMResource>>     m_vManagers;
    std::vector<SP<CWLSHMPoolResource>> m_vPools;
    std::vector<SP<CWLSHMBuffer>>       m_vBuffers;

    //
    std::vector<uint32_t> shmFormats;

    friend class CWLSHMResource;
    friend class CWLSHMPoolResource;
    friend class CWLSHMBuffer;
};

namespace PROTO {
    inline UP<CWLSHMProtocol> shm;
};
