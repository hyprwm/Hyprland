#pragma once

#include "../defines.hpp"
#include "../helpers/Format.hpp"
#include "Texture.hpp"
#include <cstdint>
#include <drm_fourcc.h>

class CHLBufferReference;

class IFramebuffer {
  public:
    IFramebuffer() = default;
    IFramebuffer(const std::string& name);
    virtual ~IFramebuffer() = default;

    virtual bool alloc(int w, int h, uint32_t format = DRM_FORMAT_ARGB8888);
    virtual void release()                                                                         = 0;
    virtual bool readPixels(CHLBufferReference buffer, uint32_t offsetX = 0, uint32_t offsetY = 0) = 0;

    virtual void bind() = 0;

    bool         isAllocated();
    SP<ITexture> getTexture();
    SP<ITexture> getStencilTex();

    virtual void addStencil(SP<ITexture> tex) = 0;

    Vector2D     m_size;
    DRMFormat    m_drmFormat = DRM_FORMAT_INVALID;

  protected:
    virtual bool internalAlloc(int w, int h, uint32_t format = DRM_FORMAT_ARGB8888) = 0;

    SP<ITexture> m_tex;
    bool         m_fbAllocated = false;

    SP<ITexture> m_stencilTex;
    std::string  m_name; // name for logging
};
