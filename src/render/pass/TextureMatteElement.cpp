#include "TextureMatteElement.hpp"

CTextureMatteElement::CTextureMatteElement(const CTextureMatteElement::STextureMatteData& data_) : m_data(data_) {
    ;
}

bool CTextureMatteElement::needsLiveBlur() {
    return false;
}

bool CTextureMatteElement::needsPrecomputeBlur() {
    return false;
}