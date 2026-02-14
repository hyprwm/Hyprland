#include "GLFramebuffer.hpp"
#include "../OpenGL.hpp"
#include "../Renderer.hpp"
#include "render/Framebuffer.hpp"

CGLFramebuffer::CGLFramebuffer() : IFramebuffer() {
    ;
}

bool CGLFramebuffer::internalAlloc(int w, int h, uint32_t drmFormat) {
    g_pHyprOpenGL->makeEGLCurrent();

    bool firstAlloc = false;

    if (!m_tex) {
        m_tex = g_pHyprRenderer->createTexture();
        m_tex->allocate();
        m_tex->bind();
        m_tex->setTexParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        m_tex->setTexParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        m_tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        firstAlloc = true;
    }

    if (!m_fbAllocated) {
        glGenFramebuffers(1, &m_fb);
        m_fbAllocated = true;
        firstAlloc    = true;
    }

    if (firstAlloc) {
        const auto format = NFormatUtils::getPixelFormatFromDRM(drmFormat);
        m_tex->bind();
        glTexImage2D(GL_TEXTURE_2D, 0, format->glInternalFormat ? format->glInternalFormat : format->glFormat, w, h, 0, format->glFormat, format->glType, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_tex->m_texID, 0);

        if (m_stencilTex && m_stencilTex->ok()) {
            m_stencilTex->bind();
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, w, h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_stencilTex->m_texID, 0);

            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
        }

        auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        RASSERT((status == GL_FRAMEBUFFER_COMPLETE), "Framebuffer incomplete, couldn't create! (FB status: {}, GL Error: 0x{:x})", status, sc<int>(glGetError()));

        Log::logger->log(Log::DEBUG, "Framebuffer created, status {}", status);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}

void CGLFramebuffer::addStencil(SP<ITexture> tex) {
    if (m_stencilTex == tex)
        return;

    m_stencilTex = tex;
    m_stencilTex->bind();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_size.x, m_size.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fb);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_stencilTex->m_texID, 0);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    RASSERT((status == GL_FRAMEBUFFER_COMPLETE), "Failed adding a stencil to fbo!", status);

    m_stencilTex->unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CGLFramebuffer::bind() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fb);

    if (g_pHyprOpenGL)
        g_pHyprOpenGL->setViewport(0, 0, g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.x, g_pHyprRenderer->m_renderData.pMonitor->m_pixelSize.y);
    else
        glViewport(0, 0, m_size.x, m_size.y);
}

void CGLFramebuffer::unbind() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void CGLFramebuffer::release() {
    if (m_fbAllocated) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glDeleteFramebuffers(1, &m_fb);
        m_fbAllocated = false;
        m_fb          = 0;
    }

    if (m_tex)
        m_tex.reset();

    m_size = Vector2D();
}

CGLFramebuffer::~CGLFramebuffer() {
    release();
}

GLuint CGLFramebuffer::getFBID() {
    return m_fbAllocated ? m_fb : 0;
}

void CGLFramebuffer::invalidate(const std::vector<GLenum>& attachments) {
    if (!isAllocated())
        return;

    glInvalidateFramebuffer(GL_FRAMEBUFFER, attachments.size(), attachments.data());
}
