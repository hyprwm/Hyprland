#include "ScreenshareManager.hpp"
#include "../PointerManager.hpp"
#include "../input/InputManager.hpp"
#include "../permissions/DynamicPermissionManager.hpp"
#include "../../protocols/ColorManagement.hpp"
#include "../../protocols/XDGShell.hpp"
#include "../../Compositor.hpp"
#include "../../render/Renderer.hpp"
#include "../../render/OpenGL.hpp"
#include "../../helpers/Monitor.hpp"
#include "../../desktop/Window.hpp"

CScreenshareFrame::CScreenshareFrame(WP<CScreenshareSession> session, bool overlayCursor) :
    m_session(session), m_bufferSize(m_session->bufferSize()), m_overlayCursor(overlayCursor) {
    ;
}

CScreenshareFrame::~CScreenshareFrame() {
    if (m_failed || !m_shared)
        return;

    if (!m_copied && m_callback)
        m_callback(RESULT_NOT_COPIED);
}

bool CScreenshareFrame::done() const {
    if (m_session.expired() || m_session->m_stopped)
        return true;

    if (m_failed || m_copied)
        return true;

    if (m_session->m_type == SHARE_MONITOR && !m_session->monitor())
        return true;

    if (m_session->m_type == SHARE_REGION && !m_session->monitor())
        return true;

    if (m_session->m_type == SHARE_WINDOW && (!m_session->monitor() || !validMapped(m_session->m_window)))
        return true;

    if (!m_shared)
        return false;

    if (!m_buffer || !m_buffer->m_resource || !m_buffer->m_resource->good())
        return true;

    if (!m_callback)
        return true;

    return false;
}

eScreenshareError CScreenshareFrame::share(SP<IHLBuffer> buffer, FScreenshareCallback callback) {
    if UNLIKELY (done())
        return ERROR_STOPPED;

    if UNLIKELY (!m_session->monitor() || !g_pCompositor->monitorExists(m_session->monitor())) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        m_failed = true;
        return ERROR_STOPPED;
    }

    if UNLIKELY (m_session->m_type == SHARE_WINDOW && !validMapped(m_session->m_window)) {
        LOGM(ERR, "Client requested sharing of window that is gone or not shareable!");
        m_failed = true;
        return ERROR_STOPPED;
    }

    if UNLIKELY (!buffer || !buffer->m_resource || !buffer->m_resource->good()) {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return ERROR_NO_BUFFER;
    }

    if UNLIKELY (buffer->size != m_bufferSize) {
        LOGM(ERR, "Client requested sharing to an invalid buffer size");
        return ERROR_BUFFER_SIZE;
    }

    uint32_t bufFormat;
    if (buffer->dmabuf().success) {
        bufFormat = buffer->dmabuf().format;
    } else if (buffer->shm().success) {
        bufFormat = buffer->shm().format;
    } else {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return ERROR_NO_BUFFER;
    }

    if (std::ranges::count_if(m_session->allowedFormats(), [&](const SDRMFormat& format) { return format.drmFormat == bufFormat; }) == 0) {
        LOGM(ERR, "Invalid format {} in {:x}", bufFormat, (uintptr_t)this);
        return ERROR_BUFFER_FORMAT;
    }

    m_buffer   = buffer;
    m_callback = callback;
    m_shared   = true;

    g_pHyprRenderer->m_directScanoutBlocked = true;

    return ERROR_NONE;
}

void CScreenshareFrame::copy() {
    if (done())
        return;

    // tell client to send presented timestamp
    // TODO: is this right? this is right after we commit to aq, not when page flip happens..
    m_callback(RESULT_TIMESTAMP);

    // store a snapshot before the permission popup so we don't break screenshots
    const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(m_session->m_client, PERMISSION_TYPE_SCREENCOPY);
    if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING && !m_session->m_tempFB.isAllocated())
        storeTempFB();

    if (m_buffer->shm().success)
        m_failed = !copyShm();
    else if (m_buffer->dmabuf().success)
        m_failed = !copyDmabuf();

    if (m_failed)
        m_callback(RESULT_NOT_COPIED);
}

