#pragma once

#include "../defines.hpp"
#include <aquamarine/buffer/Buffer.hpp>
#include <hyprutils/math/Misc.hpp>
#include "../protocols/types/ColorManagement.hpp"

class IHLBuffer;
HYPRUTILS_FORWARD(Math, CRegion);

enum eTextureType : int8_t {
    TEXTURE_INVALID = -1, // Invalid
    TEXTURE_RGBA    = 0,  // 4 channels
    TEXTURE_RGBX,         // discard A
    TEXTURE_EXTERNAL,     // EGLImage
};

class CTexture {
  public:
    CTexture();

    CTexture(CTexture&)        = delete;
    CTexture(CTexture&&)       = delete;
    CTexture(const CTexture&&) = delete;
    CTexture(const CTexture&)  = delete;

    CTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false);

    CTexture(const SP<Aquamarine::IBuffer> buffer, bool keepDataCopy = false);
    CTexture(const SP<Aquamarine::IBuffer> buffer, const NColorManagement::SImageDescription& imageDescription);
    // this ctor takes ownership of the eglImage.
    CTexture(const Aquamarine::SDMABUFAttrs&, void* image);
    ~CTexture();

    void                        destroyTexture();
    void                        allocate();
    void                        update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage);
    const std::vector<uint8_t>& dataCopy();
    void                        bind();
    void                        unbind();
    void                        setTexParameter(GLenum pname, GLint param);

    eTextureType                m_type          = TEXTURE_RGBA;
    GLenum                      m_target        = GL_TEXTURE_2D;
    GLuint                      m_texID         = 0;
    Vector2D                    m_size          = {};
    void*                       m_eglImage      = nullptr;
    eTransform                  m_transform     = HYPRUTILS_TRANSFORM_NORMAL;
    bool                        m_opaque        = false;
    uint32_t                    m_drmFormat     = 0; // for shm
    bool                        m_isSynchronous = false;

    //
    std::optional<NColorManagement::SImageDescription> m_imageDescription;

  private:
    enum eTextureParam : uint8_t {
        TEXTURE_PAR_WRAP_S = 0,
        TEXTURE_PAR_WRAP_T,
        TEXTURE_PAR_MAG_FILTER,
        TEXTURE_PAR_MIN_FILTER,
        TEXTURE_PAR_SWIZZLE_R,
        TEXTURE_PAR_SWIZZLE_B,
        TEXTURE_PAR_LAST,
    };

    void                                               createFromShm(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size);
    void                                               createFromDma(const Aquamarine::SDMABUFAttrs&, void* image);
    inline constexpr std::optional<size_t>             getCacheStateIndex(GLenum pname);

    bool                                               m_keepDataCopy = false;
    std::vector<uint8_t>                               m_dataCopy;
    std::array<std::optional<GLint>, TEXTURE_PAR_LAST> m_cachedStates;
};
