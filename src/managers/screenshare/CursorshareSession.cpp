#include "ScreenshareManager.hpp"
#include "../PointerManager.hpp"
#include "../../protocols/core/Seat.hpp"
#include "../permissions/DynamicPermissionManager.hpp"
#include "../../render/Renderer.hpp"

using namespace Screenshare;

CCursorshareSession::CCursorshareSession(wl_client* client, WP<CWLPointerResource> pointer) : m_client(client), m_pointer(pointer) {
    m_listeners.pointerDestroyed = m_pointer->m_events.destroyed.listen([this] { stop(); });
    m_listeners.cursorChanged    = g_pPointerManager->m_events.cursorChanged.listen([this] {
        calculateConstraints();
        m_events.constraintsChanged.emit();

        if (m_pendingFrame.pending) {
            if (copy())
                return;

            LOGM(Log::ERR, "Failed to copy cursor image for cursor share");
            if (m_pendingFrame.callback)
                m_pendingFrame.callback(RESULT_NOT_COPIED);
            m_pendingFrame.pending = false;
            return;
        }
    });

    calculateConstraints();
}

CCursorshareSession::~CCursorshareSession() {
    stop();
}

void CCursorshareSession::stop() {
    if (m_stopped)
        return;
    m_stopped = true;
    m_events.stopped.emit();
}

void CCursorshareSession::calculateConstraints() {
    const auto& cursorImage = g_pPointerManager->currentCursorImage();
    m_constraintsChanged    = true;

    // cursor is hidden, keep the previous constraints and render 0 alpha
    if (!cursorImage.pBuffer)
        return;

    // TODO: should cursor share have a format bit flip for RGBA?
    if (auto attrs = cursorImage.pBuffer->shm(); attrs.success) {
        m_format = attrs.format;
    } else {
        // we only have shm cursors
        return;
    }

    m_hotspot    = cursorImage.hotspot;
    m_bufferSize = cursorImage.size;
}

// TODO: allow render to buffer without monitor and remove monitor param
eScreenshareError CCursorshareSession::share(PHLMONITOR monitor, SP<IHLBuffer> buffer, FSourceBoxCallback sourceBoxCallback, FScreenshareCallback callback) {
    if (m_stopped || m_pointer.expired() || m_bufferSize == Vector2D(0, 0))
        return ERROR_STOPPED;

    if UNLIKELY (!buffer || !buffer->m_resource || !buffer->m_resource->good()) {
        LOGM(Log::ERR, "Client requested sharing to an invalid buffer");
        return ERROR_NO_BUFFER;
    }

    if UNLIKELY (buffer->size != m_bufferSize) {
        LOGM(Log::ERR, "Client requested sharing to an invalid buffer size");
        return ERROR_BUFFER_SIZE;
    }

    uint32_t bufFormat;
    if (buffer->dmabuf().success)
        bufFormat = buffer->dmabuf().format;
    else if (buffer->shm().success)
        bufFormat = buffer->shm().format;
    else {
        LOGM(Log::ERR, "Client requested sharing to an invalid buffer");
        return ERROR_NO_BUFFER;
    }

    if (bufFormat != m_format) {
        LOGM(Log::ERR, "Invalid format {} in {:x}", bufFormat, (uintptr_t)this);
        return ERROR_BUFFER_FORMAT;
    }

    m_pendingFrame.pending           = true;
    m_pendingFrame.monitor           = monitor;
    m_pendingFrame.buffer            = buffer;
    m_pendingFrame.sourceBoxCallback = sourceBoxCallback;
    m_pendingFrame.callback          = callback;

    // nothing changed, then delay copy until contraints changed
    if (!m_constraintsChanged)
        return ERROR_NONE;

    if (!copy()) {
        LOGM(Log::ERR, "Failed to copy cursor image for cursor share");
        callback(RESULT_NOT_COPIED);
        m_pendingFrame.pending = false;
        return ERROR_UNKNOWN;
    }

    return ERROR_NONE;
}