void CScreenshareFrame::renderMonitor() {
    if ((m_session->m_type != SHARE_MONITOR && m_session->m_type != SHARE_REGION) || done())
        return;

    const auto PMONITOR = m_session->monitor();

    auto       TEXTURE = makeShared<CTexture>(PMONITOR->m_output->state->state().buffer);

    const bool IS_CM_AWARE = PROTO::colorManagement && PROTO::colorManagement->isClientCMAware(m_session->m_client);

    // render monitor texture
    CBox monbox = CBox{{}, PMONITOR->m_pixelSize}
                      .translate({-m_session->m_box.x, -m_session->m_box.y}) // vvvv kinda ass-backwards but that's how I designed the renderer... sigh.
                      .transform(wlTransformToHyprutils(invertTransform(PMONITOR->m_transform)), PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y);
    g_pHyprOpenGL->pushMonitorTransformEnabled(true);
    g_pHyprOpenGL->setRenderModifEnabled(false);
    g_pHyprOpenGL->renderTexture(TEXTURE, monbox,
                                 {
                                     .cmBackToSRGB       = !IS_CM_AWARE,
                                     .cmBackToSRGBSource = !IS_CM_AWARE ? PMONITOR : nullptr,
                                 });
    g_pHyprOpenGL->setRenderModifEnabled(true);
    g_pHyprOpenGL->popMonitorTransformEnabled();

    // render black boxes for noscreenshare
    for (auto const& w : g_pCompositor->m_windows) {
        if (!w->m_windowData.noScreenShare.valueOrDefault())
            continue;

        if (!g_pHyprRenderer->shouldRenderWindow(w, PMONITOR))
            continue;

        if (w->isHidden())
            continue;

        const auto PWORKSPACE = w->m_workspace;

        if UNLIKELY (!PWORKSPACE && !w->m_fadingOut && w->m_alpha->value() != 0.f)
            continue;

        const auto renderOffset     = PWORKSPACE && !w->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D{};
        const auto REALPOS          = w->m_realPosition->value() + renderOffset;
        const auto noScreenShareBox = CBox{REALPOS.x, REALPOS.y, std::max(w->m_realSize->value().x, 5.0), std::max(w->m_realSize->value().y, 5.0)}
                                          .translate(-PMONITOR->m_position)
                                          .scale(PMONITOR->m_scale)
                                          .translate(-m_session->m_box.pos());

        const auto dontRound     = w->isEffectiveInternalFSMode(FSMODE_FULLSCREEN) || w->m_windowData.noRounding.valueOrDefault();
        const auto rounding      = dontRound ? 0 : w->rounding() * PMONITOR->m_scale;
        const auto roundingPower = dontRound ? 2.0f : w->roundingPower();

        g_pHyprOpenGL->renderRect(noScreenShareBox, Colors::BLACK, {.round = rounding, .roundingPower = roundingPower});

        if (w->m_isX11 || !w->m_popupHead)
            continue;

        const auto     geom            = w->m_xdgSurface->m_current.geometry;
        const Vector2D popupBaseOffset = REALPOS - Vector2D{geom.pos().x, geom.pos().y};

        w->m_popupHead->breadthfirst(
            [&](WP<CPopup> popup, void*) {
                if (!popup->m_wlSurface || !popup->m_wlSurface->resource() || !popup->m_mapped)
                    return;

                const auto popRel = popup->coordsRelativeToParent();
                popup->m_wlSurface->resource()->breadthfirst(
                    [&](SP<CWLSurfaceResource> surf, const Vector2D& localOff, void*) {
                        const auto size = surf->m_current.size;
                        const auto surfBox =
                            CBox{popupBaseOffset + popRel + localOff, size}.translate(-PMONITOR->m_position).scale(PMONITOR->m_scale).translate(-m_session->m_box.pos());

                        if LIKELY (surfBox.w > 0 && surfBox.h > 0)
                            g_pHyprOpenGL->renderRect(surfBox, Colors::BLACK, {});
                    },
                    nullptr);
            },
            nullptr);
    }

    if (m_overlayCursor) {
        CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};
        g_pPointerManager->renderSoftwareCursorsFor(PMONITOR, Time::steadyNow(), fakeDamage,
                                                    g_pInputManager->getMouseCoordsInternal() - PMONITOR->m_position - m_session->m_box.pos() / PMONITOR->m_scale, true);
    }
}

void CScreenshareFrame::renderWindow() {
    if (m_session->m_type != SHARE_WINDOW || done())
        return;

    const auto PWINDOW  = m_session->m_window.lock();
    const auto PMONITOR = m_session->monitor();

    const auto NOW = Time::steadyNow();

    g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(PWINDOW); // block the feedback to avoid spamming the surface if it's visible
    g_pHyprRenderer->renderWindow(PWINDOW, PMONITOR, NOW, false, RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    if (!m_overlayCursor)
        return;

    auto pointerSurfaceResource = g_pSeatManager->m_state.pointerFocus.lock();

    if (!pointerSurfaceResource)
        return;

    auto pointerSurface = CWLSurface::fromResource(pointerSurfaceResource);

    if (!pointerSurface || pointerSurface->getWindow() != m_session->m_window)
        return;

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};
    g_pPointerManager->renderSoftwareCursorsFor(PMONITOR->m_self.lock(), NOW, fakeDamage, g_pInputManager->getMouseCoordsInternal() - PWINDOW->m_realPosition->value(), true);
}

