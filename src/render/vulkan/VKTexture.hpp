#pragma once

#include "../Texture.hpp"

class CVKTexture : public ITexture {
  public:
    CVKTexture(CVKTexture&)        = delete;
    CVKTexture(CVKTexture&&)       = delete;
    CVKTexture(const CVKTexture&&) = delete;
    CVKTexture(const CVKTexture&)  = delete;

    CVKTexture(bool opaque = false);
    CVKTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy = false, bool opaque = false);
    CVKTexture(const Aquamarine::SDMABUFAttrs&, void* image, bool opaque = false);
    ~CVKTexture();

    void destroyTexture() override;
    void setTexParameter(GLenum pname, GLint param) override;
    void allocate() override;
    void update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) override;
    // virtual void                bind() {};
    // virtual void                unbind() {};
    // virtual bool                ok();
    // virtual bool                isDMA();

  private:
};
