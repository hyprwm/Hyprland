#pragma once

#include "Buffer.hpp"

class CDMABuffer : public IWLBuffer {
  public:
    CDMABuffer(uint32_t id, wl_client* client, SDMABUFAttrs const& attrs_);
    virtual ~CDMABuffer();

    virtual eBufferCapability                      caps();
    virtual eBufferType                            type();
    virtual bool                                   isSynchronous();
    virtual void                                   update(const CRegion& damage);
    virtual SDMABUFAttrs                           dmabuf();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    bool                                           good();
    void                                           updateTexture();
    void                                           closeFDs();

    bool                                           success = false;

  private:
    SDMABUFAttrs attrs;

    struct {
        CHyprSignalListener resourceDestroy;
    } listeners;
};