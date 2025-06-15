#include "TextureMatteElement.hpp"
#include "../OpenGL.hpp"

CTextureMatteElement::CTextureMatteElement(const CTextureMatteElement::STextureMatteData& data_) : m_data(data_) {
    ;
}

void CTextureMatteElement::draw(const CRegion& damage) {
    if (m_data.disableTransformAndModify) {
        g_pHyprOpenGL->pushMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTextureMatte(m_data.tex, m_data.box, *m_data.fb);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->popMonitorTransformEnabled();
    } else
        g_pHyprOpenGL->renderTextureMatte(m_data.tex, m_data.box, *m_data.fb);
}

bool CTextureMatteElement::needsLiveBlur() {
    return false;
}

bool CTextureMatteElement::needsPrecomputeBlur() {
    return false;
}