void CCursorshareSession::render() {
    const auto  PERM = g_pDynamicPermissionManager->clientPermissionMode(m_client, PERMISSION_TYPE_CURSOR_POS);

    const auto& cursorImage = g_pPointerManager->currentCursorImage();

    // TODO: implement a monitor independent render mode to buffer that does this in CHyprRenderer::begin() or something like that
    g_pHyprOpenGL->m_renderData.transformDamage = false;
    g_pHyprOpenGL->setViewport(0, 0, m_bufferSize.x, m_bufferSize.y);

    bool overlaps = g_pPointerManager->getCursorBoxGlobal().overlaps(m_pendingFrame.sourceBoxCallback());
    if (PERM != PERMISSION_RULE_ALLOW_MODE_ALLOW || !overlaps) {
        // render black when not allowed
        g_pHyprOpenGL->clear(Colors::BLACK);
    } else if (!cursorImage.pBuffer || !cursorImage.surface || !cursorImage.bufferTex) {
        // render clear when cursor is probably hidden
        g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 0));
    } else {
        // render cursor
        CBox texbox = {{}, cursorImage.bufferTex->m_size};
        g_pHyprOpenGL->renderTexture(cursorImage.bufferTex, texbox, {});
    }

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
}

bool CCursorshareSession::copy() {
    if (!m_pendingFrame.callback || !m_pendingFrame.monitor || !m_pendingFrame.callback || !m_pendingFrame.sourceBoxCallback)
        return false;

    // FIXME: this doesn't really make sense but just to be safe
    m_pendingFrame.callback(RESULT_TIMESTAMP);

    g_pHyprRenderer->makeEGLCurrent();

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};
    if (auto attrs = m_pendingFrame.buffer->dmabuf(); attrs.success) {
        if (attrs.format != m_format) {
            LOGM(Log::ERR, "Can't copy: invalid format");
            return false;
        }

        if (!g_pHyprRenderer->beginRender(m_pendingFrame.monitor, fakeDamage, RENDER_MODE_TO_BUFFER, m_pendingFrame.buffer, nullptr, true)) {
            LOGM(Log::ERR, "Can't copy: failed to begin rendering to dmabuf");
            return false;
        }

        render();

        g_pHyprRenderer->endRender([callback = m_pendingFrame.callback]() {
            if (callback)
                callback(RESULT_COPIED);
        });
    } else if (auto attrs = m_pendingFrame.buffer->shm(); attrs.success) {
        auto [bufData, fmt, bufLen] = m_pendingFrame.buffer->beginDataPtr(0);
        const auto PFORMAT          = NFormatUtils::getPixelFormatFromDRM(m_format);

        if (attrs.format != m_format || !PFORMAT) {
            LOGM(Log::ERR, "Can't copy: invalid format");
            return false;
        }

        CFramebuffer outFB;
        outFB.alloc(m_bufferSize.x, m_bufferSize.y, m_format);

        if (!g_pHyprRenderer->beginRender(m_pendingFrame.monitor, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &outFB, true)) {
            LOGM(Log::ERR, "Can't copy: failed to begin rendering to shm");
            return false;
        }

        render();

        g_pHyprRenderer->endRender();

        g_pHyprOpenGL->m_renderData.pMonitor = m_pendingFrame.monitor;
        outFB.bind();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, outFB.getFBID());

        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        int glFormat = PFORMAT->glFormat;

        if (glFormat == GL_RGBA)
            glFormat = GL_BGRA_EXT;

        if (glFormat != GL_BGRA_EXT && glFormat != GL_RGB) {
            if (PFORMAT->swizzle.has_value()) {
                std::array<GLint, 4> RGBA = SWIZZLE_RGBA;
                std::array<GLint, 4> BGRA = SWIZZLE_BGRA;
                if (PFORMAT->swizzle == RGBA)
                    glFormat = GL_RGBA;
                else if (PFORMAT->swizzle == BGRA)
                    glFormat = GL_BGRA_EXT;
                else {
                    LOGM(Log::ERR, "Copied frame via shm might be broken or color flipped");
                    glFormat = GL_RGBA;
                }
            }
        }

        glReadPixels(0, 0, m_bufferSize.x, m_bufferSize.y, glFormat, PFORMAT->glType, bufData);

        g_pHyprOpenGL->m_renderData.pMonitor.reset();

        m_pendingFrame.buffer->endDataPtr();
        outFB.unbind();
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

        m_pendingFrame.callback(RESULT_COPIED);
    } else {
        LOGM(Log::ERR, "Can't copy: invalid buffer type");
        return false;
    }

    m_pendingFrame.pending = false;
    m_constraintsChanged   = false;
    return true;
}

DRMFormat CCursorshareSession::format() const {
    return m_format;
}

Vector2D CCursorshareSession::bufferSize() const {
    return m_bufferSize;
}

Vector2D CCursorshareSession::hotspot() const {
    return m_hotspot;
}
