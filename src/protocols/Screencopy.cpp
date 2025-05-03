#include "Screencopy.hpp"
#include "../Compositor.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/PointerManager.hpp"
#include "../managers/input/InputManager.hpp"
#include "../managers/EventManager.hpp"
#include "../managers/permissions/DynamicPermissionManager.hpp"
#include "../render/Renderer.hpp"
#include "../render/OpenGL.hpp"
#include "../helpers/Monitor.hpp"
#include "core/Output.hpp"
#include "types/WLBuffer.hpp"
#include "types/Buffer.hpp"
#include "../helpers/Format.hpp"
#include "../helpers/time/Time.hpp"

#include <algorithm>
#include <functional>

CScreencopyFrame::CScreencopyFrame(SP<CZwlrScreencopyFrameV1> resource_, int32_t overlay_cursor, wl_resource* output, CBox box_) : resource(resource_) {
    if UNLIKELY (!good())
        return;

    overlayCursor = !!overlay_cursor;
    pMonitor      = CWLOutputResource::fromResource(output)->m_monitor;

    if (!pMonitor) {
        LOGM(ERR, "Client requested sharing of a monitor that doesnt exist");
        resource->sendFailed();
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

    shmFormat = g_pHyprOpenGL->getPreferredReadFormat(pMonitor.lock());
    if (shmFormat == DRM_FORMAT_INVALID) {
        LOGM(ERR, "No format supported by renderer in capture output");
        resource->sendFailed();
        return;
    }

    // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT wont work here
    if (shmFormat == DRM_FORMAT_XRGB2101010 || shmFormat == DRM_FORMAT_ARGB2101010)
        shmFormat = DRM_FORMAT_XBGR2101010;

    const auto PSHMINFO = NFormatUtils::getPixelFormatFromDRM(shmFormat);
    if (!PSHMINFO) {
        LOGM(ERR, "No pixel format supported by renderer in capture output");
        resource->sendFailed();
        return;
    }

    dmabufFormat = pMonitor->m_output->state->state().drmFormat;

    if (box_.width == 0 && box_.height == 0)
        box = {0, 0, (int)(pMonitor->m_size.x), (int)(pMonitor->m_size.y)};
    else {
        box = box_;
    }

    box.transform(wlTransformToHyprutils(pMonitor->m_transform), pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y).scale(pMonitor->m_scale).round();

    shmStride = NFormatUtils::minStride(PSHMINFO, box.w);

    resource->sendBuffer(NFormatUtils::drmToShm(shmFormat), box.width, box.height, shmStride);

    if (resource->version() >= 3) {
        if LIKELY (dmabufFormat != DRM_FORMAT_INVALID)
            resource->sendLinuxDmabuf(dmabufFormat, box.width, box.height);

        resource->sendBufferDone();
    }
}

void CScreencopyFrame::copy(CZwlrScreencopyFrameV1* pFrame, wl_resource* buffer_) {
    if UNLIKELY (!good()) {
        LOGM(ERR, "No frame in copyFrame??");
        return;
    }

    if UNLIKELY (!g_pCompositor->monitorExists(pMonitor.lock())) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        resource->sendFailed();
        return;
    }

    const auto PBUFFER = CWLBufferResource::fromResource(buffer_);
    if UNLIKELY (!PBUFFER) {
        LOGM(ERR, "Invalid buffer in {:x}", (uintptr_t)this);
        resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    if UNLIKELY (PBUFFER->buffer->size != box.size()) {
        LOGM(ERR, "Invalid dimensions in {:x}", (uintptr_t)this);
        resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer dimensions");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    if UNLIKELY (buffer) {
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

    buffer = CHLBufferReference(PBUFFER->buffer.lock());

    PROTO::screencopy->m_vFramesAwaitingWrite.emplace_back(self);

    g_pHyprRenderer->m_bDirectScanoutBlocked = true;

    if (!withDamage)
        g_pHyprRenderer->damageMonitor(pMonitor.lock());
}

void CScreencopyFrame::share() {
    if (!buffer || !pMonitor)
        return;

    const auto NOW = Time::steadyNow();

    auto       callback = [this, NOW, weak = self](bool success) {
        if (weak.expired())
            return;

        if (!success) {
            LOGM(ERR, "{} copy failed in {:x}", bufferDMA ? "Dmabuf" : "Shm", (uintptr_t)this);
            resource->sendFailed();
            return;
        }

        resource->sendFlags((zwlrScreencopyFrameV1Flags)0);
        if (withDamage) {
            // TODO: add a damage ring for this.
            resource->sendDamage(0, 0, buffer->size.x, buffer->size.y);
        }

        const auto [sec, nsec] = Time::secNsec(NOW);

        uint32_t tvSecHi = (sizeof(sec) > 4) ? sec >> 32 : 0;
        uint32_t tvSecLo = sec & 0xFFFFFFFF;
        resource->sendReady(tvSecHi, tvSecLo, nsec);
    };

    if (bufferDMA)
        copyDmabuf(callback);
    else
        callback(copyShm());
}

void CScreencopyFrame::copyDmabuf(std::function<void(bool)> callback) {
    const auto PERM    = g_pDynamicPermissionManager->clientPermissionMode(resource->client(), PERMISSION_TYPE_SCREENCOPY);
    auto       TEXTURE = makeShared<CTexture>(pMonitor->m_output->state->state().buffer);

    CRegion    fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_TO_BUFFER, buffer.buffer, nullptr, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering to dma frame");
        callback(false);
        return;
    }

    if (PERM == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        CBox monbox = CBox{0, 0, pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y}
                          .translate({-box.x, -box.y}) // vvvv kinda ass-backwards but that's how I designed the renderer... sigh.
                          .transform(wlTransformToHyprutils(invertTransform(pMonitor->m_transform)), pMonitor->m_pixelSize.x, pMonitor->m_pixelSize.y);
        g_pHyprOpenGL->setMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTexture(TEXTURE, monbox, 1);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->setMonitorTransformEnabled(false);
        if (overlayCursor)
            g_pPointerManager->renderSoftwareCursorsFor(pMonitor.lock(), Time::steadyNow(), fakeDamage,
                                                        g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position - box.pos(), true);
    } else if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING)
        g_pHyprOpenGL->clear(Colors::BLACK);
    else {
        g_pHyprOpenGL->clear(Colors::BLACK);
        CBox texbox =
            CBox{pMonitor->m_transformedSize / 2.F, g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize}.translate(-g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pScreencopyDeniedTexture, texbox, 1);
    }

    g_pHyprOpenGL->m_RenderData.blockScreenShader = true;

    g_pHyprRenderer->endRender([callback]() {
        LOGM(TRACE, "Copied frame via dma");
        callback(true);
    });
}

