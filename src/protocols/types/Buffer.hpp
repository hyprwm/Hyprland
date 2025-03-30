#pragma once

#include "../../defines.hpp"
#include "../../render/Texture.hpp"
#include "./WLBuffer.hpp"

#include <aquamarine/buffer/Buffer.hpp>

class CDRMSyncPointState;
class CHLBufferReference;

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
    void                                  addReleasePoint(UP<CDRMSyncPointState> point);

    SP<CTexture>                          texture;
    bool                                  opaque = false;
    SP<CWLBufferResource>                 resource;

    struct {
        CHyprSignalListener backendRelease;
        CHyprSignalListener backendRelease2; // for explicit ds
    } hlEvents;

  private:
    std::vector<UP<CDRMSyncPointState>> releasePoints;
    int                                 nLocks = 0;

    friend class CHLBufferReference;
};

// for ref-counting. Releases in ~dtor
class CHLBufferReference {
  public:
    CHLBufferReference();
    CHLBufferReference(const CHLBufferReference& other);
    CHLBufferReference(SP<IHLBuffer> buffer);
    ~CHLBufferReference();

    CHLBufferReference& operator=(const CHLBufferReference& other);
    CHLBufferReference& operator=(CHLBufferReference&& other) noexcept;
    bool                operator==(const CHLBufferReference& other) const;
    bool                operator==(const SP<IHLBuffer>& other) const;
    bool                operator==(const SP<Aquamarine::IBuffer>& other) const;
    SP<IHLBuffer>       operator->() const;
    operator bool() const;

    // unlock and drop the buffer without sending release
    void                   drop();

    UP<CDRMSyncPointState> release;
    SP<IHLBuffer>          buffer;
};
