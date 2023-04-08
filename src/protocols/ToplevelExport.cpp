#include "ToplevelExport.hpp"
#include "../Compositor.hpp"
#include <drm_fourcc.h>

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

    g_pHookSystem->hookDynamic("preRender", [&](void* self, std::any data) { onMonitorRender(std::any_cast<CMonitor*>(data)); });

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

static SToplevelClient*                                         clientFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &hyprland_toplevel_export_manager_v1_interface, &toplevelExportManagerImpl));
    return (SToplevelClient*)wl_resource_get_user_data(resource);
}

static SToplevelFrame* frameFromResource(wl_resource* resource) {
    ASSERT(wl_resource_instance_of(resource, &hyprland_toplevel_export_frame_v1_interface, &toplevelFrameImpl));
    return (SToplevelFrame*)wl_resource_get_user_data(resource);
}

void CToplevelExportProtocolManager::removeClient(SToplevelClient* client, bool force) {
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

    PCLIENT->resource = wl_resource_create(client, &hyprland_toplevel_export_manager_v1_interface, version, id);

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

void CToplevelExportProtocolManager::removeFrame(SToplevelFrame* frame, bool force) {
    if (!frame)
        return;

    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return other == frame; });

    wl_resource_set_user_data(frame->resource, nullptr);
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
        Debug::log(ERR, "Client requested sharing of window handle %x which does not exist!", PFRAME->pWindow);
        hyprland_toplevel_export_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    if (!PFRAME->pWindow->m_bIsMapped || PFRAME->pWindow->isHidden()) {
        Debug::log(ERR, "Client requested sharing of window handle %x which is not shareable!", PFRAME->pWindow);
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

    PFRAME->shmFormat = wlr_output_preferred_read_format(PMONITOR->output);
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
    wlr_box_transform(&PFRAME->box, &PFRAME->box, PMONITOR->transform, ow, oh);

    PFRAME->shmStride = (PSHMINFO->bpp / 8) * PFRAME->box.width;

    hyprland_toplevel_export_frame_v1_send_buffer(PFRAME->resource, convert_drm_format_to_wl_shm(PFRAME->shmFormat), PFRAME->box.width, PFRAME->box.height, PFRAME->shmStride);
}

