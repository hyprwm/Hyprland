#include "ImageCopyCapture.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../managers/PointerManager.hpp"
#include "./core/Seat.hpp"
#include "LinuxDMABUF.hpp"
#include "../desktop/Window.hpp"
#include <cstring>

CImageCopyCaptureSession::CImageCopyCaptureSession(SP<CExtImageCopyCaptureSessionV1> resource, SP<CImageCaptureSource> source, extImageCopyCaptureManagerV1Options options) :
    m_resource(resource), m_source(source), m_paintCursor(options & EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS) {
    m_resource->setDestroy([this](CExtImageCopyCaptureSessionV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });

    m_resource->setCreateFrame([this](CExtImageCopyCaptureSessionV1* pMgr, uint32_t id) {
        if (!m_frame.expired()) {
            LOGM(LOG, "Duplicate frame in session for source: \"{}\"", m_source->getName());
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_SESSION_V1_ERROR_DUPLICATE_FRAME, "duplicate frame");
            return;
        }

        auto PFRAME = PROTO::imageCopyCapture->m_frames.emplace_back(
            makeShared<CImageCopyCaptureFrame>(makeShared<CExtImageCopyCaptureFrameV1>(pMgr->client(), pMgr->version(), id), m_self));

        m_frame = PFRAME;
    });

    if (m_source->m_monitor)
        m_session = g_pScreenshareManager->newSession(m_resource->client(), m_source->m_monitor.lock());
    else
        m_session = g_pScreenshareManager->newSession(m_resource->client(), m_source->m_window.lock());

    if UNLIKELY (!m_session) {
        m_resource->sendStopped();
        m_resource->error(-1, "unable to share screen");
        return;
    }

    sendConstraints();

    m_listeners.constraintsChanged = m_session->m_events.constraintsChanged.listen([this]() { sendConstraints(); });
    m_listeners.stopped            = m_session->m_events.stopped.listen([this]() { PROTO::imageCopyCapture->destroyResource(this); });
}

CImageCopyCaptureSession::~CImageCopyCaptureSession() {
    if (m_session)
        m_session->stop();
    if (m_resource->resource())
        m_resource->sendStopped();
}

void CImageCopyCaptureSession::sendConstraints() {
    auto formats = m_session->allowedFormats();

    if UNLIKELY (formats.empty()) {
        m_session->stop();
        m_resource->error(-1, "no formats available");
        return;
    }

    for (const auto& format : formats) {
        // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT won't work here
        // if (m_shmFormat == DRM_FORMAT_XRGB2101010 || m_shmFormat == DRM_FORMAT_ARGB2101010)
        //     m_shmFormat = DRM_FORMAT_XBGR2101010;
        m_resource->sendShmFormat(NFormatUtils::drmToShm(format.drmFormat));

        wl_array modsArr;
        wl_array_init(&modsArr);
        if (!format.modifiers.empty()) {
            wl_array_add(&modsArr, format.modifiers.size() * sizeof(uint64_t));
            memcpy(modsArr.data, format.modifiers.data(), format.modifiers.size() * sizeof(uint64_t));
        }
        m_resource->sendDmabufFormat(format.drmFormat, &modsArr);
        wl_array_release(&modsArr);
    }

    dev_t           device    = PROTO::linuxDma->getMainDevice();
    struct wl_array deviceArr = {
        .size = sizeof(device),
        .data = sc<void*>(&device),
    };
    m_resource->sendDmabufDevice(&deviceArr);

    m_bufferSize = m_session->bufferSize();
    m_resource->sendBufferSize(m_bufferSize.x, m_bufferSize.y);

    m_resource->sendDone();
}

CImageCopyCaptureCursorSession::CImageCopyCaptureCursorSession(SP<CExtImageCopyCaptureCursorSessionV1> resource, SP<CImageCaptureSource> source, SP<CWLPointerResource> pointer) :
    m_resource(resource), m_source(source), m_pointer(pointer) {
    if (!m_source || (!m_source->m_monitor && !m_source->m_window))
        return;

    const auto PMONITOR = m_source->m_monitor.expired() ? m_source->m_window->m_monitor.lock() : m_source->m_monitor.lock();

    if (m_source->m_monitor) {
        m_sourceBox = PMONITOR->logicalBox();
    } else {
        m_sourceBox = CBox{m_source->m_window->m_position, m_source->m_window->m_size};
    }

    sendCursorEvents();
    m_listeners.commit = PMONITOR->m_events.commit.listen([this, PMONITOR]() { sendCursorEvents(); });

    m_resource->setDestroy([this](CExtImageCopyCaptureCursorSessionV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });
    m_resource->setGetCaptureSession([this](CExtImageCopyCaptureCursorSessionV1* pMgr, uint32_t id) {
        // TODO: implement this
        m_resource->error(-1, "not implemented");
    });
}

