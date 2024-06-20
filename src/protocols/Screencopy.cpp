#include "Screencopy.hpp"
#include "../Compositor.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/PointerManager.hpp"
#include "core/Output.hpp"
#include "types/WLBuffer.hpp"
#include "types/Buffer.hpp"
#include "../helpers/Format.hpp"

#include <algorithm>

#define SCREENCOPY_VERSION 3

static void bindManagerInt(wl_client* client, void* data, uint32_t version, uint32_t id) {
    g_pProtocolManager->m_pScreencopyProtocolManager->bindManager(client, data, version, id);
}

static void handleDisplayDestroy(struct wl_listener* listener, void* data) {
    g_pProtocolManager->m_pScreencopyProtocolManager->displayDestroy();
}

void CScreencopyProtocolManager::displayDestroy() {
    wl_global_destroy(m_pGlobal);
}

static SScreencopyFrame* frameFromResource(wl_resource*);

CScreencopyProtocolManager::CScreencopyProtocolManager() {

    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, &zwlr_screencopy_manager_v1_interface, SCREENCOPY_VERSION, this, bindManagerInt);

    if (!m_pGlobal) {
        Debug::log(ERR, "ScreencopyProtocolManager could not start! Screensharing will not work!");
        return;
    }

    m_liDisplayDestroy.notify = handleDisplayDestroy;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    Debug::log(LOG, "ScreencopyProtocolManager started successfully!");

    m_pSoftwareCursorTimer = makeShared<CEventLoopTimer>(
        std::nullopt,
        [this](SP<CEventLoopTimer> self, void* data) {
            // TODO: make it per-monitor
            for (auto& m : g_pCompositor->m_vMonitors) {
                g_pPointerManager->unlockSoftwareForMonitor(m);
            }
            m_bTimerArmed = false;

            Debug::log(LOG, "[screencopy] Releasing software cursor lock");
        },
        nullptr);
    g_pEventLoopManager->addTimer(m_pSoftwareCursorTimer);
}

static void handleCaptureOutput(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, wl_resource* output) {
    g_pProtocolManager->m_pScreencopyProtocolManager->captureOutput(client, resource, frame, overlay_cursor, output);
}

static void handleCaptureRegion(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, wl_resource* output, int32_t x, int32_t y, int32_t width,
                                int32_t height) {
    g_pProtocolManager->m_pScreencopyProtocolManager->captureOutput(client, resource, frame, overlay_cursor, output, {x, y, width, height});
}

static void handleDestroy(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static void handleCopyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer) {
    const auto PFRAME = frameFromResource(resource);

    if (!PFRAME)
        return;

    g_pProtocolManager->m_pScreencopyProtocolManager->copyFrame(client, resource, buffer);
}

static void handleCopyWithDamage(wl_client* client, wl_resource* resource, wl_resource* buffer) {
    const auto PFRAME = frameFromResource(resource);

    if (!PFRAME)
        return;

    PFRAME->withDamage = true;
    handleCopyFrame(client, resource, buffer);
}

static void handleDestroyFrame(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static const struct zwlr_screencopy_manager_v1_interface screencopyMgrImpl = {
    .capture_output        = handleCaptureOutput,
    .capture_output_region = handleCaptureRegion,
    .destroy               = handleDestroy,
};

static const struct zwlr_screencopy_frame_v1_interface screencopyFrameImpl = {
    .copy             = handleCopyFrame,
    .destroy          = handleDestroyFrame,
    .copy_with_damage = handleCopyWithDamage,
};

static CScreencopyClient* clientFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &zwlr_screencopy_manager_v1_interface, &screencopyMgrImpl));
    return (CScreencopyClient*)wl_resource_get_user_data(resource);
}

static SScreencopyFrame* frameFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &zwlr_screencopy_frame_v1_interface, &screencopyFrameImpl));
    return (SScreencopyFrame*)wl_resource_get_user_data(resource);
}

void CScreencopyProtocolManager::removeClient(CScreencopyClient* client, bool force) {
    if (!client)
        return;

    if (!force) {
        if (!client || client->ref <= 0)
            return;

        if (--client->ref != 0)
            return;
    }

    m_lClients.remove(*client); // TODO: this doesn't get cleaned up after sharing app exits???

    for (auto& f : m_lFrames) {
        // avoid dangling ptrs
        if (f.client == client)
            f.client = nullptr;
    }
}

