#include "ToplevelExport.hpp"
#include "../Compositor.hpp"
#include "ForeignToplevelWlr.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/SeatManager.hpp"
#include "types/WLBuffer.hpp"
#include "types/Buffer.hpp"
#include "../helpers/Format.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/permissions/DynamicPermissionManager.hpp"
#include "../render/Renderer.hpp"

#include <algorithm>
#include <hyprutils/math/Vector2D.hpp>

CToplevelExportClient::CToplevelExportClient(SP<CHyprlandToplevelExportManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setCaptureToplevel([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, uint32_t handle) {
        this->captureToplevel(pMgr, frame, overlayCursor, g_pCompositor->getWindowFromHandle(handle));
    });
    m_resource->setCaptureToplevelWithWlrToplevelHandle([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* handle) {
        this->captureToplevel(pMgr, frame, overlayCursor, PROTO::foreignToplevelWlr->windowFromHandleResource(handle));
    });

    m_lastMeasure.reset();
    m_lastFrame.reset();
    m_tickCallback = g_pHookSystem->hookDynamic("tick", [&](void* self, SCallbackInfo& info, std::any data) { onTick(); });
}

void CToplevelExportClient::captureToplevel(CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor_, PHLWINDOW handle) {
    // create a frame
    const auto FRAME = PROTO::toplevelExport->m_frames.emplace_back(
        makeShared<CToplevelExportFrame>(makeShared<CHyprlandToplevelExportFrameV1>(m_resource->client(), m_resource->version(), frame), overlayCursor_, handle));

    if UNLIKELY (!FRAME->good()) {
        LOGM(ERR, "Couldn't alloc frame for sharing! (no memory)");
        m_resource->noMemory();
        PROTO::toplevelExport->destroyResource(FRAME.get());
        return;
    }

    FRAME->m_self   = FRAME;
    FRAME->m_client = m_self;
}

void CToplevelExportClient::onTick() {
    if (m_lastMeasure.getMillis() < 500)
        return;

    m_framesInLastHalfSecond = m_frameCounter;
    m_frameCounter           = 0;
    m_lastMeasure.reset();

    const auto LASTFRAMEDELTA = m_lastFrame.getMillis() / 1000.0;
    const bool FRAMEAWAITING  = std::ranges::any_of(PROTO::toplevelExport->m_frames, [&](const auto& frame) { return frame->m_client.get() == this; });

    if (m_framesInLastHalfSecond > 3 && !m_sentScreencast) {
        EMIT_HOOK_EVENT("screencast", (std::vector<uint64_t>{1, (uint64_t)m_framesInLastHalfSecond, (uint64_t)m_clientOwner}));
        g_pEventManager->postEvent(SHyprIPCEvent{"screencast", "1," + std::to_string(m_clientOwner)});
        m_sentScreencast = true;
    } else if (m_framesInLastHalfSecond < 4 && m_sentScreencast && LASTFRAMEDELTA > 1.0 && !FRAMEAWAITING) {
        EMIT_HOOK_EVENT("screencast", (std::vector<uint64_t>{0, (uint64_t)m_framesInLastHalfSecond, (uint64_t)m_clientOwner}));
        g_pEventManager->postEvent(SHyprIPCEvent{"screencast", "0," + std::to_string(m_clientOwner)});
        m_sentScreencast = false;
    }
}

bool CToplevelExportClient::good() {
    return m_resource->resource();
}

