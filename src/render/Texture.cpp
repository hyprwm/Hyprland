#include "Texture.hpp"

CTexture::CTexture() {
    // naffin'
}

CTexture::CTexture(wlr_texture* tex) {
    RASSERT(wlr_texture_is_gles2(tex), "wlr_texture provided to CTexture that isn't GLES2!");
    wlr_gles2_texture_attribs attrs;
    wlr_gles2_texture_get_attribs(tex, &attrs);

    m_iTarget = attrs.target;
    m_iTexID  = attrs.tex;

    if (m_iTarget == GL_TEXTURE_2D) {
        m_iType = attrs.has_alpha ? TEXTURE_RGBA : TEXTURE_RGBX;
    } else {
        m_iType = TEXTURE_EXTERNAL;
    }

    m_vSize = Vector2D(tex->width, tex->height);
}

void CTexture::destroyTexture() {
    if (m_iTexID) {
        glDeleteTextures(1, &m_iTexID);
        m_iTexID = 0;
    }
}

void CTexture::allocate() {
    if (!m_iTexID) {
        glGenTextures(1, &m_iTexID);
    }
}