static void handleManagerResourceDestroy(wl_resource* resource) {
    const auto PCLIENT = clientFromResource(resource);

    g_pProtocolManager->m_pScreencopyProtocolManager->removeClient(PCLIENT, true);
}

CScreencopyClient::~CScreencopyClient() {
    g_pHookSystem->unhook(tickCallback);
}

CScreencopyClient::CScreencopyClient() {
    lastMeasure.reset();
    lastFrame.reset();
    tickCallback = g_pHookSystem->hookDynamic("tick", [&](void* self, SCallbackInfo& info, std::any data) { onTick(); });
}

void CScreencopyClient::onTick() {
    if (lastMeasure.getMillis() < 500)
        return;

    framesInLastHalfSecond = frameCounter;
    frameCounter           = 0;
    lastMeasure.reset();

    const auto LASTFRAMEDELTA = lastFrame.getMillis() / 1000.0;
    const bool FRAMEAWAITING  = std::ranges::any_of(g_pProtocolManager->m_pScreencopyProtocolManager->m_lFrames, [&](const auto& frame) { return frame.client == this; }) ||
        std::ranges::any_of(g_pProtocolManager->m_pToplevelExportProtocolManager->m_lFrames, [&](const auto& frame) { return frame.client == this; });

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

void CScreencopyProtocolManager::bindManager(wl_client* client, void* data, uint32_t version, uint32_t id) {
    const auto PCLIENT = &m_lClients.emplace_back();

    PCLIENT->resource = wl_resource_create(client, &zwlr_screencopy_manager_v1_interface, version, id);

    if (!PCLIENT->resource) {
        Debug::log(ERR, "ScreencopyProtocolManager could not bind! (out of memory?)");
        m_lClients.remove(*PCLIENT);
        wl_client_post_no_memory(client);
        return;
    }

    PCLIENT->ref = 1;

    wl_resource_set_implementation(PCLIENT->resource, &screencopyMgrImpl, PCLIENT, handleManagerResourceDestroy);

    Debug::log(LOG, "ScreencopyProtocolManager bound successfully!");
}

static void handleFrameResourceDestroy(wl_resource* resource) {
    const auto PFRAME = frameFromResource(resource);

    g_pProtocolManager->m_pScreencopyProtocolManager->removeFrame(PFRAME);
}

void CScreencopyProtocolManager::removeFrame(SScreencopyFrame* frame, bool force) {
    if (!frame)
        return;

    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return other == frame; });

    wl_resource_set_user_data(frame->resource, nullptr);
    if (frame->buffer && frame->buffer->locked())
        frame->buffer->unlock();
    removeClient(frame->client, force);
    m_lFrames.remove(*frame);
}

