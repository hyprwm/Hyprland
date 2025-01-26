#include "TextureMatteElement.hpp"
#include "../OpenGL.hpp"

CTextureMatteElement::CTextureMatteElement(const CTextureMatteElement::STextureMatteData& data_) : data(data_) {
    ;
}

void CTextureMatteElement::draw(const CRegion& damage) {
    if (data.disableTransformAndModify) {
        g_pHyprOpenGL->setMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTextureMatte(data.tex, data.box, *data.fb);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->setMonitorTransformEnabled(false);
    } else
        g_pHyprOpenGL->renderTextureMatte(data.tex, data.box, *data.fb);
}

bool CTextureMatteElement::needsLiveBlur() {
    return false;
}

bool CTextureMatteElement::needsPrecomputeBlur() {
    return false;
}