CToplevelExportFrame::CToplevelExportFrame(SP<CHyprlandToplevelExportFrameV1> resource_, int32_t overlayCursor_, PHLWINDOW pWindow_) : m_resource(resource_), m_window(pWindow_) {
    if UNLIKELY (!good())
        return;

    m_cursorOverlayRequested = !!overlayCursor_;

    if UNLIKELY (!m_window) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which does not exist!", m_window);
        m_resource->sendFailed();
        return;
    }

    if UNLIKELY (!m_window->m_isMapped) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which is not shareable!", m_window);
        m_resource->sendFailed();
        return;
    }

    m_resource->setOnDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setCopy([this](CHyprlandToplevelExportFrameV1* pFrame, wl_resource* res, int32_t ignoreDamage) { this->copy(pFrame, res, ignoreDamage); });

    const auto PMONITOR = m_window->m_monitor.lock();

    g_pHyprRenderer->makeEGLCurrent();

    m_shmFormat = g_pHyprOpenGL->getPreferredReadFormat(PMONITOR);
    if UNLIKELY (m_shmFormat == DRM_FORMAT_INVALID) {
        LOGM(ERR, "No format supported by renderer in capture toplevel");
        m_resource->sendFailed();
        return;
    }

    const auto PSHMINFO = NFormatUtils::getPixelFormatFromDRM(m_shmFormat);
    if UNLIKELY (!PSHMINFO) {
        LOGM(ERR, "No pixel format supported by renderer in capture toplevel");
        m_resource->sendFailed();
        return;
    }

    m_dmabufFormat = PMONITOR->m_output->state->state().drmFormat;

    m_box = {0, 0, (int)(m_window->m_realSize->value().x * PMONITOR->m_scale), (int)(m_window->m_realSize->value().y * PMONITOR->m_scale)};

    m_box.transform(wlTransformToHyprutils(PMONITOR->m_transform), PMONITOR->m_transformedSize.x, PMONITOR->m_transformedSize.y).round();

    m_shmStride = NFormatUtils::minStride(PSHMINFO, m_box.w);

    m_resource->sendBuffer(NFormatUtils::drmToShm(m_shmFormat), m_box.width, m_box.height, m_shmStride);

    if LIKELY (m_dmabufFormat != DRM_FORMAT_INVALID)
        m_resource->sendLinuxDmabuf(m_dmabufFormat, m_box.width, m_box.height);

    m_resource->sendBufferDone();
}

void CToplevelExportFrame::copy(CHyprlandToplevelExportFrameV1* pFrame, wl_resource* buffer_, int32_t ignoreDamage) {
    if UNLIKELY (!good()) {
        LOGM(ERR, "No frame in copyFrame??");
        return;
    }

    if UNLIKELY (!validMapped(m_window)) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which is gone!", m_window);
        m_resource->sendFailed();
        return;
    }

    if UNLIKELY (!m_window->m_isMapped) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which is not shareable (2)!", m_window);
        m_resource->sendFailed();
        return;
    }

    const auto PBUFFER = CWLBufferResource::fromResource(buffer_);
    if UNLIKELY (!PBUFFER) {
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    if UNLIKELY (PBUFFER->m_buffer->size != m_box.size()) {
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer dimensions");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    if UNLIKELY (m_buffer) {
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    if (auto attrs = PBUFFER->m_buffer->dmabuf(); attrs.success) {
        m_bufferDMA = true;

        if (attrs.format != m_dmabufFormat) {
            m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::toplevelExport->destroyResource(this);
            return;
        }
    } else if (auto attrs = PBUFFER->m_buffer->shm(); attrs.success) {
        if (attrs.format != m_shmFormat) {
            m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::toplevelExport->destroyResource(this);
            return;
        } else if ((int)attrs.stride != m_shmStride) {
            m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer stride");
            PROTO::toplevelExport->destroyResource(this);
            return;
        }
    } else {
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer type");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    m_buffer = CHLBufferReference(PBUFFER->m_buffer.lock());

    m_ignoreDamage = ignoreDamage;

    if (ignoreDamage && validMapped(m_window))
        share();
    else
        PROTO::toplevelExport->m_framesAwaitingWrite.emplace_back(m_self);
}

void CToplevelExportFrame::share() {
    if (!m_buffer || !validMapped(m_window))
        return;

    if (m_bufferDMA) {
        if (!copyDmabuf(Time::steadyNow())) {
            m_resource->sendFailed();
            return;
        }
    } else {
        if (!copyShm(Time::steadyNow())) {
            m_resource->sendFailed();
            return;
        }
    }

    m_resource->sendFlags((hyprlandToplevelExportFrameV1Flags)0);

    if (!m_ignoreDamage)
        m_resource->sendDamage(0, 0, m_box.width, m_box.height);

    const auto [sec, nsec] = Time::secNsec(Time::steadyNow());

    uint32_t tvSecHi = (sizeof(sec) > 4) ? sec >> 32 : 0;
    uint32_t tvSecLo = sec & 0xFFFFFFFF;
    m_resource->sendReady(tvSecHi, tvSecLo, nsec);
}

bool CToplevelExportFrame::copyShm(const Time::steady_tp& now) {
    const auto PERM               = g_pDynamicPermissionManager->clientPermissionMode(m_resource->client(), PERMISSION_TYPE_SCREENCOPY);
    auto       shm                = m_buffer->shm();
    auto [pixelData, fmt, bufLen] = m_buffer->beginDataPtr(0); // no need for end, cuz it's shm

    // render the client
    const auto PMONITOR = m_window->m_monitor.lock();
    CRegion    fakeDamage{0, 0, PMONITOR->m_pixelSize.x * 10, PMONITOR->m_pixelSize.y * 10};

    g_pHyprRenderer->makeEGLCurrent();

    CFramebuffer outFB;
    outFB.alloc(PMONITOR->m_pixelSize.x, PMONITOR->m_pixelSize.y, PMONITOR->m_output->state->state().drmFormat);

    auto overlayCursor = shouldOverlayCursor();

    if (overlayCursor) {
        g_pPointerManager->lockSoftwareForMonitor(PMONITOR->m_self.lock());
        g_pPointerManager->damageCursor(PMONITOR->m_self.lock());
    }

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &outFB))
        return false;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 1.0));

    // render client at 0,0
    if (PERM == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(m_window); // block the feedback to avoid spamming the surface if it's visible
        g_pHyprRenderer->renderWindow(m_window, PMONITOR, now, false, RENDER_PASS_ALL, true, true);
        g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

        if (overlayCursor)
            g_pPointerManager->renderSoftwareCursorsFor(PMONITOR->m_self.lock(), now, fakeDamage, g_pInputManager->getMouseCoordsInternal() - m_window->m_realPosition->value());
    } else if (PERM == PERMISSION_RULE_ALLOW_MODE_DENY) {
        CBox texbox =
            CBox{PMONITOR->m_transformedSize / 2.F, g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize}.translate(-g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pScreencopyDeniedTexture, texbox, 1);
    }

    const auto PFORMAT = NFormatUtils::getPixelFormatFromDRM(shm.format);
    if (!PFORMAT) {
        g_pHyprRenderer->endRender();
        return false;
    }

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_RenderData.pMonitor = PMONITOR;
    outFB.bind();

