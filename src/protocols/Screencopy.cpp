#include "Screencopy.hpp"
#include "../Compositor.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/PointerManager.hpp"
#include "core/Output.hpp"
#include "types/WLBuffer.hpp"
#include "types/Buffer.hpp"
#include "../helpers/Format.hpp"

#include <algorithm>

CScreencopyFrame::~CScreencopyFrame() {
    if (buffer && buffer->locked())
        buffer->unlock();
}

CScreencopyFrame::CScreencopyFrame(SP<CZwlrScreencopyFrameV1> resource_, int32_t overlay_cursor, wl_resource* output, CBox box_) : resource(resource_) {
    if (!good())
        return;

    overlayCursor = !!overlay_cursor;
    pMonitor      = CWLOutputResource::fromResource(output)->monitor.get();

    if (!pMonitor) {
        LOGM(ERR, "Client requested sharing of a monitor that doesnt exist");
        resource->sendFailed();
        PROTO::screencopy->destroyResource(this);
        return;
    }

    resource->setOnDestroy([this](CZwlrScreencopyFrameV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    resource->setDestroy([this](CZwlrScreencopyFrameV1* pFrame) { PROTO::screencopy->destroyResource(this); });
    resource->setCopy([this](CZwlrScreencopyFrameV1* pFrame, wl_resource* res) { this->copy(pFrame, res); });
    resource->setCopyWithDamage([this](CZwlrScreencopyFrameV1* pFrame, wl_resource* res) {
        withDamage = true;
        this->copy(pFrame, res);
    });

    g_pHyprRenderer->makeEGLCurrent();

    if (g_pHyprOpenGL->m_mMonitorRenderResources.contains(pMonitor)) {
        const auto& RDATA = g_pHyprOpenGL->m_mMonitorRenderResources.at(pMonitor);
        // bind the fb for its format. Suppress gl errors.
#ifndef GLES2
        glBindFramebuffer(GL_READ_FRAMEBUFFER, RDATA.offloadFB.m_iFb);
#else
        glBindFramebuffer(GL_FRAMEBUFFER, RDATA.offloadFB.m_iFb);
#endif
    } else
        LOGM(ERR, "No RDATA in screencopy???");

    shmFormat = g_pHyprOpenGL->getPreferredReadFormat(pMonitor);
    if (shmFormat == DRM_FORMAT_INVALID) {
        LOGM(ERR, "No format supported by renderer in capture output");
        resource->sendFailed();
        PROTO::screencopy->destroyResource(this);
        return;
    }

    const auto PSHMINFO = FormatUtils::getPixelFormatFromDRM(shmFormat);
    if (!PSHMINFO) {
        LOGM(ERR, "No pixel format supported by renderer in capture output");
        resource->sendFailed();
        PROTO::screencopy->destroyResource(this);
        return;
    }

    dmabufFormat = pMonitor->output->state->state().drmFormat;

    if (box_.width == 0 && box_.height == 0)
        box = {0, 0, (int)(pMonitor->vecSize.x), (int)(pMonitor->vecSize.y)};
    else {
        box = box_;
    }

    box.transform(wlTransformToHyprutils(pMonitor->transform), pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y).scale(pMonitor->scale).round();

    shmStride = FormatUtils::minStride(PSHMINFO, box.w);

    resource->sendBuffer(FormatUtils::drmToShm(shmFormat), box.width, box.height, shmStride);

    if (resource->version() >= 3) {
        if (dmabufFormat != DRM_FORMAT_INVALID) {
            resource->sendLinuxDmabuf(dmabufFormat, box.width, box.height);
        }

        resource->sendBufferDone();
    }
}

void CScreencopyFrame::copy(CZwlrScreencopyFrameV1* pFrame, wl_resource* buffer_) {
    if (!good()) {
        LOGM(ERR, "No frame in copyFrame??");
        return;
    }

    if (!g_pCompositor->monitorExists(pMonitor)) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        resource->sendFailed();
        PROTO::screencopy->destroyResource(this);
        return;
    }

    const auto PBUFFER = CWLBufferResource::fromResource(buffer_);
    if (!PBUFFER) {
        LOGM(ERR, "Invalid buffer in {:x}", (uintptr_t)this);
        resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    PBUFFER->buffer->lock();

    if (PBUFFER->buffer->size != box.size()) {
        LOGM(ERR, "Invalid dimensions in {:x}", (uintptr_t)this);
        resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer dimensions");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    if (buffer) {
        LOGM(ERR, "Buffer used in {:x}", (uintptr_t)this);
        resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    if (auto attrs = PBUFFER->buffer->dmabuf(); attrs.success) {
        bufferDMA = true;

        if (attrs.format != dmabufFormat) {
            LOGM(ERR, "Invalid buffer dma format in {:x}", (uintptr_t)pFrame);
            resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::screencopy->destroyResource(this);
            return;
        }
    } else if (auto attrs = PBUFFER->buffer->shm(); attrs.success) {
        if (attrs.format != shmFormat) {
            LOGM(ERR, "Invalid buffer shm format in {:x}", (uintptr_t)pFrame);
            resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::screencopy->destroyResource(this);
            return;
        } else if ((int)attrs.stride != shmStride) {
            LOGM(ERR, "Invalid buffer shm stride in {:x}", (uintptr_t)pFrame);
            resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer stride");
            PROTO::screencopy->destroyResource(this);
            return;
        }
    } else {
        LOGM(ERR, "Invalid buffer type in {:x}", (uintptr_t)pFrame);
        resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer type");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    buffer = PBUFFER->buffer;

    PROTO::screencopy->m_vFramesAwaitingWrite.emplace_back(self);

    g_pHyprRenderer->m_bDirectScanoutBlocked = true;
    if (overlayCursor && !lockedSWCursors) {
        lockedSWCursors = true;
        // TODO: make it per-monitor
        if (!PROTO::screencopy->m_bTimerArmed) {
            for (auto& m : g_pCompositor->m_vMonitors) {
                g_pPointerManager->lockSoftwareForMonitor(m);
            }
            PROTO::screencopy->m_bTimerArmed = true;
            LOGM(LOG, "Locking sw cursors due to screensharing");
        }
        PROTO::screencopy->m_pSoftwareCursorTimer->updateTimeout(std::chrono::seconds(1));
    }

    if (!withDamage)
        g_pHyprRenderer->damageMonitor(pMonitor);
}

void CScreencopyFrame::share() {
    if (!buffer || !pMonitor)
        return;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (bufferDMA) {
        if (!copyDmabuf()) {
            LOGM(ERR, "Dmabuf copy failed in {:x}", (uintptr_t)this);
            resource->sendFailed();
            return;
        }
    } else {
        if (!copyShm()) {
            LOGM(ERR, "Shm copy failed in {:x}", (uintptr_t)this);
            resource->sendFailed();
            return;
        }
    }

    resource->sendFlags((zwlrScreencopyFrameV1Flags)0);
    if (withDamage) {
        // TODO: add a damage ring for this.
        resource->sendDamage(0, 0, buffer->size.x, buffer->size.y);
    }

    uint32_t tvSecHi = (sizeof(now.tv_sec) > 4) ? now.tv_sec >> 32 : 0;
    uint32_t tvSecLo = now.tv_sec & 0xFFFFFFFF;
    resource->sendReady(tvSecHi, tvSecLo, now.tv_nsec);
}

bool CScreencopyFrame::copyDmabuf() {
    auto    TEXTURE = makeShared<CTexture>(pMonitor->output->state->state().buffer);

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(pMonitor, fakeDamage, RENDER_MODE_TO_BUFFER, buffer.lock(), nullptr, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering to dma frame");
        return false;
    }

    CBox monbox = CBox{0, 0, pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y}
                      .translate({-box.x, -box.y}) // vvvv kinda ass-backwards but that's how I designed the renderer... sigh.
                      .transform(wlTransformToHyprutils(invertTransform(pMonitor->transform)), pMonitor->vecPixelSize.x, pMonitor->vecPixelSize.y);
    g_pHyprOpenGL->setMonitorTransformEnabled(true);
    g_pHyprOpenGL->setRenderModifEnabled(false);
    g_pHyprOpenGL->renderTexture(TEXTURE, &monbox, 1);
    g_pHyprOpenGL->setRenderModifEnabled(true);
    g_pHyprOpenGL->setMonitorTransformEnabled(false);

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    LOGM(TRACE, "Copied frame via dma");

    return true;
}

bool CScreencopyFrame::copyShm() {
    auto TEXTURE = makeShared<CTexture>(pMonitor->output->state->state().buffer);

    auto shm                      = buffer->shm();
    auto [pixelData, fmt, bufLen] = buffer->beginDataPtr(0); // no need for end, cuz it's shm

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->makeEGLCurrent();

    CFramebuffer fb;
    fb.alloc(box.w, box.h, g_pHyprRenderer->isNvidia() ? DRM_FORMAT_XBGR8888 : pMonitor->output->state->state().drmFormat);

    if (!g_pHyprRenderer->beginRender(pMonitor, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &fb, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering");
        return false;
    }

    CBox monbox = CBox{0, 0, pMonitor->vecTransformedSize.x, pMonitor->vecTransformedSize.y}.translate({-box.x, -box.y});
    g_pHyprOpenGL->setMonitorTransformEnabled(true);
    g_pHyprOpenGL->setRenderModifEnabled(false);
    g_pHyprOpenGL->renderTexture(TEXTURE, &monbox, 1);
    g_pHyprOpenGL->setRenderModifEnabled(true);
    g_pHyprOpenGL->setMonitorTransformEnabled(false);

#ifndef GLES2
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.m_iFb);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, fb.m_iFb);
#endif

    const auto PFORMAT = FormatUtils::getPixelFormatFromDRM(shm.format);
    if (!PFORMAT) {
        LOGM(ERR, "Can't copy: failed to find a pixel format");
        g_pHyprRenderer->endRender();
        return false;
    }

    auto glFormat = PFORMAT->flipRB ? GL_BGRA_EXT : GL_RGBA;

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_RenderData.pMonitor = pMonitor;
    fb.bind();

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    const auto drmFmt     = FormatUtils::getPixelFormatFromDRM(shm.format);
    uint32_t   packStride = FormatUtils::minStride(drmFmt, box.w);

    if (packStride == (uint32_t)shm.stride) {
        glReadPixels(0, 0, box.w, box.h, glFormat, PFORMAT->glType, pixelData);
    } else {
        for (size_t i = 0; i < box.h; ++i) {
            uint32_t y = i;
            glReadPixels(0, y, box.w, 1, glFormat, PFORMAT->glType, ((unsigned char*)pixelData) + i * shm.stride);
        }
    }

    g_pHyprOpenGL->m_RenderData.pMonitor = nullptr;

    LOGM(TRACE, "Copied frame via shm");

    return true;
}

bool CScreencopyFrame::good() {
    return resource->resource();
}

CScreencopyClient::~CScreencopyClient() {
    g_pHookSystem->unhook(tickCallback);
}

CScreencopyClient::CScreencopyClient(SP<CZwlrScreencopyManagerV1> resource_) : resource(resource_) {
    if (!good())
        return;

    resource->setDestroy([this](CZwlrScreencopyManagerV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    resource->setOnDestroy([this](CZwlrScreencopyManagerV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    resource->setCaptureOutput(
        [this](CZwlrScreencopyManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* output) { this->captureOutput(frame, overlayCursor, output, {}); });
    resource->setCaptureOutputRegion([this](CZwlrScreencopyManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* output, int32_t x, int32_t y, int32_t w,
                                            int32_t h) { this->captureOutput(frame, overlayCursor, output, {x, y, w, h}); });

    lastMeasure.reset();
    lastFrame.reset();
    tickCallback = g_pHookSystem->hookDynamic("tick", [&](void* self, SCallbackInfo& info, std::any data) { onTick(); });
}

void CScreencopyClient::captureOutput(uint32_t frame, int32_t overlayCursor_, wl_resource* output, CBox box) {
    const auto FRAME = PROTO::screencopy->m_vFrames.emplace_back(
        makeShared<CScreencopyFrame>(makeShared<CZwlrScreencopyFrameV1>(resource->client(), resource->version(), frame), overlayCursor_, output, box));

    if (!FRAME->good()) {
        LOGM(ERR, "Couldn't alloc frame for sharing! (no memory)");
        resource->noMemory();
        PROTO::screencopy->destroyResource(FRAME.get());
        return;
    }

    FRAME->self   = FRAME;
    FRAME->client = self;
}

void CScreencopyClient::onTick() {
    if (lastMeasure.getMillis() < 500)
        return;

    framesInLastHalfSecond = frameCounter;
    frameCounter           = 0;
    lastMeasure.reset();

    const auto LASTFRAMEDELTA = lastFrame.getMillis() / 1000.0;
    const bool FRAMEAWAITING  = std::ranges::any_of(PROTO::screencopy->m_vFrames, [&](const auto& frame) { return frame->client.get() == this; });

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

bool CScreencopyClient::good() {
    return resource->resource();
}

CScreencopyProtocol::CScreencopyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    m_pSoftwareCursorTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            // TODO: make it per-monitor
            for (auto& m : g_pCompositor->m_vMonitors) {
                g_pPointerManager->unlockSoftwareForMonitor(m);
            }
            m_bTimerArmed = false;

            LOGM(LOG, "Releasing software cursor lock");
        },
        nullptr);
    g_pEventLoopManager->addTimer(m_pSoftwareCursorTimer);
}

void CScreencopyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_vClients.emplace_back(makeShared<CScreencopyClient>(makeShared<CZwlrScreencopyManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        LOGM(LOG, "Failed to bind client! (out of memory)");
        CLIENT->resource->noMemory();
        m_vClients.pop_back();
        return;
    }

    CLIENT->self = CLIENT;

    LOGM(LOG, "Bound client successfully!");
}

void CScreencopyProtocol::destroyResource(CScreencopyClient* client) {
    std::erase_if(m_vClients, [&](const auto& other) { return other.get() == client; });
    std::erase_if(m_vFrames, [&](const auto& other) { return other->client.get() == client; });
    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return other->client.get() == client; });
}

void CScreencopyProtocol::destroyResource(CScreencopyFrame* frame) {
    std::erase_if(m_vFrames, [&](const auto& other) { return other.get() == frame; });
    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return other.get() == frame; });
}

void CScreencopyProtocol::onOutputCommit(CMonitor* pMonitor) {
    if (m_vFramesAwaitingWrite.empty()) {
        g_pHyprRenderer->m_bDirectScanoutBlocked = false;
        return; // nothing to share
    }

    std::vector<WP<CScreencopyFrame>> framesToRemove;

    // share frame if correct output
    for (auto& f : m_vFramesAwaitingWrite) {
        if (!f->pMonitor || !f->buffer) {
            framesToRemove.push_back(f);
            continue;
        }

        if (f->pMonitor != pMonitor)
            continue;

        f->share();

        f->client->lastFrame.reset();
        ++f->client->frameCounter;

        framesToRemove.push_back(f);
    }

    for (auto& f : framesToRemove) {
        destroyResource(f.get());
    }
}
