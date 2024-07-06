#pragma once

#include "../../defines.hpp"
#include "../../render/Texture.hpp"
#include "./WLBuffer.hpp"

#include <aquamarine/buffer/Buffer.hpp>

class IHLBuffer : public Aquamarine::IBuffer {
  public:
    virtual ~IHLBuffer() {
        ;
    }
    virtual Aquamarine::eBufferCapability caps()                        = 0;
    virtual Aquamarine::eBufferType       type()                        = 0;
    virtual void                          update(const CRegion& damage) = 0;
    virtual bool                          isSynchronous()               = 0; // whether the updates to this buffer are synchronous, aka happen over cpu
    virtual bool                          good()                        = 0;
    virtual void                          sendRelease();
    virtual void                          sendReleaseWithSurface(SP<CWLSurfaceResource>);

    SP<CTexture>                          texture;
    bool                                  opaque = false;
    SP<CWLBufferResource>                 resource;
};
