#include "ScreenshareManager.hpp"
#include "../PointerManager.hpp"
#include "../../protocols/core/Seat.hpp"
#include "../permissions/DynamicPermissionManager.hpp"
#include "../../render/Renderer.hpp"

#include <cstring>

CCursorshareSession::CCursorshareSession(wl_client* client, WP<CWLPointerResource> pointer) : m_client(client), m_pointer(pointer) {
    m_listeners.pointerDestroyed = m_pointer->m_events.destroyed.listen([this] { stop(); });
    m_listeners.cursorChanged    = g_pPointerManager->m_events.cursorChanged.listen([this] {
        calculateConstraints();
        m_events.constraintsChanged.emit();
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
eScreenshareError CCursorshareSession::share(PHLMONITOR monitor, SP<IHLBuffer> buffer, FScreenshareCallback callback) {
    if (m_stopped || m_pointer.expired())
        return ERROR_STOPPED;

    if UNLIKELY (!buffer || !buffer->m_resource || !buffer->m_resource->good()) {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return ERROR_NO_BUFFER;
    }

    if UNLIKELY (buffer->size != m_bufferSize) {
        LOGM(ERR, "Client requested sharing to an invalid buffer size");
        return ERROR_BUFFER_SIZE;
    }

    if (auto attrs = buffer->dmabuf(); attrs.success && attrs.format != m_format) {
        LOGM(ERR, "Invalid dmabuf format {} in {:x}", attrs.format, (uintptr_t)this);
        return ERROR_BUFFER_FORMAT;
    } else if (auto attrs = buffer->shm(); attrs.success && attrs.format != m_format) {
        LOGM(ERR, "Invalid shm format {} in {:x}", attrs.format, (uintptr_t)this);
        return ERROR_BUFFER_FORMAT;
    } else {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return ERROR_NO_BUFFER;
    }

    if (!copy(monitor, buffer, callback)) {
        callback(RESULT_NOT_COPIED);
        return ERROR_UNKNOWN;
    }

    return ERROR_NONE;
}

bool CCursorshareSession::copy(PHLMONITOR monitor, SP<IHLBuffer> buffer, FScreenshareCallback callback) {
    const auto& cursorImage = g_pPointerManager->currentCursorImage();

    const auto  PERM = g_pDynamicPermissionManager->clientPermissionMode(m_client, PERMISSION_TYPE_CURSOR);
    if (PERM != PERMISSION_RULE_ALLOW_MODE_PENDING && PERM != PERMISSION_RULE_ALLOW_MODE_ALLOW)
        return false;

    // FIXME: this doesn't really make sense but just to be safe
    callback(RESULT_TIMESTAMP);

    if (auto attrs = buffer->dmabuf(); attrs.success) {
        if (attrs.format != m_format)
            return false;

        CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

        if (!g_pHyprRenderer->beginRender(monitor, fakeDamage, RENDER_MODE_TO_BUFFER, buffer, nullptr, true)) {
            LOGM(ERR, "Can't copy: failed to begin rendering to dma frame");
            return false;
        }

        if (cursorImage.surface && cursorImage.bufferTex && PERM != PERMISSION_RULE_ALLOW_MODE_PENDING) {
            CBox texbox = {{}, cursorImage.bufferTex->m_size};
            g_pHyprOpenGL->renderTexture(cursorImage.bufferTex, texbox, {});
        } else {
            g_pHyprOpenGL->clear(Colors::BLACK);
        }

        g_pHyprOpenGL->m_renderData.blockScreenShader = true;

        g_pHyprRenderer->endRender([callback]() {
            if (callback)
                callback(RESULT_COPIED);
        });
    } else if (auto attrs = buffer->shm(); attrs.success) {
        auto [bufData, fmt, bufLen] = buffer->beginDataPtr(0);

        if (cursorImage.surface && cursorImage.pBuffer && PERM != PERMISSION_RULE_ALLOW_MODE_PENDING) {
            // we have to copy from gpu if we render this anyway, so dont render and just copy
            auto [cursorPixelData, cursorFmt, cursorBufLen] = cursorImage.pBuffer->beginDataPtr(0);

            if (fmt != m_format || fmt != cursorFmt || bufLen != cursorBufLen)
                return false;

            memcpy(bufData, cursorPixelData, bufLen);

            buffer->endDataPtr();
            cursorImage.pBuffer->endDataPtr();

        } else {
            memset(bufData, 0, bufLen);
        }

        callback(RESULT_COPIED);
    } else
        return false;

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
