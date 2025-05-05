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

CScreencopyFrame::CScreencopyFrame(SP<CZwlrScreencopyFrameV1> resource_, int32_t overlay_cursor, wl_resource* output, CBox box_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_overlayCursor = !!overlay_cursor;
    m_monitor       = CWLOutputResource::fromResource(output)->m_monitor;

    if (!m_monitor) {
        LOGM(ERR, "Client requested sharing of a monitor that doesnt exist");
        m_resource->sendFailed();
        return;
    }

    m_resource->setOnDestroy([this](CZwlrScreencopyFrameV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    m_resource->setDestroy([this](CZwlrScreencopyFrameV1* pFrame) { PROTO::screencopy->destroyResource(this); });
    m_resource->setCopy([this](CZwlrScreencopyFrameV1* pFrame, wl_resource* res) { this->copy(pFrame, res); });
    m_resource->setCopyWithDamage([this](CZwlrScreencopyFrameV1* pFrame, wl_resource* res) {
        m_withDamage = true;
        this->copy(pFrame, res);
    });

    g_pHyprRenderer->makeEGLCurrent();

    m_shmFormat = g_pHyprOpenGL->getPreferredReadFormat(m_monitor.lock());
    if (m_shmFormat == DRM_FORMAT_INVALID) {
        LOGM(ERR, "No format supported by renderer in capture output");
        m_resource->sendFailed();
        return;
    }

    // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT wont work here
    if (m_shmFormat == DRM_FORMAT_XRGB2101010 || m_shmFormat == DRM_FORMAT_ARGB2101010)
        m_shmFormat = DRM_FORMAT_XBGR2101010;

    const auto PSHMINFO = NFormatUtils::getPixelFormatFromDRM(m_shmFormat);
    if (!PSHMINFO) {
        LOGM(ERR, "No pixel format supported by renderer in capture output");
        m_resource->sendFailed();
        return;
    }

    m_dmabufFormat = m_monitor->m_output->state->state().drmFormat;

    if (box_.width == 0 && box_.height == 0)
        m_box = {0, 0, (int)(m_monitor->m_size.x), (int)(m_monitor->m_size.y)};
    else {
        m_box = box_;
    }

    m_box.transform(wlTransformToHyprutils(m_monitor->m_transform), m_monitor->m_transformedSize.x, m_monitor->m_transformedSize.y).scale(m_monitor->m_scale).round();

    m_shmStride = NFormatUtils::minStride(PSHMINFO, m_box.w);

    m_resource->sendBuffer(NFormatUtils::drmToShm(m_shmFormat), m_box.width, m_box.height, m_shmStride);

    if (m_resource->version() >= 3) {
        if LIKELY (m_dmabufFormat != DRM_FORMAT_INVALID)
            m_resource->sendLinuxDmabuf(m_dmabufFormat, m_box.width, m_box.height);

        m_resource->sendBufferDone();
    }
}

void CScreencopyFrame::copy(CZwlrScreencopyFrameV1* pFrame, wl_resource* buffer_) {
    if UNLIKELY (!good()) {
        LOGM(ERR, "No frame in copyFrame??");
        return;
    }

    if UNLIKELY (!g_pCompositor->monitorExists(m_monitor.lock())) {
        LOGM(ERR, "Client requested sharing of a monitor that is gone");
        m_resource->sendFailed();
        return;
    }

    const auto PBUFFER = CWLBufferResource::fromResource(buffer_);
    if UNLIKELY (!PBUFFER) {
        LOGM(ERR, "Invalid buffer in {:x}", (uintptr_t)this);
        m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    if UNLIKELY (PBUFFER->m_buffer->size != m_box.size()) {
        LOGM(ERR, "Invalid dimensions in {:x}", (uintptr_t)this);
        m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer dimensions");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    if UNLIKELY (m_buffer) {
        LOGM(ERR, "Buffer used in {:x}", (uintptr_t)this);
        m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    if (auto attrs = PBUFFER->m_buffer->dmabuf(); attrs.success) {
        m_bufferDMA = true;

        if (attrs.format != m_dmabufFormat) {
            LOGM(ERR, "Invalid buffer dma format in {:x}", (uintptr_t)pFrame);
            m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::screencopy->destroyResource(this);
            return;
        }
    } else if (auto attrs = PBUFFER->m_buffer->shm(); attrs.success) {
        if (attrs.format != m_shmFormat) {
            LOGM(ERR, "Invalid buffer shm format in {:x}", (uintptr_t)pFrame);
            m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer format");
            PROTO::screencopy->destroyResource(this);
            return;
        } else if ((int)attrs.stride != m_shmStride) {
            LOGM(ERR, "Invalid buffer shm stride in {:x}", (uintptr_t)pFrame);
            m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer stride");
            PROTO::screencopy->destroyResource(this);
            return;
        }
    } else {
        LOGM(ERR, "Invalid buffer type in {:x}", (uintptr_t)pFrame);
        m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer type");
        PROTO::screencopy->destroyResource(this);
        return;
    }

    m_buffer = CHLBufferReference(PBUFFER->m_buffer.lock());

    PROTO::screencopy->m_framesAwaitingWrite.emplace_back(m_self);

    g_pHyprRenderer->m_directScanoutBlocked = true;

    if (!m_withDamage)
        g_pHyprRenderer->damageMonitor(m_monitor.lock());
}

void CScreencopyFrame::share() {
    if (!m_buffer || !m_monitor)
        return;

    const auto NOW = Time::steadyNow();

    auto       callback = [this, NOW, weak = m_self](bool success) {
        if (weak.expired())
            return;

        if (!success) {
            LOGM(ERR, "{} copy failed in {:x}", m_bufferDMA ? "Dmabuf" : "Shm", (uintptr_t)this);
            m_resource->sendFailed();
            return;
        }

        m_resource->sendFlags((zwlrScreencopyFrameV1Flags)0);
        if (m_withDamage) {
            // TODO: add a damage ring for this.
            m_resource->sendDamage(0, 0, m_buffer->size.x, m_buffer->size.y);
        }

        const auto [sec, nsec] = Time::secNsec(NOW);

        uint32_t tvSecHi = (sizeof(sec) > 4) ? sec >> 32 : 0;
        uint32_t tvSecLo = sec & 0xFFFFFFFF;
        m_resource->sendReady(tvSecHi, tvSecLo, nsec);
    };

    if (m_bufferDMA)
        copyDmabuf(callback);
    else
        callback(copyShm());
}

void CScreencopyFrame::copyDmabuf(std::function<void(bool)> callback) {
    const auto PERM    = g_pDynamicPermissionManager->clientPermissionMode(m_resource->client(), PERMISSION_TYPE_SCREENCOPY);
    auto       TEXTURE = makeShared<CTexture>(m_monitor->m_output->state->state().buffer);

    CRegion    fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    if (!g_pHyprRenderer->beginRender(m_monitor.lock(), fakeDamage, RENDER_MODE_TO_BUFFER, m_buffer.m_buffer, nullptr, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering to dma frame");
        callback(false);
        return;
    }

    if (PERM == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        CBox monbox = CBox{0, 0, m_monitor->m_pixelSize.x, m_monitor->m_pixelSize.y}
                          .translate({-m_box.x, -m_box.y}) // vvvv kinda ass-backwards but that's how I designed the renderer... sigh.
                          .transform(wlTransformToHyprutils(invertTransform(m_monitor->m_transform)), m_monitor->m_pixelSize.x, m_monitor->m_pixelSize.y);
        g_pHyprOpenGL->setMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTexture(TEXTURE, monbox, 1);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->setMonitorTransformEnabled(false);
        if (m_overlayCursor)
            g_pPointerManager->renderSoftwareCursorsFor(m_monitor.lock(), Time::steadyNow(), fakeDamage,
                                                        g_pInputManager->getMouseCoordsInternal() - m_monitor->m_position - m_box.pos(), true);
    } else if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING)
        g_pHyprOpenGL->clear(Colors::BLACK);
    else {
        g_pHyprOpenGL->clear(Colors::BLACK);
        CBox texbox = CBox{m_monitor->m_transformedSize / 2.F, g_pHyprOpenGL->m_screencopyDeniedTexture->m_size}.translate(-g_pHyprOpenGL->m_screencopyDeniedTexture->m_size / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_screencopyDeniedTexture, texbox, 1);
    }

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;

    g_pHyprRenderer->endRender([callback]() {
        LOGM(TRACE, "Copied frame via dma");
        callback(true);
    });
}

bool CScreencopyFrame::copyShm() {
    const auto PERM    = g_pDynamicPermissionManager->clientPermissionMode(m_resource->client(), PERMISSION_TYPE_SCREENCOPY);
    auto       TEXTURE = makeShared<CTexture>(m_monitor->m_output->state->state().buffer);

    auto       shm                = m_buffer->shm();
    auto [pixelData, fmt, bufLen] = m_buffer->beginDataPtr(0); // no need for end, cuz it's shm

    CRegion fakeDamage = {0, 0, INT16_MAX, INT16_MAX};

    g_pHyprRenderer->makeEGLCurrent();

    CFramebuffer fb;
    fb.alloc(m_box.w, m_box.h, m_monitor->m_output->state->state().drmFormat);

    if (!g_pHyprRenderer->beginRender(m_monitor.lock(), fakeDamage, RENDER_MODE_FULL_FAKE, nullptr, &fb, true)) {
        LOGM(ERR, "Can't copy: failed to begin rendering");
        return false;
    }

    if (PERM == PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        CBox monbox = CBox{0, 0, m_monitor->m_transformedSize.x, m_monitor->m_transformedSize.y}.translate({-m_box.x, -m_box.y});
        g_pHyprOpenGL->setMonitorTransformEnabled(true);
        g_pHyprOpenGL->setRenderModifEnabled(false);
        g_pHyprOpenGL->renderTexture(TEXTURE, monbox, 1);
        g_pHyprOpenGL->setRenderModifEnabled(true);
        g_pHyprOpenGL->setMonitorTransformEnabled(false);
        if (m_overlayCursor)
            g_pPointerManager->renderSoftwareCursorsFor(m_monitor.lock(), Time::steadyNow(), fakeDamage,
                                                        g_pInputManager->getMouseCoordsInternal() - m_monitor->m_position - m_box.pos(), true);
    } else if (PERM == PERMISSION_RULE_ALLOW_MODE_PENDING)
        g_pHyprOpenGL->clear(Colors::BLACK);
    else {
        g_pHyprOpenGL->clear(Colors::BLACK);
        CBox texbox = CBox{m_monitor->m_transformedSize / 2.F, g_pHyprOpenGL->m_screencopyDeniedTexture->m_size}.translate(-g_pHyprOpenGL->m_screencopyDeniedTexture->m_size / 2.F);
        g_pHyprOpenGL->renderTexture(g_pHyprOpenGL->m_screencopyDeniedTexture, texbox, 1);
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

    g_pHyprOpenGL->m_renderData.blockScreenShader = true;
    g_pHyprRenderer->endRender();

    g_pHyprRenderer->makeEGLCurrent();
    g_pHyprOpenGL->m_renderData.pMonitor = m_monitor;
    fb.bind();

    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    const auto drmFmt     = NFormatUtils::getPixelFormatFromDRM(shm.format);
    uint32_t   packStride = NFormatUtils::minStride(drmFmt, m_box.w);

    // This could be optimized by using a pixel buffer object to make this async,
    // but really clients should just use a dma buffer anyways.
    if (packStride == (uint32_t)shm.stride) {
        glReadPixels(0, 0, m_box.w, m_box.h, glFormat, PFORMAT->glType, pixelData);
    } else {
        for (size_t i = 0; i < m_box.h; ++i) {
            uint32_t y = i;
            glReadPixels(0, y, m_box.w, 1, glFormat, PFORMAT->glType, ((unsigned char*)pixelData) + i * shm.stride);
        }
    }

    g_pHyprOpenGL->m_renderData.pMonitor.reset();

#ifndef GLES2
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
#else
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

    LOGM(TRACE, "Copied frame via shm");

    return true;
}

bool CScreencopyFrame::good() {
    return m_resource->resource();
}

CScreencopyClient::~CScreencopyClient() {
    g_pHookSystem->unhook(m_tickCallback);
}

CScreencopyClient::CScreencopyClient(SP<CZwlrScreencopyManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwlrScreencopyManagerV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrScreencopyManagerV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    m_resource->setCaptureOutput(
        [this](CZwlrScreencopyManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* output) { this->captureOutput(frame, overlayCursor, output, {}); });
    m_resource->setCaptureOutputRegion([this](CZwlrScreencopyManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* output, int32_t x, int32_t y, int32_t w,
                                              int32_t h) { this->captureOutput(frame, overlayCursor, output, {x, y, w, h}); });

    m_lastMeasure.reset();
    m_lastFrame.reset();
    m_tickCallback = g_pHookSystem->hookDynamic("tick", [&](void* self, SCallbackInfo& info, std::any data) { onTick(); });
}

void CScreencopyClient::captureOutput(uint32_t frame, int32_t overlayCursor_, wl_resource* output, CBox box) {
    const auto FRAME = PROTO::screencopy->m_frames.emplace_back(
        makeShared<CScreencopyFrame>(makeShared<CZwlrScreencopyFrameV1>(m_resource->client(), m_resource->version(), frame), overlayCursor_, output, box));

    if (!FRAME->good()) {
        LOGM(ERR, "Couldn't alloc frame for sharing! (no memory)");
        m_resource->noMemory();
        PROTO::screencopy->destroyResource(FRAME.get());
        return;
    }

    FRAME->m_self   = FRAME;
    FRAME->m_client = m_self;
}

void CScreencopyClient::onTick() {
    if (m_lastMeasure.getMillis() < 500)
        return;

    m_framesInLastHalfSecond = m_frameCounter;
    m_frameCounter           = 0;
    m_lastMeasure.reset();

    const auto LASTFRAMEDELTA = m_lastFrame.getMillis() / 1000.0;
    const bool FRAMEAWAITING  = std::ranges::any_of(PROTO::screencopy->m_frames, [&](const auto& frame) { return frame->m_client.get() == this; });

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

bool CScreencopyClient::good() {
    return m_resource->resource();
}

CScreencopyProtocol::CScreencopyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CScreencopyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_clients.emplace_back(makeShared<CScreencopyClient>(makeShared<CZwlrScreencopyManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        LOGM(LOG, "Failed to bind client! (out of memory)");
        CLIENT->m_resource->noMemory();
        m_clients.pop_back();
        return;
    }

    CLIENT->m_self = CLIENT;

    LOGM(LOG, "Bound client successfully!");
}

void CScreencopyProtocol::destroyResource(CScreencopyClient* client) {
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
    std::erase_if(m_frames, [&](const auto& other) { return other->m_client.get() == client; });
    std::erase_if(m_framesAwaitingWrite, [&](const auto& other) { return !other || other->m_client.get() == client; });
}

void CScreencopyProtocol::destroyResource(CScreencopyFrame* frame) {
    std::erase_if(m_frames, [&](const auto& other) { return other.get() == frame; });
    std::erase_if(m_framesAwaitingWrite, [&](const auto& other) { return !other || other.get() == frame; });
}

void CScreencopyProtocol::onOutputCommit(PHLMONITOR pMonitor) {
    if (m_framesAwaitingWrite.empty()) {
        g_pHyprRenderer->m_directScanoutBlocked = false;
        return; // nothing to share
    }

    std::vector<WP<CScreencopyFrame>> framesToRemove;
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

        // otherwise share. If it's denied, it will be black.

        if (!f->m_monitor || !f->m_buffer) {
            framesToRemove.emplace_back(f);
            continue;
        }

        if (f->m_monitor != pMonitor)
            continue;

        f->share();

        f->m_client->m_lastFrame.reset();
        ++f->m_client->m_frameCounter;

        framesToRemove.emplace_back(f);
    }

    for (auto const& f : framesToRemove) {
        std::erase(m_framesAwaitingWrite, f);
    }
}