void CScreenshareFrame::render() {
    const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(m_session->m_client, PERMISSION_TYPE_SCREENCOPY);

    if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING) {
        g_pHyprOpenGL->clear(Colors::BLACK);
        return;
    }

    if (PERM == PERMISSION_RULE_ALLOW_MODE_DENY || (m_session->m_type == SHARE_WINDOW && m_session->m_window->m_windowData.noScreenShare.valueOrDefault())) {
        g_pHyprOpenGL->clear(Colors::BLACK);
        CBox texbox = CBox{m_session->m_box.size() / 2.F, g_pHyprOpenGL->m_screencopyDeniedTexture->m_size}.translate(-g_pHyprOpenGL->m_screencopyDeniedTexture->m_size / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_screencopyDeniedTexture, texbox, {});
        return;
    }

    if (m_session->m_tempFB.isAllocated()) {
        CBox texbox = {{}, m_session->m_box.size()};
        g_pHyprOpenGL->renderTexture(m_session->m_tempFB.getTexture(), texbox, {});
        m_session->m_tempFB.release();
        return;
    }

    switch (m_session->m_type) {
        case SHARE_REGION: // TODO: could this be better? this is how screencopy works
        case SHARE_MONITOR: renderMonitor(); break;
        case SHARE_WINDOW: renderWindow(); break;
    }
}

bool CScreenshareFrame::copyDmabuf() {
    if (done())
        return false;

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(m_session->monitor(), fakeDamage, RENDER_MODE_TO_BUFFER, m_buffer, nullptr, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering to dma frame");
        return false;
    }

    render();

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;

    g_pHyprRenderer->endRender([self = m_self]() {
        if (!self || self.expired() || self->m_copied)
            return;

        LOGM(TRACE, "Copied frame via dma");
        self->m_callback(RESULT_COPIED);
        self->m_copied = true;
    });

    return true;
}

bool CScreenshareFrame::copyShm() {
    if (done())
        return false;

    g_pHyprRenderer->makeEGLCurrent();

    auto shm                      = m_buffer->shm();
    auto [pixelData, fmt, bufLen] = m_buffer->beginDataPtr(0); // no need for end, cuz it's shm

    const auto PFORMAT = NFormatUtils::getPixelFormatFromDRM(shm.format);
    if (!PFORMAT) {
        LOGM(ERR, "Can't copy: failed to find a pixel format");
        return false;
    }

    const auto   PMONITOR   = m_session->monitor();
    CRegion      fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    CFramebuffer outFB;
    outFB.alloc(m_session->m_box.w, m_session->m_box.h, shm.format);

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &outFB, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering");
        return false;
    }

    render();

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;

    g_pHyprRenderer->endRender();

    auto glFormat = PFORMAT->flipRB ? GL_BGRA_EXT : GL_RGBA;

    g_pHyprOpenGL->m_renderData.pMonitor = PMONITOR;
    outFB.bind();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, outFB.getFBID());

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    const auto drmFmt     = NFormatUtils::getPixelFormatFromDRM(shm.format);
    uint32_t   packStride = NFormatUtils::minStride(drmFmt, m_session->m_box.w);

    // TODO: use pixel buffer object to not block cpu
    if (packStride == sc<uint32_t>(shm.stride)) {
        glReadPixels(0, 0, m_session->m_box.w, m_session->m_box.h, glFormat, PFORMAT->glType, pixelData);
    } else {
        for (size_t i = 0; i < m_session->m_box.h; ++i) {
            uint32_t y = i;
            glReadPixels(0, y, m_session->m_box.w, 1, glFormat, PFORMAT->glType, pixelData + (i * shm.stride));
        }
    }

    outFB.unbind();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    g_pHyprOpenGL->m_renderData.pMonitor.reset();

    if (!m_copied) {
        LOGM(TRACE, "Copied frame via shm");
        m_callback(RESULT_COPIED);
    }

    return true;
}

void CScreenshareFrame::storeTempFB() {
    g_pHyprRenderer->makeEGLCurrent();

    m_session->m_tempFB.alloc(m_session->m_box.w, m_session->m_box.h);

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(m_session->monitor(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &m_session->m_tempFB, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering to temp fb");
        return;
    }

    switch (m_session->m_type) {
        case SHARE_REGION: // TODO: could this be better? this is how screencopy works
        case SHARE_MONITOR: renderMonitor(); break;
        case SHARE_WINDOW: renderWindow(); break;
    }

    g_pHyprRenderer->endRender();
}

Vector2D CScreenshareFrame::bufferSize() const {
    return m_bufferSize;
}