void CImageCopyCaptureCursorSession::sendCursorEvents() {
    const auto PMONITOR = m_source->m_monitor.expired() ? m_source->m_window->m_monitor.lock() : m_source->m_monitor.lock();
    bool       overlaps = m_sourceBox.overlaps(g_pPointerManager->getCursorBoxGlobal());

    if (m_entered && !overlaps) {
        m_entered = false;
        m_resource->sendLeave();
        return;
    } else if (!m_entered && overlaps) {
        m_entered = true;
        m_resource->sendEnter();
    }

    if (!overlaps)
        return;

    Vector2D pos = g_pPointerManager->position() - m_sourceBox.pos();
    if (pos != m_pos) {
        m_pos = pos;
        m_resource->sendPosition(m_pos.x, m_pos.y);
    }

    Vector2D hotspot = g_pPointerManager->hotspot();
    if (hotspot != m_hotspot) {
        m_hotspot = hotspot;
        m_resource->sendHotspot(m_hotspot.x, m_hotspot.y);
    }
}

CImageCopyCaptureFrame::CImageCopyCaptureFrame(SP<CExtImageCopyCaptureFrameV1> resource, WP<CImageCopyCaptureSession> session) : m_resource(resource), m_session(session) {
    if (m_session->m_bufferSize != m_session->m_session->bufferSize()) {
        m_session->sendConstraints();
        m_resource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }

    m_frame = m_session->m_session->nextFrame(m_session->m_paintCursor);

    m_resource->setDestroy([this](CExtImageCopyCaptureFrameV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });

    m_resource->setAttachBuffer([this](CExtImageCopyCaptureFrameV1* pMgr, wl_resource* buf) {
        if (m_captured) {
            LOGM(ERR, "Frame already captured in attach_buffer, {:x}", (uintptr_t)this);
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            return;
        }

        auto PBUFFERRES = CWLBufferResource::fromResource(buf);
        if (!PBUFFERRES || !PBUFFERRES->m_buffer) {
            LOGM(ERR, "Invalid buffer in attach_buffer {:x}", (uintptr_t)this);
            m_resource->error(-1, "invalid buffer");
            return;
        }

        m_buffer = PBUFFERRES->m_buffer.lock();
    });

    // TODO: properly do damage, currently we copy everything every frame
    m_resource->sendDamage(0, 0, INT32_MAX, INT32_MAX);
    m_resource->setDamageBuffer([this](CExtImageCopyCaptureFrameV1* pMgr, int32_t x, int32_t y, int32_t w, int32_t h) {
        if (m_captured) {
            LOGM(ERR, "Frame already captured in damage_buffer, {:x}", (uintptr_t)this);
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            return;
        }

        if (x < 0 || y < 0 || w <= 0 || h <= 0) {
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_INVALID_BUFFER_DAMAGE, "invalid buffer damage");
            return;
        }

        // m_damage.add(x, y, w, h);
    });

    m_resource->setCapture([this](CExtImageCopyCaptureFrameV1* pMgr) {
        if (m_captured) {
            LOGM(ERR, "Frame already captured in capture, {:x}", (uintptr_t)this);
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            return;
        }

        // auto [sec, nsec] = Time::secNsec(Time::steadyNow());
        // uint32_t tvSecHi = (sizeof(sec) > 4) ? sec >> 32 : 0;
        // uint32_t tvSecLo = sec & 0xFFFFFFFF;
        // m_resource->sendPresentationTime(tvSecHi, tvSecLo, nsec);

        auto error = m_frame->share(m_buffer, [this](eScreenshareResult result) {
            switch (result) {
                case RESULT_COPIED: m_resource->sendReady(); break;
                case RESULT_NOT_COPIED: m_resource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN); break;
                case RESULT_TIMESTAMP:
                    auto [sec, nsec] = Time::secNsec(Time::steadyNow());
                    uint32_t tvSecHi = (sizeof(sec) > 4) ? sec >> 32 : 0;
                    uint32_t tvSecLo = sec & 0xFFFFFFFF;
                    m_resource->sendPresentationTime(tvSecHi, tvSecLo, nsec);
                    break;
            }
        });

        switch (error) {
            case ERROR_NONE: m_captured = true; break;
            case ERROR_NO_BUFFER: m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_NO_BUFFER, "no buffer attached"); break;
            case ERROR_BUFFER_SIZE:
            case ERROR_BUFFER_FORMAT: m_resource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS); break;
            case ERROR_STOPPED: m_resource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED); break;
        }
    });
}

