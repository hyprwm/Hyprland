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
#include "../render/Renderer.hpp"

#include <algorithm>

CToplevelExportClient::CToplevelExportClient(SP<CHyprlandToplevelExportManagerV1> resource_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    resource->setOnDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) { PROTO::toplevelExport->destroyResource(this); });
    resource->setDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) { PROTO::toplevelExport->destroyResource(this); });
    resource->setCaptureToplevel([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, uint32_t handle) {
        this->captureToplevel(pMgr, frame, overlayCursor, g_pCompositor->getWindowFromHandle(handle));
    });
    resource->setCaptureToplevelWithWlrToplevelHandle([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* handle) {
        this->captureToplevel(pMgr, frame, overlayCursor, PROTO::foreignToplevelWlr->windowFromHandleResource(handle));
    });

    lastMeasure.reset();
    lastFrame.reset();
    tickCallback = g_pHookSystem->hookDynamic("tick", [&](void* self, SCallbackInfo& info, std::any data) { onTick(); });
}

void CToplevelExportClient::captureToplevel(CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor_, PHLWINDOW handle) {
    // create a frame
    const auto FRAME = PROTO::toplevelExport->m_vFrames.emplace_back(
        makeShared<CToplevelExportFrame>(makeShared<CHyprlandToplevelExportFrameV1>(resource->client(), resource->version(), frame), overlayCursor_, handle));

    if UNLIKELY (!FRAME->good()) {
        LOGM(ERR, "Couldn't alloc frame for sharing! (no memory)");
        resource->noMemory();
        PROTO::toplevelExport->destroyResource(FRAME.get());
        return;
    }

    FRAME->self   = FRAME;
    FRAME->client = self;
}

void CToplevelExportClient::onTick() {
    if (lastMeasure.getMillis() < 500)
        return;

    framesInLastHalfSecond = frameCounter;
    frameCounter           = 0;
    lastMeasure.reset();

    const auto LASTFRAMEDELTA = lastFrame.getMillis() / 1000.0;
    const bool FRAMEAWAITING  = std::ranges::any_of(PROTO::toplevelExport->m_vFrames, [&](const auto& frame) { return frame->client.get() == this; });

    if (framesInLastHalfSecond > 3 && !sentScreencast) {
        EMIT_HOOK_EVENT("screencast", (std::vector<uint64_t>{1, (uint64_t)framesInLastHalfSecond, (uint64_t)clientOwner}));
        g_pEventManager->postEvent(SHyprIPCEvent{"screencast", "1," + std::to_string(clientOwner)});
        sentScreencast = true;
    } else if (framesInLastHalfSecond < 4 && sentScreencast && LASTFRAMEDELTA > 1.0 && !FRAMEAWAITING) {
        EMIT_HOOK_EVENT("screencast", (std::vector<uint64_t>{0, (uint64_t)framesInLastHalfSecond, (uint64_t)clientOwner}));
        g_pEventManager->postEvent(SHyprIPCEvent{"screencast", "0," + std::to_string(clientOwner)});
        sentScreencast = false;
    }
}

bool CToplevelExportClient::good() {
    return resource->resource();
}

CToplevelExportFrame::~CToplevelExportFrame() {
    if (buffer && buffer->locked())
        buffer->unlock();
}

