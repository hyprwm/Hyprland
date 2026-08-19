#include "TextureMatteElement.hpp"

CTextureMatteElement::CTextureMatteElement(const CTextureMatteElement::STextureMatteData& data_) : m_data(data_) {
    ;
}

bool CTextureMatteElement::needsLiveBlur(const Render::CRenderingContext&) {
    return false;
}

bool CTextureMatteElement::needsPrecomputeBlur(const Render::CRenderingContext&) {
    return false;
}