bool CScreencopyFrame::copyShm() {
    const auto PERM    = g_pDynamicPermissionManager->clientPermissionMode(resource->client(), PERMISSION_TYPE_SCREENCOPY);
    auto       TEXTURE = makeShared<CTexture>(pMonitor->m_output->state->state().buffer);

    auto       shm                = buffer->shm();
    auto [pixelData, fmt, bufLen] = buffer->beginDataPtr(0); // no need for end, cuz it's shm

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->makeEGLCurrent();

    CFramebuffer fb;
    fb.alloc(box.w, box.h, pMonitor->m_output->state->state().drmFormat);

    if (!g_pHyprRenderer->beginRender(pMonitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &fb, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering");
        return false;
    }

    if (PERM == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        CBox monbox = CBox{0, 0, pMonitor->m_transformedSize.x, pMonitor->m_transformedSize.y}.translate({-box.x, -box.y});
        g_pHyprOpenGL->setMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTexture(TEXTURE, monbox, 1);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->setMonitorTransformEnabled(false);
        if (overlayCursor)
            g_pPointerManager->renderSoftwareCursorsFor(pMonitor.lock(), Time::steadyNow(), fakeDamage,
                                                        g_pInputManager->getMouseCoordsInternal() - pMonitor->m_position - box.pos(), true);
    } else if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING)
        g_pHyprOpenGL->clear(Colors::BLACK);
    else {
        g_pHyprOpenGL->clear(Colors::BLACK);
        CBox texbox =
            CBox{pMonitor->m_transformedSize / 2.F, g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize}.translate(-g_pHyprOpenGL->m_pScreencopyDeniedTexture->m_vSize / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_pScreencopyDeniedTexture, texbox, 1);
    }

#ifndef GLES2
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.getFBID());
#else
    glBindFramebuffer(GL_FRAMEBUFFER, fb.getFBID());
#endif

    const auto PFORMAT = NFormatUtils::getPixelFormatFromDRM(shm.format);
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

    const auto drmFmt     = NFormatUtils::getPixelFormatFromDRM(shm.format);
    uint32_t   packStride = NFormatUtils::minStride(drmFmt, box.w);

    // This could be optimized by using a pixel buffer object to make this async,
    // but really clients should just use a dma buffer anyways.
    if (packStride == (uint32_t)shm.stride) {
        glReadPixels(0, 0, box.w, box.h, glFormat, PFORMAT->glType, pixelData);
    } else {
        for (size_t i = 0; i < box.h; ++i) {
            uint32_t y = i;
            glReadPixels(0, y, box.w, 1, glFormat, PFORMAT->glType, ((unsigned char*)pixelData) + i * shm.stride);
        }
    }

    g_pHyprOpenGL->m_RenderData.pMonitor.reset();

#ifndef GLES2
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

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
    if UNLIKELY (!good())
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
    ;
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
    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return !other || other->client.get() == client; });
}

void CScreencopyProtocol::destroyResource(CScreencopyFrame* frame) {
    std::erase_if(m_vFrames, [&](const auto& other) { return other.get() == frame; });
    std::erase_if(m_vFramesAwaitingWrite, [&](const auto& other) { return !other || other.get() == frame; });
}

void CScreencopyProtocol::onOutputCommit(PHLMONITOR pMonitor) {
    if (m_vFramesAwaitingWrite.empty()) {
        g_pHyprRenderer->m_bDirectScanoutBlocked = false;
        return; // nothing to share
    }

    std::vector<WP<CScreencopyFrame>> framesToRemove;
    // reserve number of elements to avoid reallocations
    framesToRemove.reserve(m_vFramesAwaitingWrite.size());

    // share frame if correct output
    for (auto const& f : m_vFramesAwaitingWrite) {
        if (!f)
            continue;

        // check permissions
        const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(f->resource->client(), PERMISSION_TYPE_SCREENCOPY);

        if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING)
            continue; // pending an answer, don't do anything yet.

        // otherwise share. If it's denied, it will be black.

        if (!f->pMonitor || !f->buffer) {
            framesToRemove.emplace_back(f);
            continue;
        }

        if (f->pMonitor != pMonitor)
            continue;

        f->share();

        f->client->lastFrame.reset();
        ++f->client->frameCounter;

        framesToRemove.emplace_back(f);
    }

    for (auto const& f : framesToRemove) {
        std::erase(m_vFramesAwaitingWrite, f);
    }
}
