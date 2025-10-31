#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "single-pixel-buffer-v1.hpp"
#include "types/Buffer.hpp"

class CSinglePixelBuffer : public IHLBuffer {
  public:
    CSinglePixelBuffer(uint32_t id, wl_client* client, CHyprColor col);
    virtual ~CSinglePixelBuffer();

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual bool                                   isSynchronous();
    virtual void                                   update(const CRegion& damage);
    virtual Aquamarine::SDMABUFAttrs               dmabuf();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    //
    bool good();
    bool m_success = false;

  private:
    uint32_t m_color = 0x00000000;
};

class CSinglePixelBufferResource {
  public:
    CSinglePixelBufferResource(uint32_t id, wl_client* client, CHyprColor color);
    ~CSinglePixelBufferResource() = default;

    bool good();

  private:
    SP<CSinglePixelBuffer> m_buffer;

    struct {
        CHyprSignalListener bufferResourceDestroy;
    } m_listeners;
};

class CSinglePixelBufferManagerResource {
  public:
    CSinglePixelBufferManagerResource(UP<CWpSinglePixelBufferManagerV1>&& resource_);

    bool good();

  private:
    UP<CWpSinglePixelBufferManagerV1> m_resource;
};

class CSinglePixelProtocol : public IWaylandProtocol {
  public:
    CSinglePixelProtocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

  private:
    void destroyResource(CSinglePixelBufferManagerResource* resource);
    void destroyResource(CSinglePixelBufferResource* resource);

    //
    std::vector<UP<CSinglePixelBufferManagerResource>> m_managers;
    std::vector<UP<CSinglePixelBufferResource>>        m_buffers;

    friend class CSinglePixelBufferManagerResource;
    friend class CSinglePixelBufferResource;
};

namespace PROTO {
    inline UP<CSinglePixelProtocol> singlePixel;
};
