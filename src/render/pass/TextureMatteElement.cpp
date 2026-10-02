#include "TextureMatteElement.hpp"

CTextureMatteElement::CTextureMatteElement(const CTextureMatteElement::STextureMatteData& data_) : m_data(data_) {
    ;
}

bool CTextureMatteElement::needsLiveBlur(Render::CRenderContext& ctx) {
    return false;
}

bool CTextureMatteElement::needsPrecomputeBlur(Render::CRenderContext& ctx) {
    return false;
}
