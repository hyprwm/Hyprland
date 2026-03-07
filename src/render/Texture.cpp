#include "Texture.hpp"
#include <cstring>

ITexture::ITexture(uint32_t drmFormat, uint8_t* pixels, uint32_t stride, const Vector2D& size, bool keepDataCopy, bool opaque) :
    m_size(size), m_opaque(opaque), m_drmFormat(drmFormat), m_keepDataCopy(keepDataCopy) {
    if (m_keepDataCopy && stride && pixels) {
        m_dataCopy.resize(stride * size.y);
        memcpy(m_dataCopy.data(), pixels, stride * size.y);
    }
}

ITexture::ITexture(std::span<const float> lut3D, size_t N) : m_type(TEXTURE_3D_LUT), m_size(lut3D.size() / 3, 1), m_isSynchronous(true) {}

bool ITexture::ok() {
    return false;
}

bool ITexture::isDMA() {
    return false;
}

const std::vector<uint8_t>& ITexture::dataCopy() {
    return m_dataCopy;
}