CToplevelExportFrame::CToplevelExportFrame(SP<CHyprlandToplevelExportFrameV1> resource_, int32_t overlayCursor_, PHLWINDOW pWindow_) : resource(resource_), pWindow(pWindow_) {
    if UNLIKELY (!good())
        return;

    cursorOverlayRequested = !!overlayCursor_;

    if UNLIKELY (!pWindow) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which does not exist!", pWindow);
        resource->sendFailed();
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    if UNLIKELY (!pWindow->m_bIsMapped) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which is not shareable!", pWindow);
        resource->sendFailed();
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    resource->setOnDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    resource->setDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    resource->setCopy([this](CHyprlandToplevelExportFrameV1* pFrame, wl_resource* res, int32_t ignoreDamage) { this->copy(pFrame, res, ignoreDamage); });

    const auto PMONITOR = pWindow->m_pMonitor.lock();

    g_pHyprRenderer->makeEGLCurrent();

    shmFormat = g_pHyprOpenGL->getPreferredReadFormat(PMONITOR);
    if UNLIKELY (shmFormat == DRM_FORMAT_INVALID) {
        LOGM(ERR, "No format supported by renderer in capture toplevel");
        resource->sendFailed();
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    const auto PSHMINFO = NFormatUtils::getPixelFormatFromDRM(shmFormat);
    if UNLIKELY (!PSHMINFO) {
        LOGM(ERR, "No pixel format supported by renderer in capture toplevel");
        resource->sendFailed();
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    dmabufFormat = PMONITOR->output->state->state().drmFormat;

    box = {0, 0, (int)(pWindow->m_vRealSize->value().x * PMONITOR->scale), (int)(pWindow->m_vRealSize->value().y * PMONITOR->scale)};

    box.transform(wlTransformToHyprutils(PMONITOR->transform), PMONITOR->vecTransformedSize.x, PMONITOR->vecTransformedSize.y).round();

    shmStride = NFormatUtils::minStride(PSHMINFO, box.w);

    resource->sendBuffer(NFormatUtils::drmToShm(shmFormat), box.width, box.height, shmStride);

    if LIKELY (dmabufFormat != DRM_FORMAT_INVALID)
        resource->sendLinuxDmabuf(dmabufFormat, box.width, box.height);

    resource->sendBufferDone();
}

void CToplevelExportFrame::copy(CHyprlandToplevelExportFrameV1* pFrame, wl_resource* buffer_, int32_t ignoreDamage) {
    if UNLIKELY (!good()) {
        LOGM(ERR, "No frame in copyFrame??");
        return;
    }

    if UNLIKELY (!validMapped(pWindow)) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which is gone!", pWindow);
        resource->sendFailed();
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    if UNLIKELY (!pWindow->m_bIsMapped) {
        LOGM(ERR, "Client requested sharing of window handle {:x} which is not shareable (2)!", pWindow);
        resource->sendFailed();
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    const auto PBUFFER = CWLBufferResource::fromResource(buffer_);
    if UNLIKELY (!PBUFFER) {
        resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    PBUFFER->buffer->lock();

    if UNLIKELY (PBUFFER->buffer->size != box.size()) {
        resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer dimensions");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    if UNLIKELY (buffer) {
        resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    if (auto attrs = PBUFFER->buffer->dmabuf(); attrs.success) {
        bufferDMA = true;

        if (attrs.format != dmabufFormat) {
            resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::toplevelExport->destroyResource(this);
            return;
        }
    } else if (auto attrs = PBUFFER->buffer->shm(); attrs.success) {
        if (attrs.format != shmFormat) {
            resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::toplevelExport->destroyResource(this);
            return;
        } else if ((int)attrs.stride != shmStride) {
            resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer stride");
            PROTO::toplevelExport->destroyResource(this);
            return;
        }
    } else {
        resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer type");
        PROTO::toplevelExport->destroyResource(this);
        return;
    }

    buffer = PBUFFER->buffer;

    m_ignoreDamage = ignoreDamage;

    if (ignoreDamage && validMapped(pWindow))
        share();
    else
        PROTO::toplevelExport->m_vFramesAwaitingWrite.emplace_back(self);
}

void CToplevelExportFrame::share() {
    if (!buffer || !validMapped(pWindow))
        return;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (bufferDMA) {
        if (!copyDmabuf(&now)) {
            resource->sendFailed();
            return;
        }
    } else {
        if (!copyShm(&now)) {
            resource->sendFailed();
            return;
        }
    }

    resource->sendFlags((hyprlandToplevelExportFrameV1Flags)0);

    if (!m_ignoreDamage) {
        resource->sendDamage(0, 0, box.width, box.height);
    }

    uint32_t tvSecHi = (sizeof(now.tv_sec) > 4) ? now.tv_sec >> 32 : 0;
    uint32_t tvSecLo = now.tv_sec & 0xFFFFFFFF;
    resource->sendReady(tvSecHi, tvSecLo, now.tv_nsec);
}

bool CToplevelExportFrame::copyShm(timespec* now) {
    auto shm                      = buffer->shm();
    auto [pixelData, fmt, bufLen] = buffer->beginDataPtr(0); // no need for end, cuz it's shm

    // render the client
    const auto PMONITOR = pWindow->m_pMonitor.lock();
    CRegion    fakeDamage{0, 0, PMONITOR->vecPixelSize.x * 10, PMONITOR->vecPixelSize.y * 10};

    g_pHyprRenderer->makeEGLCurrent();

    CFramebuffer outFB;
    outFB.alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, PMONITOR->output->state->state().drmFormat);

    auto overlayCursor = shouldOverlayCursor();

    if (overlayCursor) {
        g_pPointerManager->lockSoftwareForMonitor(PMONITOR->self.lock());
        g_pPointerManager->damageCursor(PMONITOR->self.lock());
    }

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &outFB))
        return false;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 1.0));

    // render client at 0,0
    g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(pWindow); // block the feedback to avoid spamming the surface if it's visible
    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, now, false, RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    if (overlayCursor)
        g_pPointerManager->renderSoftwareCursorsFor(PMONITOR->self.lock(), now, fakeDamage, g_pInputManager->getMouseCoordsInternal() - pWindow->m_vRealPosition->value());

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
    glReadPixels(0, 0, box.width, box.height, glFormat, PFORMAT->glType, pixelData);

    if (overlayCursor) {
        g_pPointerManager->unlockSoftwareForMonitor(PMONITOR->self.lock());
        g_pPointerManager->damageCursor(PMONITOR->self.lock());
    }

    return true;
}

