#pragma once

#include "Buffer.hpp"
#include <hyprutils/os/FileDescriptor.hpp>

class CDMABuffer : public IHLBuffer {
  public:
    CDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs const& attrs_);
    virtual ~CDMABuffer();

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual bool                                   isSynchronous();
    virtual void                                   update(const CRegion& damage);
    virtual Aquamarine::SDMABUFAttrs               dmabuf();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    virtual void                                   createTexture();
    bool                                           good();
    void                                           closeFDs();
    Hyprutils::OS::CFileDescriptor                 exportSyncFile();

    bool                                           m_success = false;

  private:
    Aquamarine::SDMABUFAttrs m_attrs;

    struct {
        CHyprSignalListener resourceDestroy;
    } m_listeners;
};
