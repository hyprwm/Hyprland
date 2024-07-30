#pragma once

#include "../../defines.hpp"
#include "../../render/Texture.hpp"
#include "./WLBuffer.hpp"

#include <aquamarine/buffer/Buffer.hpp>

class IHLBuffer : public Aquamarine::IBuffer {
  public:
    virtual ~IHLBuffer();
    virtual Aquamarine::eBufferCapability caps()                        = 0;
    virtual Aquamarine::eBufferType       type()                        = 0;
    virtual void                          update(const CRegion& damage) = 0;
    virtual bool                          isSynchronous()               = 0; // whether the updates to this buffer are synchronous, aka happen over cpu
    virtual bool                          good()                        = 0;
    virtual void                          sendRelease();
    virtual void                          sendReleaseWithSurface(SP<CWLSurfaceResource>);
    virtual void                          lock();
    virtual void                          unlock();
    virtual void                          unlockWithSurface(SP<CWLSurfaceResource> surf);
    virtual bool                          locked();

    void                                  unlockOnBufferRelease(WP<CWLSurfaceResource> surf /* optional */);

    SP<CTexture>                          texture;
    bool                                  opaque = false;
    SP<CWLBufferResource>                 resource;

    struct {
        CHyprSignalListener backendRelease;
    } hlEvents;

  private:
    int                    nLocks = 0;

    WP<CWLSurfaceResource> unlockSurface;
};

// for ref-counting. Releases in ~dtor
// surface optional
class CHLBufferReference {
  public:
    CHLBufferReference(SP<IHLBuffer> buffer, SP<CWLSurfaceResource> surface);
    ~CHLBufferReference();

    WP<IHLBuffer> buffer;

  private:
    WP<CWLSurfaceResource> surface;
};
