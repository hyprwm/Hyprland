#include "VKTexture.hpp"

CVKTexture::CVKTexture(bool opaque) {
    m_opaque = opaque;
};

CVKTexture::CVKTexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) :
    ITexture(drmFormat, pixels, stride, size, keepDataCopy, opaque) {

    };

CVKTexture::CVKTexture(const Aquamarine::SDMABUFAttrs&, void* image, bool opaque) {
    m_opaque = opaque;
};

CVKTexture::~CVKTexture() {
    destroyTexture();
};

void CVKTexture::destroyTexture() {};
void CVKTexture::setTexParameter(GLenum pname, GLint param) {};
void CVKTexture::allocate() {};
void CVKTexture::update(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const CRegion& damage) {};