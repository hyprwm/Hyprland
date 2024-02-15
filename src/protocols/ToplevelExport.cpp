#include "ToplevelExport.hpp"
#include "../Compositor.hpp"

#include <algorithm>

#include "ToplevelExportWlrFuncs.hpp"

#define TOPLEVEL_EXPORT_VERSION 2

static void bindManagerInt(wl_client* client, void* data, uint32_t version, uint32_t id) {
    g_pProtocolManager->m_pToplevelExportProtocolManager->bindManager(client, data, version, id);
}

static void handleDisplayDestroy(struct wl_listener* listener, void* data) {
    g_pProtocolManager->m_pToplevelExportProtocolManager->displayDestroy();
}

void CToplevelExportProtocolManager::displayDestroy() {
    wl_global_destroy(m_pGlobal);
}

CToplevelExportProtocolManager::CToplevelExportProtocolManager() {

#ifndef GLES32
    Debug::log(WARN, "Toplevel sharing is not supported on LEGACY_RENDERER!");
    return;
#endif

    m_pGlobal = wl_global_create(g_pCompositor->m_sWLDisplay, &hyprland_toplevel_export_manager_v1_interface, TOPLEVEL_EXPORT_VERSION, this, bindManagerInt);

    if (!m_pGlobal) {
        Debug::log(ERR, "ToplevelExportManager could not start! Sharing windows will not work!");
        return;
    }

    m_liDisplayDestroy.notify = handleDisplayDestroy;
    wl_display_add_destroy_listener(g_pCompositor->m_sWLDisplay, &m_liDisplayDestroy);

    Debug::log(LOG, "ToplevelExportManager started successfully!");
}

wlr_foreign_toplevel_handle_v1* zwlrHandleFromResource(wl_resource* resource) {
    // we can't assert here, but it doesnt matter.
    return (wlr_foreign_toplevel_handle_v1*)wl_resource_get_user_data(resource);
}

static void handleCaptureToplevel(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, uint32_t handle) {
    g_pProtocolManager->m_pToplevelExportProtocolManager->captureToplevel(client, resource, frame, overlay_cursor, g_pCompositor->getWindowFromHandle(handle));
}

static void handleCaptureToplevelWithWlr(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, wl_resource* handle) {
    g_pProtocolManager->m_pToplevelExportProtocolManager->captureToplevel(client, resource, frame, overlay_cursor, g_pCompositor->getWindowFromZWLRHandle(handle));
}

static void handleDestroy(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static void handleCopyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer, int32_t ignore_damage) {
    g_pProtocolManager->m_pToplevelExportProtocolManager->copyFrame(client, resource, buffer, ignore_damage);
}

static void handleDestroyFrame(wl_client* client, wl_resource* resource) {
    wl_resource_destroy(resource);
}

static const struct hyprland_toplevel_export_manager_v1_interface toplevelExportManagerImpl = {
    .capture_toplevel                          = handleCaptureToplevel,
    .destroy                                   = handleDestroy,
    .capture_toplevel_with_wlr_toplevel_handle = handleCaptureToplevelWithWlr,
};

static const struct hyprland_toplevel_export_frame_v1_interface toplevelFrameImpl = {.copy = handleCopyFrame, .destroy = handleDestroyFrame};

//
static CScreencopyClient* clientFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &hyprland_toplevel_export_manager_v1_interface, &toplevelExportManagerImpl));
    return (CScreencopyClient*)wl_resource_get_user_data(resource);
}

static SScreencopyFrame* frameFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &hyprland_toplevel_export_frame_v1_interface, &toplevelFrameImpl));
    return (SScreencopyFrame*)wl_resource_get_user_data(resource);
}

void CToplevelExportProtocolManager::removeClient(CScreencopyClient* client, bool force) {
    if (!force) {
        if (!client || client->ref <= 0)
            return;

        if (--client->ref != 0)
            return;
    }

    m_lClients.remove(*client); // TODO: this doesn't get cleaned up after sharing app exits???
}

static void handleManagerResourceDestroy(wl_resource* resource) {
    const auto PCLIENT = clientFromResource(resource);

    g_pProtocolManager->m_pToplevelExportProtocolManager->removeClient(PCLIENT, true);
}

void CToplevelExportProtocolManager::bindManager(wl_client* client, void* data, uint32_t version, uint32_t id) {
    const auto PCLIENT = &m_lClients.emplace_back();

    PCLIENT->clientOwner = CLIENT_TOPLEVEL_EXPORT;
    PCLIENT->resource    = wl_resource_create(client, &hyprland_toplevel_export_manager_v1_interface, version, id);

    if (!PCLIENT->resource) {
        Debug::log(ERR, "ToplevelExportManager could not bind! (out of memory?)");
        m_lClients.remove(*PCLIENT);
        wl_client_post_no_memory(client);
        return;
    }

    PCLIENT->ref = 1;

    wl_resource_set_implementation(PCLIENT->resource, &toplevelExportManagerImpl, PCLIENT, handleManagerResourceDestroy);

    Debug::log(LOG, "ToplevelExportManager bound successfully!");
}

