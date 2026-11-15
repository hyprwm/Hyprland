#include "Texture.hpp"

using namespace Render;

ITexture::ITexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool opaque) : m_size(size), m_opaque(opaque), m_drmFormat(drmFormat) {
    ;
}

ITexture::ITexture(std::span<const float> lut3D, size_t N) : m_type(TEXTURE_3D_LUT), m_size(lut3D.size() / 3, 1), m_isSynchronous(true) {}

bool ITexture::ok() {
    return false;
}

bool ITexture::isDMA() {
    return false;
}