void CScreencopyProtocolManager::captureOutput(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, wl_resource* output, CBox box) {
    const auto PCLIENT = clientFromResource(resource);

    const auto PFRAME     = &m_lFrames.emplace_back();
    PFRAME->overlayCursor = !!overlay_cursor;
    PFRAME->resource      = wl_resource_create(client, &zwlr_screencopy_frame_v1_interface, wl_resource_get_version(resource), frame);
    PFRAME->pMonitor      = CWLOutputResource::fromResource(output)->monitor.get();

    if (!PFRAME->pMonitor) {
        Debug::log(ERR, "client requested sharing of a monitor that doesnt exist");
        zwlr_screencopy_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    if (!PFRAME->resource) {
        Debug::log(ERR, "Couldn't alloc frame for sharing! (no memory)");
        removeFrame(PFRAME);
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(PFRAME->resource, &screencopyFrameImpl, PFRAME, handleFrameResourceDestroy);

    PFRAME->client = PCLIENT;
    PCLIENT->ref++;

    g_pHyprRenderer->makeEGLCurrent();

    if (g_pHyprOpenGL->m_mMonitorRenderResources.contains(PFRAME->pMonitor)) {
        const auto& RDATA = g_pHyprOpenGL->m_mMonitorRenderResources.at(PFRAME->pMonitor);
        // bind the fb for its format. Suppress gl errors.
#ifndef GLES2
        glBindFramebuffer(GL_READ_FRAMEBUFFER, RDATA.offloadFB.m_iFb);
#else
        glBindFramebuffer(GL_FRAMEBUFFER, RDATA.offloadFB.m_iFb);
#endif
    } else
        Debug::log(ERR, "No RDATA in screencopy???");

    PFRAME->shmFormat = g_pHyprOpenGL->getPreferredReadFormat(PFRAME->pMonitor);
    if (PFRAME->shmFormat == DRM_FORMAT_INVALID) {
        Debug::log(ERR, "No format supported by renderer in capture output");
        zwlr_screencopy_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    const auto PSHMINFO = FormatUtils::getPixelFormatFromDRM(PFRAME->shmFormat);
    if (!PSHMINFO) {
        Debug::log(ERR, "No pixel format supported by renderer in capture output");
        zwlr_screencopy_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    PFRAME->dmabufFormat = PFRAME->pMonitor->output->state->state().drmFormat;

    if (box.width == 0 && box.height == 0)
        PFRAME->box = {0, 0, (int)(PFRAME->pMonitor->vecSize.x), (int)(PFRAME->pMonitor->vecSize.y)};
    else {
        PFRAME->box = box;
    }

    PFRAME->box.transform(wlTransformToHyprutils(PFRAME->pMonitor->transform), PFRAME->pMonitor->vecTransformedSize.x, PFRAME->pMonitor->vecTransformedSize.y)
        .scale(PFRAME->pMonitor->scale)
        .round();

    PFRAME->shmStride = FormatUtils::minStride(PSHMINFO, PFRAME->box.w);

    zwlr_screencopy_frame_v1_send_buffer(PFRAME->resource, FormatUtils::drmToShm(PFRAME->shmFormat), PFRAME->box.width, PFRAME->box.height, PFRAME->shmStride);

    if (wl_resource_get_version(resource) >= 3) {
        if (PFRAME->dmabufFormat != DRM_FORMAT_INVALID) {
            zwlr_screencopy_frame_v1_send_linux_dmabuf(PFRAME->resource, PFRAME->dmabufFormat, PFRAME->box.width, PFRAME->box.height);
        }

        zwlr_screencopy_frame_v1_send_buffer_done(PFRAME->resource);
    }
}

void CScreencopyProtocolManager::copyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer) {
    const auto PFRAME = frameFromResource(resource);

    if (!PFRAME) {
        Debug::log(ERR, "No frame in copyFrame??");
        return;
    }

    if (!g_pCompositor->monitorExists(PFRAME->pMonitor)) {
        Debug::log(ERR, "client requested sharing of a monitor that is gone");
        zwlr_screencopy_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    const auto PBUFFER = CWLBufferResource::fromResource(buffer);
    if (!PBUFFER) {
        Debug::log(ERR, "[sc] invalid buffer in {:x}", (uintptr_t)PFRAME);
        wl_resource_post_error(PFRAME->resource, ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        removeFrame(PFRAME);
        return;
    }

    PBUFFER->buffer->lock();

    if (PBUFFER->buffer->size != PFRAME->box.size()) {
        Debug::log(ERR, "[sc] invalid dimensions in {:x}", (uintptr_t)PFRAME);
        wl_resource_post_error(PFRAME->resource, ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer dimensions");
        removeFrame(PFRAME);
        return;
    }

    if (PFRAME->buffer) {
        Debug::log(ERR, "[sc] buffer used in {:x}", (uintptr_t)PFRAME);
        wl_resource_post_error(PFRAME->resource, ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        removeFrame(PFRAME);
        return;
    }

    if (auto attrs = PBUFFER->buffer->dmabuf(); attrs.success) {
        PFRAME->bufferDMA = true;

        if (attrs.format != PFRAME->dmabufFormat) {
            Debug::log(ERR, "[sc] invalid buffer dma format in {:x}", (uintptr_t)PFRAME);
            wl_resource_post_error(PFRAME->resource, ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            removeFrame(PFRAME);
            return;
        }
    } else if (auto attrs = PBUFFER->buffer->shm(); attrs.success) {
        if (attrs.format != PFRAME->shmFormat) {
            Debug::log(ERR, "[sc] invalid buffer shm format in {:x}", (uintptr_t)PFRAME);
            wl_resource_post_error(PFRAME->resource, ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            removeFrame(PFRAME);
            return;
        } else if ((int)attrs.stride != PFRAME->shmStride) {
            Debug::log(ERR, "[sc] invalid buffer shm stride in {:x}", (uintptr_t)PFRAME);
            wl_resource_post_error(PFRAME->resource, ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer stride");
            removeFrame(PFRAME);
            return;
        }
    } else {
        Debug::log(ERR, "[sc] invalid buffer type in {:x}", (uintptr_t)PFRAME);
        wl_resource_post_error(PFRAME->resource, ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer type");
        removeFrame(PFRAME);
        return;
    }

    PFRAME->buffer = PBUFFER->buffer;

    m_vFramesAwaitingWrite.emplace_back(PFRAME);

    g_pHyprRenderer->m_bDirectScanoutBlocked = true;
    if (PFRAME->overlayCursor && !PFRAME->lockedSWCursors) {
        PFRAME->lockedSWCursors = true;
        // TODO: make it per-monitor
        if (!m_bTimerArmed) {
            for (auto& m : g_pCompositor->m_vMonitors) {
                g_pPointerManager->lockSoftwareForMonitor(m);
            }
            m_bTimerArmed = true;
            Debug::log(LOG, "[screencopy] Locking sw cursors due to screensharing");
        }
        m_pSoftwareCursorTimer->updateTimeout(std::chrono::seconds(1));
    }

    if (!PFRAME->withDamage)
        g_pHyprRenderer->damageMonitor(PFRAME->pMonitor);
}

void CScreencopyProtocolManager::onOutputCommit(CMonitor* pMonitor) {
    m_pLastMonitorBackBuffer = pMonitor->output->state->state().buffer;
    shareAllFrames(pMonitor);
    m_pLastMonitorBackBuffer.reset();
}

void CScreencopyProtocolManager::shareAllFrames(CMonitor* pMonitor) {
    if (m_vFramesAwaitingWrite.empty())
        return; // nothing to share

    std::vector<SScreencopyFrame*> framesToRemove;

    // share frame if correct output
    for (auto& f : m_vFramesAwaitingWrite) {
        if (!f->pMonitor || !f->buffer) {
            framesToRemove.push_back(f);
            continue;
        }

        if (f->pMonitor != pMonitor)
            continue;

        shareFrame(f);

        f->client->lastFrame.reset();
        ++f->client->frameCounter;

        framesToRemove.push_back(f);
    }

    for (auto& f : framesToRemove) {
        removeFrame(f);
    }

    if (m_vFramesAwaitingWrite.empty()) {
        g_pHyprRenderer->m_bDirectScanoutBlocked = false;
    }
}

void CScreencopyProtocolManager::shareFrame(SScreencopyFrame* frame) {
    if (!frame->buffer)
        return;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32_t flags = 0;
    if (frame->bufferDMA) {
        if (!copyFrameDmabuf(frame)) {
            Debug::log(ERR, "[sc] dmabuf copy failed in {:x}", (uintptr_t)frame);
            zwlr_screencopy_frame_v1_send_failed(frame->resource);
            return;
        }
    } else {
        if (!copyFrameShm(frame, &now)) {
            Debug::log(ERR, "[sc] shm copy failed in {:x}", (uintptr_t)frame);
            zwlr_screencopy_frame_v1_send_failed(frame->resource);
            return;
        }
    }

    zwlr_screencopy_frame_v1_send_flags(frame->resource, flags);
    sendFrameDamage(frame);
    uint32_t tvSecHi = (sizeof(now.tv_sec) > 4) ? now.tv_sec >> 32 : 0;
    uint32_t tvSecLo = now.tv_sec & 0xFFFFFFFF;
    zwlr_screencopy_frame_v1_send_ready(frame->resource, tvSecHi, tvSecLo, now.tv_nsec);
}

void CScreencopyProtocolManager::sendFrameDamage(SScreencopyFrame* frame) {
    if (!frame->withDamage)
        return;

    // TODO:
    // add a damage ring for this.

    // for (auto& RECT : frame->pMonitor->lastFrameDamage.getRects()) {

    //     if (frame->buffer->width < 1 || frame->buffer->height < 1 || frame->buffer->width - RECT.x1 < 1 || frame->buffer->height - RECT.y1 < 1) {
    //         Debug::log(ERR, "[sc] Failed to send damage");
    //         break;
    //     }

    //     zwlr_screencopy_frame_v1_send_damage(frame->resource, std::clamp(RECT.x1, 0, frame->buffer->width), std::clamp(RECT.y1, 0, frame->buffer->height),
    //                                          std::clamp(RECT.x2 - RECT.x1, 0, frame->buffer->width - RECT.x1), std::clamp(RECT.y2 - RECT.y1, 0, frame->buffer->height - RECT.y1));
    // }

    zwlr_screencopy_frame_v1_send_damage(frame->resource, 0, 0, frame->buffer->size.x, frame->buffer->size.y);
}

bool CScreencopyProtocolManager::copyFrameShm(SScreencopyFrame* frame, timespec* now) {
    auto TEXTURE = makeShared<CTexture>(m_pLastMonitorBackBuffer);

    auto shm                      = frame->buffer->shm();
    auto [pixelData, fmt, bufLen] = frame->buffer->beginDataPtr(0); // no need for end, cuz it's shm

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->makeEGLCurrent();

    CFramebuffer fb;
    fb.alloc(frame->box.w, frame->box.h, g_pHyprRenderer->isNvidia() ? DRM_FORMAT_XBGR8888 : frame->pMonitor->drmFormat);

    if (!g_pHyprRenderer->beginRender(frame->pMonitor, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &fb, true))
        return false;

    CBox monbox = CBox{0, 0, frame->pMonitor->vecTransformedSize.x, frame->pMonitor->vecTransformedSize.y}.translate({-frame->box.x, -frame->box.y});
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
        g_pHyprRenderer->endRender();
        return false;
    }

    auto glFormat = PFORMAT->flipRB ? GL_BGRA_EXT : GL_RGBA;

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_RenderData.pMonitor = frame->pMonitor;
    fb.bind();

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    const auto drmFmt     = FormatUtils::getPixelFormatFromDRM(shm.format);
    uint32_t   packStride = FormatUtils::minStride(drmFmt, frame->box.w);

    if (packStride == (uint32_t)shm.stride) {
        glReadPixels(0, 0, frame->box.w, frame->box.h, glFormat, PFORMAT->glType, pixelData);
    } else {
        for (size_t i = 0; i < frame->box.h; ++i) {
            uint32_t y = i;
            glReadPixels(0, y, frame->box.w, 1, glFormat, PFORMAT->glType, ((unsigned char*)pixelData) + i * shm.stride);
        }
    }

    g_pHyprOpenGL->m_RenderData.pMonitor = nullptr;

    return true;
}

bool CScreencopyProtocolManager::copyFrameDmabuf(SScreencopyFrame* frame) {
    auto    TEXTURE = makeShared<CTexture>(m_pLastMonitorBackBuffer);

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(frame->pMonitor, fakeDamage, RENDER_MODE_TO_BUFFER, frame->buffer.lock(), nullptr, true))
        return false;

    CBox monbox = CBox{0, 0, frame->pMonitor->vecPixelSize.x, frame->pMonitor->vecPixelSize.y}
                      .translate({-frame->box.x, -frame->box.y}) // vvvv kinda ass-backwards but that's how I designed the renderer... sigh.
                      .transform(wlTransformToHyprutils(wlr_output_transform_invert(frame->pMonitor->transform)), frame->pMonitor->vecPixelSize.x, frame->pMonitor->vecPixelSize.y);
    g_pHyprOpenGL->setMonitorTransformEnabled(true);
    g_pHyprOpenGL->setRenderModifEnabled(false);
    g_pHyprOpenGL->renderTexture(TEXTURE, &monbox, 1);
    g_pHyprOpenGL->setRenderModifEnabled(true);
    g_pHyprOpenGL->setMonitorTransformEnabled(false);

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    return true;
}
