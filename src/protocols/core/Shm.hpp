#pragma once

/*
    Implementations for:
     - wl_shm
     - wl_shm_pool
     - wl_buffer with shm
*/

#include <hyprutils/os/FileDescriptor.hpp>
#include <memory>
#include <vector>
#include <cstdint>
#include "../WaylandProtocol.hpp"
#include "wayland.hpp"
#include "../types/Buffer.hpp"
#include "../../helpers/math/Math.hpp"

class CWLSHMPoolResource;

class CSHMPool {
  public:
    CSHMPool() = delete;
    CSHMPool(Hyprutils::OS::CFileDescriptor fd, size_t size);
    ~CSHMPool();

    Hyprutils::OS::CFileDescriptor m_fd;
    size_t                         m_size = 0;
    void*                          m_data = nullptr;

    void                           resize(size_t size);
};

class CWLSHMBuffer : public IHLBuffer {
  public:
    CWLSHMBuffer(WP<CWLSHMPoolResource> pool, uint32_t id, int32_t offset, const Vector2D& size, int32_t stride, uint32_t fmt);
    virtual ~CWLSHMBuffer();

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual void                                   update(const CRegion& damage);
    virtual bool                                   isSynchronous();
    virtual Aquamarine::SSHMAttrs                  shm();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    virtual SP<CTexture>                           createTexture();

    bool                                           good();

    int32_t                                        m_offset = 0;
    int32_t                                        m_stride = 0;
    uint32_t                                       m_fmt    = 0;
    SP<CSHMPool>                                   m_pool;

  private:
    struct {
        CHyprSignalListener bufferResourceDestroy;
    } m_listeners;
};

class CWLSHMPoolResource {
  public:
    CWLSHMPoolResource(UP<CWlShmPool>&& resource_, Hyprutils::OS::CFileDescriptor fd, size_t size);

    bool                   good();

    SP<CSHMPool>           m_pool;

    WP<CWLSHMPoolResource> m_self;

  private:
    UP<CWlShmPool> m_resource;

    friend class CWLSHMBuffer;
};

class CWLSHMResource {
  public:
    CWLSHMResource(UP<CWlShm>&& resource_);

    bool good();

  private:
    UP<CWlShm> m_resource;
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
    std::vector<UP<CWLSHMResource>>     m_managers;
    std::vector<UP<CWLSHMPoolResource>> m_pools;
    std::vector<SP<CWLSHMBuffer>>       m_buffers;

    //
    std::vector<uint32_t> m_shmFormats;

    friend class CWLSHMResource;
    friend class CWLSHMPoolResource;
    friend class CWLSHMBuffer;
};

namespace PROTO {
    inline UP<CWLSHMProtocol> shm;
};