#ifndef GLES2
    glBindFramebuffer(GL_READ_FRAMEBUFFER, outFB.getFBID());
#endif

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    auto glFormat = PFORMAT->flipRB ? GL_BGRA_EXT : GL_RGBA;

    auto origin = Vector2D(0, 0);
    switch (PMONITOR->m_transform) {
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
        case WL_OUTPUT_TRANSFORM_90: {
            origin.y = PMONITOR->m_pixelSize.y - m_box.height;
            break;
        }
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
        case WL_OUTPUT_TRANSFORM_180: {
            origin.x = PMONITOR->m_pixelSize.x - m_box.width;
            origin.y = PMONITOR->m_pixelSize.y - m_box.height;
            break;
        }
        case WL_OUTPUT_TRANSFORM_FLIPPED:
        case WL_OUTPUT_TRANSFORM_270: {
            origin.x = PMONITOR->m_pixelSize.x - m_box.width;
            break;
        }
        default: break;
    }

    glReadPixels(origin.x, origin.y, m_box.width, m_box.height, glFormat, PFORMAT->glType, pixelData);

    if (overlayCursor) {
        g_pPointerManager->unlockSoftwareForMonitor(PMONITOR->m_self.lock());
        g_pPointerManager->damageCursor(PMONITOR->m_self.lock());
    }

    outFB.unbind();

#ifndef GLES2
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
#endif

    return true;
}