void CToplevelExportProtocolManager::copyFrame(wl_client* client, wl_resource* resource, wl_resource* buffer, int32_t ignore_damage) {
    const auto PFRAME = frameFromResource(resource);

    if (!PFRAME) {
        Debug::log(ERR, "No frame in copyFrame??");
        return;
    }

    if (!PFRAME->pWindow->m_bIsMapped || PFRAME->pWindow->isHidden()) {
        Debug::log(ERR, "Client requested sharing of window handle %x which is not shareable (2)!", PFRAME->pWindow);
        hyprland_toplevel_export_frame_v1_send_failed(PFRAME->resource);
        removeFrame(PFRAME);
        return;
    }

    const auto PBUFFER = wlr_buffer_from_resource(buffer);
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

void CToplevelExportProtocolManager::onMonitorRender(CMonitor* pMonitor) {

    if (m_vFramesAwaitingWrite.empty())
        return; // nothing to share

    std::vector<SToplevelFrame*> framesToRemove;

    // share frame if correct output
    for (auto& f : m_vFramesAwaitingWrite) {
        if (!f->pWindow) {
            framesToRemove.push_back(f);
            continue;
        }

        wlr_box geometry = {f->pWindow->m_vRealPosition.vec().x, f->pWindow->m_vRealPosition.vec().y, f->pWindow->m_vRealSize.vec().x, f->pWindow->m_vRealSize.vec().y};

        if (!wlr_output_layout_intersects(g_pCompositor->m_sWLROutputLayout, pMonitor->output, &geometry))
            continue;

        shareFrame(f);

        framesToRemove.push_back(f);
    }

    for (auto& f : framesToRemove) {
        removeFrame(f);
    }
}

void CToplevelExportProtocolManager::shareFrame(SToplevelFrame* frame) {
    if (!frame->buffer) {
        return;
    }

    // TODO: damage

    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint32_t flags = 0;
    if (frame->bufferCap == WLR_BUFFER_CAP_DMABUF) {
        if (!copyFrameDmabuf(frame)) {
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
    // todo: send damage
    uint32_t tvSecHi = (sizeof(now.tv_sec) > 4) ? now.tv_sec >> 32 : 0;
    uint32_t tvSecLo = now.tv_sec & 0xFFFFFFFF;
    hyprland_toplevel_export_frame_v1_send_ready(frame->resource, tvSecHi, tvSecLo, now.tv_nsec);
}

bool CToplevelExportProtocolManager::copyFrameShm(SToplevelFrame* frame, timespec* now) {
    void*    data;
    uint32_t format;
    size_t   stride;
    if (!wlr_buffer_begin_data_ptr_access(frame->buffer, WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride))
        return false;

    // render the client
    const auto        PMONITOR = g_pCompositor->getMonitorFromID(frame->pWindow->m_iMonitorID);
    pixman_region32_t fakeDamage;
    pixman_region32_init_rect(&fakeDamage, 0, 0, PMONITOR->vecPixelSize.x * 10, PMONITOR->vecPixelSize.y * 10);

    if (frame->overlayCursor)
        wlr_output_lock_software_cursors(PMONITOR->output, true);

    if (!wlr_output_attach_render(PMONITOR->output, nullptr)) {
        Debug::log(ERR, "[toplevel_export] Couldn't attach render");
        pixman_region32_fini(&fakeDamage);
        wlr_buffer_end_data_ptr_access(frame->buffer);
        if (frame->overlayCursor)
            wlr_output_lock_software_cursors(PMONITOR->output, false);
        return false;
    }

    g_pHyprOpenGL->begin(PMONITOR, &fakeDamage, true);
    g_pHyprOpenGL->clear(CColor(0, 0, 0, 1.0));

    // render client at 0,0
    g_pHyprRenderer->m_bBlockSurfaceFeedback = g_pHyprRenderer->shouldRenderWindow(frame->pWindow); // block the feedback to avoid spamming the surface if it's visible
    g_pHyprRenderer->renderWindow(frame->pWindow, PMONITOR, now, false, RENDER_PASS_ALL, true, true);
    g_pHyprRenderer->m_bBlockSurfaceFeedback = false;

    if (frame->overlayCursor && wlr_renderer_begin(g_pCompositor->m_sWLRRenderer, PMONITOR->vecPixelSize.x, PMONITOR->vecPixelSize.y)) {
        // hack le massive
        wlr_output_cursor* cursor;
        const auto         OFFSET = frame->pWindow->m_vRealPosition.vec() - PMONITOR->vecPosition;
        wl_list_for_each(cursor, &PMONITOR->output->cursors, link) {
            if (!cursor->enabled || !cursor->visible || PMONITOR->output->hardware_cursor == cursor) {
                continue;
            }
            cursor->x -= OFFSET.x;
            cursor->y -= OFFSET.y;
        }
        wlr_output_render_software_cursors(PMONITOR->output, NULL);
        wl_list_for_each(cursor, &PMONITOR->output->cursors, link) {
            if (!cursor->enabled || !cursor->visible || PMONITOR->output->hardware_cursor == cursor) {
                continue;
            }
            cursor->x += OFFSET.x;
            cursor->y += OFFSET.y;
        }
        wlr_renderer_end(g_pCompositor->m_sWLRRenderer);
    }

    // copy pixels
    const auto PFORMAT = get_gles2_format_from_drm(format);
    if (!PFORMAT) {
        Debug::log(ERR, "[toplevel_export] Cannot read pixels, unsupported format %x", PFORMAT);
        g_pHyprOpenGL->end();
        pixman_region32_fini(&fakeDamage);
        wlr_buffer_end_data_ptr_access(frame->buffer);
        if (frame->overlayCursor)
            wlr_output_lock_software_cursors(PMONITOR->output, false);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_pHyprOpenGL->m_RenderData.pCurrentMonData->primaryFB.m_iFb);

    glFinish(); // flush

    glReadPixels(0, 0, frame->box.width, frame->box.height, PFORMAT->gl_format, PFORMAT->gl_type, data);

    g_pHyprOpenGL->end();

    wlr_output_rollback(PMONITOR->output);

    pixman_region32_fini(&fakeDamage);

    wlr_buffer_end_data_ptr_access(frame->buffer);

    if (frame->overlayCursor)
        wlr_output_lock_software_cursors(PMONITOR->output, false);

    return true;
}

bool CToplevelExportProtocolManager::copyFrameDmabuf(SToplevelFrame* frame) {
    // todo
    Debug::log(ERR, "DMABUF copying not impl'd!");
    return false;
}

void CToplevelExportProtocolManager::onWindowUnmap(CWindow* pWindow) {
    for (auto& f : m_lFrames) {
        if (f.pWindow == pWindow)
            f.pWindow = nullptr;
    }
}