static void handleFrameResourceDestroy(wl_resource* resource) {
    const auto PFRAME = frameFromResource(resource);

    g_pProtocolManager->m_pToplevelExportProtocolManager->removeFrame(PFRAME);
}

void CToplevelExportProtocolManager::removeFrame(SScreencopyFrame* frame, bool force) {
    if (!frame)
        return;

    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return other == frame; });

    wl_resource_set_user_data(frame->resource, nullptr);
    if (frame->buffer && frame->buffer->n_locks > 0)
        wlr_buffer_unlock(frame->buffer);
    removeClient(frame->client, force);
    m_lFrames.remove(*frame);
}

void CToplevelExportProtocolManager::captureToplevel(wl_client* client, wl_resource* resource, uint32_t frame, int32_t overlay_cursor, CWindow* pWindow) {
    const auto PCLIENT = clientFromResource(resource);

    // create a frame
    const auto PFRAME     = &m_lFrames.emplace_back();
    PFRAME->overlayCursor = !!overlay_cursor;
    PFRAME->resource      = wl_resource_create(client, &hyprland_toplevel_export_frame_v1_interface, wl_resource_get_version(resource), frame);
    PFRAME->pWindow       = pWindow;

    if (!PFRAME->pWindow) {
        Debug::log(ERR, "Client requested sharing of window handle {:x} which does not exist!", PFRAME->pWindow);
        hyprland_toplevel_export_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    if (!PFRAME->pWindow->m_bIsMapped || PFRAME->pWindow->isHidden()) {
        Debug::log(ERR, "Client requested sharing of window handle {:x} which is not shareable!", PFRAME->pWindow);
        hyprland_toplevel_export_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    if (!PFRAME->resource) {
        Debug::log(ERR, "Couldn't alloc frame for sharing! (no memory)");
        m_lFrames.remove(*PFRAME);
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(PFRAME->resource, &toplevelFrameImpl, PFRAME, handleFrameResourceDestroy);

    PFRAME->client = PCLIENT;
    PCLIENT->ref++;

    const auto PMONITOR = g_pCompositor->getMonitorFromID(PFRAME->pWindow->m_iMonitorID);

    g_pHyprRenderer->makeEGLCurrent();

    if (g_pHyprOpenGL->m_mMonitorRenderResources.contains(PMONITOR)) {
        const auto RDATA = g_pHyprOpenGL->m_mMonitorRenderResources.at(PMONITOR);
        // bind the fb for its format. Suppress gl errors.
#ifndef GLES2
        glBindFramebuffer(GL_READ_FRAMEBUFFER, RDATA.offloadFB.m_iFb);
#else
        glBindFramebuffer(GL_FRAMEBUFFER, RDATA.offloadFB.m_iFb);
#endif
    } else
        Debug::log(ERR, "No RDATA in toplevelexport???");

    PFRAME->shmFormat = g_pHyprOpenGL->getPreferredReadFormat(PMONITOR);
    if (PFRAME->shmFormat == DRM_FORMAT_INVALID) {
        Debug::log(ERR, "No format supported by renderer in capture toplevel");
        hyprland_toplevel_export_frame_v1_send_failed(resource);
        removeFrame(PFRAME);
        return;
    }

    const auto PSHMINFO = drm_get_pixel_format_info(PFRAME->shmFormat);
    if (!PSHMINFO) {
        Debug::log(ERR, "No pixel format supported by renderer in capture toplevel");
        hyprland_toplevel_export_frame_v1_send_failed(resource);
        removeFrame(PFRAME);
        return;
    }

    if (PMONITOR->output->allocator && (PMONITOR->output->allocator->buffer_caps & WLR_BUFFER_CAP_DMABUF)) {
        PFRAME->dmabufFormat = PMONITOR->output->render_format;
    } else {
        PFRAME->dmabufFormat = DRM_FORMAT_INVALID;
    }

    PFRAME->box = {0, 0, (int)(PFRAME->pWindow->m_vRealSize.vec().x * PMONITOR->scale), (int)(PFRAME->pWindow->m_vRealSize.vec().y * PMONITOR->scale)};
    int ow, oh;
    wlr_output_effective_resolution(PMONITOR->output, &ow, &oh);
    PFRAME->box.transform(PMONITOR->transform, ow, oh).round();

    PFRAME->shmStride = pixel_format_info_min_stride(PSHMINFO, PFRAME->box.w);

    hyprland_toplevel_export_frame_v1_send_buffer(PFRAME->resource, convert_drm_format_to_wl_shm(PFRAME->shmFormat), PFRAME->box.width, PFRAME->box.height, PFRAME->shmStride);

    if (PFRAME->dmabufFormat != DRM_FORMAT_INVALID) {
        hyprland_toplevel_export_frame_v1_send_linux_dmabuf(PFRAME->resource, PFRAME->dmabufFormat, PFRAME->box.width, PFRAME->box.height);
    }

    hyprland_toplevel_export_frame_v1_send_buffer_done(PFRAME->resource);
}

void CToplevelExportProtocolManager::copyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer, int32_t ignore_damage) {
    const auto PFRAME = frameFromResource(resource);

    if (!PFRAME) {
        Debug::log(ERR, "No frame in copyFrame??");
        return;
    }

    if (!g_pCompositor->windowValidMapped(PFRAME->pWindow)) {
        Debug::log(ERR, "Client requested sharing of window handle {:x} which is gone!", (uintptr_t)PFRAME->pWindow);
        hyprland_toplevel_export_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    if (!PFRAME->pWindow->m_bIsMapped || PFRAME->pWindow->isHidden()) {
        Debug::log(ERR, "Client requested sharing of window handle {:x} which is not shareable (2)!", PFRAME->pWindow);
        hyprland_toplevel_export_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    const auto PBUFFER = wlr_buffer_try_from_resource(buffer);
    if (!PBUFFER) {
        wl_resource_post_error(PFRAME->resource, HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        removeFrame(PFRAME);
        return;
    }

    if (PBUFFER->width != PFRAME->box.width || PBUFFER->height != PFRAME->box.height) {
        wl_resource_post_error(PFRAME->resource, HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer dimensions");
        removeFrame(PFRAME);
        return;
    }

    if (PFRAME->buffer) {
        wl_resource_post_error(PFRAME->resource, HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        removeFrame(PFRAME);
        return;
    }

    wlr_dmabuf_attributes dmabufAttrs;
    void*                 wlrBufferAccessData;
    uint32_t              wlrBufferAccessFormat;
    size_t                wlrBufferAccessStride;
    if (wlr_buffer_get_dmabuf(PBUFFER, &dmabufAttrs)) {
        PFRAME->bufferCap = WLR_BUFFER_CAP_DMABUF;

        if (dmabufAttrs.format != PFRAME->dmabufFormat) {
            wl_resource_post_error(PFRAME->resource, HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            removeFrame(PFRAME);
            return;
        }
    } else if (wlr_buffer_begin_data_ptr_access(PBUFFER, WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &wlrBufferAccessData, &wlrBufferAccessFormat, &wlrBufferAccessStride)) {
        wlr_buffer_end_data_ptr_access(PBUFFER);

        if (wlrBufferAccessFormat != PFRAME->shmFormat) {
            wl_resource_post_error(PFRAME->resource, HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            removeFrame(PFRAME);
            return;
        } else if ((int)wlrBufferAccessStride != PFRAME->shmStride) {
            wl_resource_post_error(PFRAME->resource, HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer stride");
            removeFrame(PFRAME);
            return;
        }
    } else {
        wl_resource_post_error(PFRAME->resource, HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer type");
        removeFrame(PFRAME);
        return;
    }

    PFRAME->buffer = PBUFFER;

    m_vFramesAwaitingWrite.emplace_back(PFRAME);
}

void CToplevelExportProtocolManager::onOutputCommit(CMonitor* pMonitor, wlr_output_event_commit* e) {
    if (m_vFramesAwaitingWrite.empty())
        return; // nothing to share

    const auto                     PMONITOR = g_pCompositor->getMonitorFromOutput(e->output);

    std::vector<SScreencopyFrame*> framesToRemove;

    // share frame if correct output
    for (auto& f : m_vFramesAwaitingWrite) {
        if (!f->pWindow) {
            framesToRemove.push_back(f);
            continue;
        }

        if (PMONITOR != g_pCompositor->getMonitorFromID(f->pWindow->m_iMonitorID))
            continue;

        CBox geometry = {f->pWindow->m_vRealPosition.vec().x, f->pWindow->m_vRealPosition.vec().y, f->pWindow->m_vRealSize.vec().x, f->pWindow->m_vRealSize.vec().y};

        if (!wlr_output_layout_intersects(g_pCompositor->m_sWLROutputLayout, pMonitor->output, geometry.pWlr()))
            continue;

        shareFrame(f);

        f->client->lastFrame.reset();
        ++f->client->frameCounter;

        framesToRemove.push_back(f);
    }

    for (auto& f : framesToRemove) {
        removeFrame(f);
    }
}

void CToplevelExportProtocolManager::shareFrame(SScreencopyFrame* frame) {
    if (!frame->buffer || !g_pCompositor->windowValidMapped(frame->pWindow))
        return;

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32_t flags = 0;
    if (frame->bufferCap == WLR_BUFFER_CAP_DMABUF) {
        if (!copyFrameDmabuf(frame, &now)) {
            hyprland_toplevel_export_frame_v1_send_failed(frame->resource);
            return;
        }
    } else {
        if (!copyFrameShm(frame, &now)) {
            hyprland_toplevel_export_frame_v1_send_failed(frame->resource);
            return;
        }
    }

    hyprland_toplevel_export_frame_v1_send_flags(frame->resource, flags);
    sendDamage(frame);
    uint32_t tvSecHi = (sizeof(now.tv_sec) > 4) ? now.tv_sec >> 32 : 0;
    uint32_t tvSecLo = now.tv_sec & 0xFFFFFFFF;
    hyprland_toplevel_export_frame_v1_send_ready(frame->resource, tvSecHi, tvSecLo, now.tv_nsec);
}

void CToplevelExportProtocolManager::sendDamage(SScreencopyFrame* frame) {
    // TODO: send proper dmg
    hyprland_toplevel_export_frame_v1_send_damage(frame->resource, 0, 0, frame->box.width, frame->box.height);
}

bool CToplevelExportProtocolManager::copyFrameShm(SScreencopyFrame* frame, timespec* now) {
    void*    data;
    uint32_t format;
    size_t   stride;
    if (!wlr_buffer_begin_data_ptr_access(frame->buffer, WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride))
        return false;

    // render the client
    const auto PMONITOR = g_pCompositor->getMonitorFromID(frame->pWindow->m_iMonitorID);
    CRegion    fakeDamage{0, 0, PMONITOR->vecPixelSize.x * 10, PMONITOR->vecPixelSize.y * 10};

    g_pHyprRenderer->makeEGLCurrent();

    CFramebuffer outFB;
    outFB.alloc(PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y, g_pHyprRenderer->isNvidia() ? DRM_FORMAT_XBGR8888 : PMONITOR->drmFormat);

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &outFB)) {
        wlr_buffer_end_data_ptr_access(frame->buffer);
        return false;
    }

    if (frame->overlayCursor)
        wlr_output_lock_software_cursors(PMONITOR->output, true);

    g_pHyprOpenGL->clear(CColor(0, 0, 0, 1.0));

    // render client at 0,0
    g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(frame->pWindow); // block the feedback to avoid spamming the surface if it's visible
    g_pHyprRenderer->renderWindow(frame->pWindow, PMONITOR, now, false, RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    if (frame->overlayCursor)
        g_pHyprRenderer->renderSoftwareCursors(PMONITOR, fakeDamage, g_pInputManager->getMouseCoordsInternal() - frame->pWindow->m_vRealPosition.vec());

    const auto PFORMAT = g_pHyprOpenGL->getPixelFormatFromDRM(format);
    if (!PFORMAT) {
        g_pHyprRenderer->endRender();
        wlr_buffer_end_data_ptr_access(frame->buffer);
        return false;
    }

    g_pHyprRenderer->endRender();

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_RenderData.pMonitor = PMONITOR;
    outFB.bind();

#ifndef GLES2
    glBindFramebuffer(GL_READ_FRAMEBUFFER, outFB.m_iFb);
#endif

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(0, 0, frame->box.width, frame->box.height, PFORMAT->glFormat, PFORMAT->glType, data);

    wlr_buffer_end_data_ptr_access(frame->buffer);

    if (frame->overlayCursor)
        wlr_output_lock_software_cursors(PMONITOR->output, false);

    return true;
}

bool CToplevelExportProtocolManager::copyFrameDmabuf(SScreencopyFrame* frame, timespec* now) {
    const auto PMONITOR = g_pCompositor->getMonitorFromID(frame->pWindow->m_iMonitorID);

    CRegion    fakeDamage{0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(PMONITOR, fakeDamage, RENDER_MODE_TO_BUFFER, frame->buffer))
        return false;

    g_pHyprOpenGL->clear(CColor(0, 0, 0, 1.0));

    g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(frame->pWindow); // block the feedback to avoid spamming the surface if it's visible
    g_pHyprRenderer->renderWindow(frame->pWindow, PMONITOR, now, false, RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    if (frame->overlayCursor)
        g_pHyprRenderer->renderSoftwareCursors(PMONITOR, fakeDamage, g_pInputManager->getMouseCoordsInternal() - frame->pWindow->m_vRealPosition.vec());

    g_pHyprRenderer->endRender();
    return true;
}

void CToplevelExportProtocolManager::onWindowUnmap(CWindow* pWindow) {
    for (auto& f : m_lFrames) {
        if (f.pWindow == pWindow)
            f.pWindow = nullptr;
    }
}