bool CToplevelExportFrame::copyDmabuf(const Time::steady_tp& now) {
    const auto PERM     = g_pDynamicPermissionManager->clientPermissionMode(m_resource->client(), PERMISSION_TYPE_SCREENCOPY);
    const auto PMONITOR = m_window->m_monitor.lock();

    CRegion    fakeDamage{0, 0, INT16_MAX, INT16_MAX};

    auto       overlayCursor = shouldOverlayCursor();

    if (overlayCursor) {
        g_pPointerManager->lockSoftwareForMonitor(PMONITOR->m_self.lock());
        g_pPointerManager->damageCursor(PMONITOR->m_self.lock());
    }

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_TO_BUFFER, m_buffer.m_buffer))
        return false;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 1.0));
    if (PERM == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(m_window); // block the feedback to avoid spamming the surface if it's visible
        g_pHyprRenderer->renderWindow(m_window, PMONITOR, now, false, RENDER_PASS_ALL, true, true);
        g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

        if (overlayCursor)
            g_pPointerManager->renderSoftwareCursorsFor(PMONITOR->m_self.lock(), now, fakeDamage, g_pInputManager->getMouseCoordsInternal() - m_window->m_realPosition->value());
    } else if (PERM == PERMISSION_RULE_ALLOW_MODE_DENY) {
        CBox texbox =
            CBox{PMONITOR->m_transformedSize / 2.F, g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize}.translate(-g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pScreencopyDeniedTexture, texbox, 1);
    }

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    if (overlayCursor) {
        g_pPointerManager->unlockSoftwareForMonitor(PMONITOR->m_self.lock());
        g_pPointerManager->damageCursor(PMONITOR->m_self.lock());
    }

    return true;
}

bool CToplevelExportFrame::shouldOverlayCursor() const {
    if (!m_cursorOverlayRequested)
        return false;

    auto pointerSurfaceResource = g_pSeatManager->m_state.pointerFocus.lock();

    if (!pointerSurfaceResource)
        return false;

    auto pointerSurface = CWLSurface::fromResource(pointerSurfaceResource);

    return pointerSurface && pointerSurface->getWindow() == m_window;
}

bool CToplevelExportFrame::good() {
    return m_resource->resource();
}

CToplevelExportProtocol::CToplevelExportProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CToplevelExportProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_clients.emplace_back(makeShared<CToplevelExportClient>(makeShared<CHyprlandToplevelExportManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        LOGM(LOG, "Failed to bind client! (out of memory)");
        wl_client_post_no_memory(client);
        m_clients.pop_back();
        return;
    }

    CLIENT->m_self = CLIENT;

    LOGM(LOG, "Bound client successfully!");
}

void CToplevelExportProtocol::destroyResource(CToplevelExportClient* client) {
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
    std::erase_if(m_frames, [&](const auto& other) { return other->m_client.get() == client; });
    std::erase_if(m_framesAwaitingWrite, [&](const auto& other) { return !other || other->m_client.get() == client; });
}

void CToplevelExportProtocol::destroyResource(CToplevelExportFrame* frame) {
    std::erase_if(m_frames, [&](const auto& other) { return other.get() == frame; });
    std::erase_if(m_framesAwaitingWrite, [&](const auto& other) { return !other || other.get() == frame; });
}

void CToplevelExportProtocol::onOutputCommit(PHLMONITOR pMonitor) {
    if (m_framesAwaitingWrite.empty())
        return; // nothing to share

    std::vector<WP<CToplevelExportFrame>> framesToRemove;
    // reserve number of elements to avoid reallocations
    framesToRemove.reserve(m_framesAwaitingWrite.size());

    // share frame if correct output
    for (auto const& f : m_framesAwaitingWrite) {
        if (!f)
            continue;

        // check permissions
        const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(f->m_resource->client(), PERMISSION_TYPE_SCREENCOPY);

        if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING)
            continue; // pending an answer, don't do anything yet.

        if (!validMapped(f->m_window)) {
            framesToRemove.emplace_back(f);
            continue;
        }

        if (!f->m_window)
            continue;

        const auto PWINDOW = f->m_window;

        if (pMonitor != PWINDOW->m_monitor.lock())
            continue;

        CBox geometry = {PWINDOW->m_realPosition->value().x, PWINDOW->m_realPosition->value().y, PWINDOW->m_realSize->value().x, PWINDOW->m_realSize->value().y};

        if (geometry.intersection({pMonitor->m_position, pMonitor->m_size}).empty())
            continue;

        f->share();

        f->m_client->m_lastFrame.reset();
        ++f->m_client->m_frameCounter;

        framesToRemove.push_back(f);
    }

    for (auto const& f : framesToRemove) {
        std::erase(m_framesAwaitingWrite, f);
    }
}

void CToplevelExportProtocol::onWindowUnmap(PHLWINDOW pWindow) {
    for (auto const& f : m_frames) {
        if (f->m_window == pWindow)
            f->m_window.reset();
    }
}