bool CToplevelExportFrame::copyDmabuf(timespec* now) {
    const auto PMONITOR = pWindow->m_pMonitor.lock();

    CRegion    fakeDamage{0, 0, INT16_MAX, INT16_MAX};

    auto       overlayCursor = shouldOverlayCursor();

    if (overlayCursor) {
        g_pPointerManager->lockSoftwareForMonitor(PMONITOR->self.lock());
        g_pPointerManager->damageCursor(PMONITOR->self.lock());
    }

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_TO_BUFFER, buffer.lock()))
        return false;

    g_pHyprOpenGL->clear(CHyprColor(0, 0, 0, 1.0));

    g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(pWindow); // block the feedback to avoid spamming the surface if it's visible
    g_pHyprRenderer->renderWindow(pWindow, PMONITOR, now, false, RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    if (overlayCursor)
        g_pPointerManager->renderSoftwareCursorsFor(PMONITOR->self.lock(), now, fakeDamage, g_pInputManager->getMouseCoordsInternal() - pWindow->m_vRealPosition->value());

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    if (overlayCursor) {
        g_pPointerManager->unlockSoftwareForMonitor(PMONITOR->self.lock());
        g_pPointerManager->damageCursor(PMONITOR->self.lock());
    }

    return true;
}

bool CToplevelExportFrame::shouldOverlayCursor() const {
    if (!cursorOverlayRequested)
        return false;

    auto pointerSurfaceResource = g_pSeatManager->state.pointerFocus.lock();

    if (!pointerSurfaceResource)
        return false;

    auto pointerSurface = CWLSurface::fromResource(pointerSurfaceResource);

    return pointerSurface && pointerSurface->getWindow() == pWindow;
}

bool CToplevelExportFrame::good() {
    return resource->resource();
}

CToplevelExportProtocol::CToplevelExportProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CToplevelExportProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_vClients.emplace_back(makeShared<CToplevelExportClient>(makeShared<CHyprlandToplevelExportManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        LOGM(LOG, "Failed to bind client! (out of memory)");
        wl_client_post_no_memory(client);
        m_vClients.pop_back();
        return;
    }

    CLIENT->self = CLIENT;

    LOGM(LOG, "Bound client successfully!");
}

void CToplevelExportProtocol::destroyResource(CToplevelExportClient* client) {
    std::erase_if(m_vClients, [&](const auto& other) { return other.get() == client; });
    std::erase_if(m_vFrames, [&](const auto& other) { return other->client.get() == client; });
    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return !other || other->client.get() == client; });
}

void CToplevelExportProtocol::destroyResource(CToplevelExportFrame* frame) {
    std::erase_if(m_vFrames, [&](const auto& other) { return other.get() == frame; });
    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return !other || other.get() == frame; });
}

void CToplevelExportProtocol::onOutputCommit(PHLMONITOR pMonitor) {
    if (m_vFramesAwaitingWrite.empty())
        return; // nothing to share

    std::vector<WP<CToplevelExportFrame>> framesToRemove;

    // share frame if correct output
    for (auto const& f : m_vFramesAwaitingWrite) {
        if (!f)
            continue;

        if (!validMapped(f->pWindow)) {
            framesToRemove.emplace_back(f);
            continue;
        }

        if (!f->pWindow)
            continue;

        const auto PWINDOW = f->pWindow;

        if (pMonitor != PWINDOW->m_pMonitor.lock())
            continue;

        CBox geometry = {PWINDOW->m_vRealPosition->value().x, PWINDOW->m_vRealPosition->value().y, PWINDOW->m_vRealSize->value().x, PWINDOW->m_vRealSize->value().y};

        if (geometry.intersection({pMonitor->vecPosition, pMonitor->vecSize}).empty())
            continue;

        f->share();

        f->client->lastFrame.reset();
        ++f->client->frameCounter;

        framesToRemove.push_back(f);
    }

    for (auto const& f : framesToRemove) {
        std::erase(m_vFramesAwaitingWrite, f);
    }
}

void CToplevelExportProtocol::onWindowUnmap(PHLWINDOW pWindow) {
    for (auto const& f : m_vFrames) {
        if (f->pWindow == pWindow)
            f->pWindow.reset();
    }
}
