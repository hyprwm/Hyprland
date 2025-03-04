#pragma once

#include "../../defines.hpp"
#include "../../render/Texture.hpp"
#include "./WLBuffer.hpp"
#include "protocols/DRMSyncobj.hpp"

#include <aquamarine/buffer/Buffer.hpp>

class CSyncReleaser;

class IHLBuffer : public Aquamarine::IBuffer {
  public:
    virtual ~IHLBuffer();
    virtual Aquamarine::eBufferCapability caps()                        = 0;
    virtual Aquamarine::eBufferType       type()                        = 0;
    virtual void                          update(const CRegion& damage) = 0;
    virtual bool                          isSynchronous()               = 0; // whether the updates to this buffer are synchronous, aka happen over cpu
    virtual bool                          good()                        = 0;
    virtual void                          sendRelease();
    virtual void                          lock();
    virtual void                          unlock();
    virtual bool                          locked();

    void                                  onBackendRelease(const std::function<void()>& fn);

    SP<CTexture>                          texture;
    bool                                  opaque = false;
    SP<CWLBufferResource>                 resource;

    struct {
        CHyprSignalListener backendRelease;
        CHyprSignalListener backendRelease2; // for explicit ds
    } hlEvents;

  private:
    int nLocks = 0;
};

// for ref-counting. Releases in ~dtor
// surface optional
class CHLBufferReference {
  public:
    CHLBufferReference(WP<IHLBuffer> buffer, SP<CWLSurfaceResource> surface);
    ~CHLBufferReference();

    WP<IHLBuffer>          buffer;
    UP<CDRMSyncPointState> acquire;
    UP<CDRMSyncPointState> release;
    UP<CSyncReleaser>      syncReleaser;

  private:
    WP<CWLSurfaceResource> surface;
};
