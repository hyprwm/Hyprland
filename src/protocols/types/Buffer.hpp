#pragma once

#include "../../defines.hpp"
#include "../../helpers/signal/Signal.hpp"
#include "../../render/Texture.hpp"

#include <array>
#include <tuple>

enum eBufferCapability {
    BUFFER_CAPABILITY_DATAPTR = (1 << 0),
};

enum eBufferType {
    BUFFER_TYPE_DMABUF = 0,
    BUFFER_TYPE_SHM,
    BUFFER_TYPE_MISC,
};

class CWLBufferResource;

struct SDMABUFAttrs {
    bool                    success = false;
    Vector2D                size;
    uint32_t                format   = 0; // fourcc
    uint64_t                modifier = 0;

    int                     planes  = 1;
    std::array<uint32_t, 4> offsets = {0};
    std::array<uint32_t, 4> strides = {0};
    std::array<int, 4>      fds     = {-1, -1, -1, -1};
};

struct SSHMAttrs {
    bool     success = false;
    int      fd      = 0;
    uint32_t format  = 0;
    Vector2D size;
    int      stride = 0;
    int64_t  offset = 0;
};

class IWLBuffer {
  public:
    virtual ~IWLBuffer() {
        ;
    };

    virtual eBufferCapability                      caps()                        = 0;
    virtual eBufferType                            type()                        = 0;
    virtual void                                   update(const CRegion& damage) = 0;
    virtual SDMABUFAttrs                           dmabuf();
    virtual SSHMAttrs                              shm();
    virtual std::tuple<uint8_t*, uint32_t, size_t> beginDataPtr(uint32_t flags);
    virtual void                                   endDataPtr();
    virtual void                                   sendRelease();
    virtual void                                   lock();
    virtual void                                   unlock();
    virtual bool                                   locked();

    Vector2D                                       size;
    bool                                           opaque = false;

    SP<CWLBufferResource>                          resource;

    SP<CTexture>                                   texture;

    struct {
        CSignal destroy;
    } events;

  private:
    int locks = 0;
};
