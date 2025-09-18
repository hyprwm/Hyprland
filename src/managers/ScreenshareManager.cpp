#include "ScreenshareManager.hpp"
#include "PointerManager.hpp"
#include "input/InputManager.hpp"
#include "permissions/DynamicPermissionManager.hpp"
#include "../protocols/ColorManagement.hpp"
#include "../protocols/XDGShell.hpp"
#include "../Compositor.hpp"
#include "../render/Renderer.hpp"
#include "../render/OpenGL.hpp"
#include "../helpers/Monitor.hpp"

// TODO: do transform and scale for both constructor's m_box

SScreenshareFrame::SScreenshareFrame(PHLMONITOR monitor, SP<IHLBuffer> buffer, wl_client* client, bool overlayCursor, FScreenshareCallback callback) :
    m_monitor(monitor), m_buffer(CHLBufferReference(buffer)), m_client(client), m_overlayCursor(overlayCursor), m_callback(callback) {
    m_box = {{}, m_monitor->m_transformedSize};
}

SScreenshareFrame::SScreenshareFrame(PHLWINDOW window, SP<IHLBuffer> buffer, wl_client* client, bool overlayCursor, FScreenshareCallback callback) :
    m_window(window), m_buffer(CHLBufferReference(buffer)), m_client(client), m_overlayCursor(overlayCursor), m_callback(callback) {
    m_box = {{}, m_window->m_size};
}

void SScreenshareFrame::renderMonitor() {
    auto       TEXTURE = makeShared<CTexture>(m_monitor->m_output->state->state().buffer);

    CRegion    fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    const bool IS_CM_AWARE = PROTO::colorManagement && PROTO::colorManagement->isClientCMAware(m_client);

    // render monitor texture
    CBox monbox = CBox{0, 0, m_monitor->m_pixelSize.x, m_monitor->m_pixelSize.y};
    g_pHyprOpenGL->setRenderModifEnabled(false);
    g_pHyprOpenGL->renderTexture(TEXTURE, monbox,
                                 {
                                     .cmBackToSRGB       = !IS_CM_AWARE,
                                     .cmBackToSRGBSource = !IS_CM_AWARE ? m_monitor.lock() : nullptr,
                                 });
    g_pHyprOpenGL->setRenderModifEnabled(true);

    // render black boxes for noscreenshare
    for (auto const& w : g_pCompositor->m_windows) {
        if (!w->m_windowData.noScreenShare.valueOrDefault())
            continue;

        if (!g_pHyprRenderer->shouldRenderWindow(w, m_monitor.lock()))
            continue;

        if (w->isHidden())
            continue;

        const auto PWORKSPACE = w->m_workspace;

        if UNLIKELY (!PWORKSPACE && !w->m_fadingOut && w->m_alpha->value() != 0.f)
            continue;

        const auto renderOffset     = PWORKSPACE && !w->m_pinned ? PWORKSPACE->m_renderOffset->value() : Vector2D{};
        const auto REALPOS          = w->m_realPosition->value() + renderOffset;
        const auto noScreenShareBox = CBox{REALPOS.x, REALPOS.y, std::max(w->m_realSize->value().x, 5.0), std::max(w->m_realSize->value().y, 5.0)}
                                          .translate(-m_monitor->m_position)
                                          .scale(m_monitor->m_scale)
                                          .translate(-m_box.pos());

        const auto dontRound     = w->isEffectiveInternalFSMode(FSMODE_FULLSCREEN) || w->m_windowData.noRounding.valueOrDefault();
        const auto rounding      = dontRound ? 0 : w->rounding() * m_monitor->m_scale;
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
                        const auto size    = surf->m_current.size;
                        const auto surfBox = CBox{popupBaseOffset + popRel + localOff, size}.translate(-m_monitor->m_position).scale(m_monitor->m_scale).translate(-m_box.pos());

                        if LIKELY (surfBox.w > 0 && surfBox.h > 0)
                            g_pHyprOpenGL->renderRect(surfBox, Colors::BLACK, {});
                    },
                    nullptr);
            },
            nullptr);
    }

    if (m_overlayCursor)
        g_pPointerManager->renderSoftwareCursorsFor(m_monitor.lock(), Time::steadyNow(), fakeDamage,
                                                    g_pInputManager->getMouseCoordsInternal() - m_monitor->m_position - m_box.pos() / m_monitor->m_scale, true);
}

void SScreenshareFrame::renderWindow() {
    const auto PMONITOR   = m_window->m_monitor.lock();
    const auto now        = Time::steadyNow();
    CRegion    fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(m_window.lock()); // block the feedback to avoid spamming the surface if it's visible
    g_pHyprRenderer->renderWindow(m_window.lock(), PMONITOR, now, false, RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    if (m_overlayCursor)
        g_pPointerManager->renderSoftwareCursorsFor(PMONITOR->m_self.lock(), now, fakeDamage, g_pInputManager->getMouseCoordsInternal() - m_window->m_realPosition->value());
}

void SScreenshareFrame::render() {
    const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(m_client, PERMISSION_TYPE_SCREENCOPY);

    if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING) {
        g_pHyprOpenGL->clear(Colors::BLACK);
        return;
    } else if (PERM == PERMISSION_RULE_ALLOW_MODE_DENY || (m_window && m_window->m_windowData.noScreenShare.valueOrDefault())) {
        g_pHyprOpenGL->clear(Colors::BLACK);
        CBox texbox = CBox{m_box.size() / 2.F, g_pHyprOpenGL->m_screencopyDeniedTexture->m_size}.translate(-g_pHyprOpenGL->m_screencopyDeniedTexture->m_size / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_screencopyDeniedTexture, texbox, {});
        return;
    }

    if (m_tempFB.isAllocated()) {
        CBox texbox = {{}, m_box.size()};
        g_pHyprOpenGL->renderTexture(m_tempFB.getTexture(), texbox, {});
        m_tempFB.release();
        return;
    }

    const auto PMONITOR = m_monitor ? m_monitor.lock() : m_window->m_monitor.lock();

    if (m_monitor)
        renderMonitor();
    else
        renderWindow();
}

