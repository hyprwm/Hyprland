#pragma once

#include "Buffer.hpp"

class CDMABuffer : public IHLBuffer {
  public:
    CDMABuffer(uint32_t id, wl_client* client, Aquamarine::SDMABUFAttrs&& attrs_);
    virtual ~CDMABuffer() = default;

    virtual Aquamarine::eBufferCapability          caps();
    virtual Aquamarine::eBufferType                type();
    virtual bool                                   isSynchronous();
    virtual void                                   update(const CRegion& damage);
    virtual const Aquamarine::SDMABUFAttrs&        dmabuf() const;
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    bool                                           good();

    bool                                           success = false;

  private:
    const Aquamarine::SDMABUFAttrs attrs;
    Aquamarine::SDMABUFAttrs       implicit;

    struct {
        CHyprSignalListener resourceDestroy;
    } listeners;
};
