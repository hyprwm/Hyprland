#pragma once

#include "../../defines.hpp"
#include "../../render/Texture.hpp"
#include "./WLBuffer.hpp"
#include "../DRMSyncobj.hpp"

#include <aquamarine/buffer/Buffer.hpp>

class CSyncReleaser;
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
    virtual SP<CTexture>                  createTexture() = 0;

    void                                  onBackendRelease(const std::function<void()>& fn);
    void                                  addReleasePoint(CDRMSyncPointState& point);

    bool                                  m_opaque = false;
    SP<CWLBufferResource>                 m_resource;
    std::vector<UP<CSyncReleaser>>        m_syncReleasers;
    Hyprutils::OS::CFileDescriptor        m_syncFd;

    struct {
        CHyprSignalListener backendRelease;
        CHyprSignalListener backendRelease2; // for explicit ds
    } m_hlEvents;

  private:
    int                   m_locks = 0;
    std::function<void()> m_backendReleaseQueuedFn;

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
    bool                operator==(const CHLBufferReference& other) const;
    bool                operator==(const SP<IHLBuffer>& other) const;
    bool                operator==(const SP<Aquamarine::IBuffer>& other) const;
    SP<IHLBuffer>       operator->() const;
    //
    operator bool() const;

    // unlock and drop the buffer without sending release
    void          drop();

    SP<IHLBuffer> m_buffer;
};