CImageCopyCaptureFrame::~CImageCopyCaptureFrame() {
    if (m_session)
        m_session->m_frame.reset();
}

CImageCopyCaptureProtocol::CImageCopyCaptureProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CImageCopyCaptureProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_managers.emplace_back(makeShared<CExtImageCopyCaptureManagerV1>(client, ver, id));

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        m_managers.pop_back();
        return;
    }

    RESOURCE->setDestroy([this](CExtImageCopyCaptureManagerV1* pMgr) { destroyResource(pMgr); });
    RESOURCE->setOnDestroy([this](CExtImageCopyCaptureManagerV1* pMgr) { destroyResource(pMgr); });

    RESOURCE->setCreateSession([this](CExtImageCopyCaptureManagerV1* pMgr, uint32_t id, wl_resource* source_, extImageCopyCaptureManagerV1Options options) {
        auto source = PROTO::imageCaptureSource->sourceFromResource(source_);
        if (!source) {
            LOGM(LOG, "Client tried to create image copy capture session from invalid source");
            pMgr->error(-1, "invalid image capture source");
            return;
        }

        if (options > 1) {
            LOGM(LOG, "Client tried to create image copy capture session with invalid options");
            pMgr->error(EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_ERROR_INVALID_OPTION, "Options can't be above 1");
            return;
        }

        auto& PSESSION =
            m_sessions.emplace_back(makeShared<CImageCopyCaptureSession>(makeShared<CExtImageCopyCaptureSessionV1>(pMgr->client(), pMgr->version(), id), source, options));
        PSESSION->m_self = PSESSION;
        LOGM(LOG, "New image copy capture session for source ({}): \"{}\"", source->getTypeName(), source->getName());
    });

    RESOURCE->setCreatePointerCursorSession([this](CExtImageCopyCaptureManagerV1* pMgr, uint32_t id, wl_resource* source_, wl_resource* pointer_) {
        SP<CImageCaptureSource> source = PROTO::imageCaptureSource->sourceFromResource(source_);
        if (!source) {
            LOGM(LOG, "Client tried to create image copy capture session from invalid source");
            destroyResource(pMgr);
            return;
        }

        m_cursorSessions.emplace_back(makeShared<CImageCopyCaptureCursorSession>(makeShared<CExtImageCopyCaptureCursorSessionV1>(pMgr->client(), pMgr->version(), id), source,
                                                                                 CWLPointerResource::fromResource(pointer_)));

        LOGM(LOG, "New image copy capture cursor session for source ({}): \"{}\"", source->getTypeName(), source->getName());
    });
}

void CImageCopyCaptureProtocol::destroyResource(CExtImageCopyCaptureManagerV1* resource) {
    std::erase_if(m_managers, [&](const auto& other) { return other.get() == resource; });
}

void CImageCopyCaptureProtocol::destroyResource(CImageCopyCaptureSession* resource) {
    std::erase_if(m_sessions, [&](const auto& other) { return other.get() == resource; });
}

void CImageCopyCaptureProtocol::destroyResource(CImageCopyCaptureCursorSession* resource) {
    std::erase_if(m_cursorSessions, [&](const auto& other) { return other.get() == resource; });
}

void CImageCopyCaptureProtocol::destroyResource(CImageCopyCaptureFrame* resource) {
    std::erase_if(m_frames, [&](const auto& other) { return other.get() == resource; });
}
