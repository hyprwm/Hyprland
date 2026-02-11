#pragma once

#include "../defines.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/math/Misc.hpp>

class IHLBuffer;
HYPRUTILS_FORWARD(Math, CRegion);

enum eTextureType : int8_t {
    TEXTURE_INVALID = -1, // Invalid
    TEXTURE_RGBA    = 0,  // 4 channels
    TEXTURE_RGBX,         // discard A
    TEXTURE_EXTERNAL,     // EGLImage
};

class ITexture {
  public:
    ITexture(ITexture&)        = delete;
    ITexture(ITexture&&)       = delete;
    ITexture(const ITexture&&) = delete;
    ITexture(const ITexture&)  = delete;

    virtual ~ITexture() = default;

    virtual void                destroyTexture()                                                                    = 0;
    virtual void                setTexParameter(GLenum pname, GLint param)                                          = 0;
    virtual void                allocate()                                                                          = 0;
    virtual void                update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) = 0;
    virtual void                bind() {};
    virtual void                unbind() {};
    virtual bool                ok();
    virtual bool                isDMA();

    const std::vector<uint8_t>& dataCopy();

    eTextureType                m_type      = TEXTURE_RGBA;
    Vector2D                    m_size      = {};
    eTransform                  m_transform = HYPRUTILS_TRANSFORM_NORMAL;
    bool                        m_opaque    = false;

    uint32_t                    m_drmFormat     = 0; // for shm
    bool                        m_isSynchronous = false;

    // TODO move to GLTexture
    GLuint m_texID   = 0;
    GLenum magFilter = GL_LINEAR; // useNearestNeighbor overwrites these
    GLenum minFilter = GL_LINEAR;

  protected:
    ITexture() = default;
    ITexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false);

    bool                 m_keepDataCopy = false;
    std::vector<uint8_t> m_dataCopy;
};