void SScreenshareFrame::share() {
    if (!m_buffer || !m_monitor || m_shared)
        return;

    m_shared = m_buffer->shm().success ? copyShm() : copyDmabuf();

    if (!m_shared)
        m_callback(false);
}

bool SScreenshareFrame::copyDmabuf() {
    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(m_monitor.lock(), fakeDamage, RENDER_MODE_TO_BUFFER, m_buffer.m_buffer, nullptr, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering to dma frame");
        return false;
    }

    render();

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;

    g_pHyprRenderer->endRender([callback = m_callback]() {
        LOGM(TRACE, "Copied frame via dma");
        callback(true);
    });

    return true;
}

bool SScreenshareFrame::copyShm() {
    g_pHyprRenderer->makeEGLCurrent();

    auto shm                      = m_buffer->shm();
    auto [pixelData, fmt, bufLen] = m_buffer->beginDataPtr(0); // no need for end, cuz it's shm

    const auto PFORMAT = NFormatUtils::getPixelFormatFromDRM(shm.format);
    if (!PFORMAT) {
        LOGM(ERR, "Can't copy: failed to find a pixel format");
        return false;
    }

    const auto   PMONITOR   = m_monitor ? m_monitor.lock() : m_window->m_monitor.lock();
    CRegion      fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    CFramebuffer outFB;
    outFB.alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, PMONITOR->m_output->state->state().drmFormat);

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &outFB, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering");
        return false;
    }

    render();

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    // TODO: use pixel buffer object to not block cpu
    // now copy over pixel data to shm buffer

    auto glFormat = PFORMAT->flipRB ? GL_BGRA_EXT : GL_RGBA;

    g_pHyprOpenGL->m_renderData.pMonitor = PMONITOR;
    outFB.bind();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, outFB.getFBID());

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, m_box.width, m_box.height, glFormat, PFORMAT->glType, pixelData);

    outFB.unbind();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    g_pHyprOpenGL->m_renderData.pMonitor.reset();

    LOGM(TRACE, "Copied frame via shm");
    m_callback(true);

    return true;
}

void SScreenshareFrame::storeTempFB() {
    g_pHyprRenderer->makeEGLCurrent();

    m_tempFB.alloc(m_box.w, m_box.h);

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(m_monitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &m_tempFB, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering to temp fb");
        return;
    }

    if (m_monitor)
        renderMonitor();
    else
        renderWindow();

    g_pHyprRenderer->endRender();
}

CScreenshareManager::CScreenshareManager() {
    ;
}

void CScreenshareManager::shareNextFrame(PHLMONITOR monitor, SP<IHLBuffer> buffer, wl_client* client, bool overlayCursor, FScreenshareCallback callback) {
    if UNLIKELY (!g_pCompositor->monitorExists(monitor)) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        return;
    }

    if UNLIKELY (!buffer || buffer->size != monitor->m_pixelSize) {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
        return;
    }

    auto readFormat = g_pHyprOpenGL->getPreferredReadFormat(monitor);
    if (auto attrs = buffer->dmabuf(); attrs.success && attrs.format != readFormat.drmFormat) {
        LOGM(ERR, "Invalid dmabuf format in {:x}", (uintptr_t)this);
        return;
    } else if (auto attrs = buffer->shm(); attrs.success && attrs.format != readFormat.drmFormat) {
        LOGM(ERR, "Invalid shm format in {:x}", (uintptr_t)this);
        return;
    } else {
        LOGM(ERR, "Client requested sharing to an invalid buffer");
    }

    m_frames.emplace_back(monitor, buffer, client, overlayCursor, callback);

    g_pHyprRenderer->m_directScanoutBlocked = true;
}

void CScreenshareManager::onOutputCommit(PHLMONITOR monitor) {
    if (m_frames.empty()) {
        g_pHyprRenderer->m_directScanoutBlocked = false;
        return; // nothing to share
    }

    std::ranges::for_each(m_frames, [&](SScreenshareFrame& frame) {
        if ((!frame.m_monitor && !frame.m_window->m_monitor) || !frame.m_buffer || frame.m_monitor != monitor || frame.m_shared)
            return;

        if (frame.m_window && frame.m_window->m_monitor != monitor)
            return;

        frame.share();
    });

    std::ranges::remove_if(m_frames, [&](const SScreenshareFrame& frame) {
        return (!frame.m_monitor && !frame.m_window) || (!frame.m_monitor && !frame.m_window->m_monitor) || !frame.m_buffer || frame.m_shared;
    });
}
