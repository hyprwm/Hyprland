#include "ImageCopyCapture.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../managers/permissions/DynamicPermissionManager.hpp"
#include "../managers/PointerManager.hpp"
#include "./core/Seat.hpp"
#include "LinuxDMABUF.hpp"
#include "../desktop/view/Window.hpp"
#include "../render/OpenGL.hpp"
#include "../desktop/state/FocusState.hpp"
#include <cstring>

using namespace Screenshare;

CImageCopyCaptureSession::CImageCopyCaptureSession(SP<CExtImageCopyCaptureSessionV1> resource, SP<CImageCaptureSource> source, extImageCopyCaptureManagerV1Options options) :
    m_resource(resource), m_source(source), m_paintCursor(options & EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CExtImageCopyCaptureSessionV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtImageCopyCaptureSessionV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });

    m_resource->setCreateFrame([this](CExtImageCopyCaptureSessionV1* pMgr, uint32_t id) {
        if (!m_frame.expired()) {
            LOGM(Log::ERR, "Duplicate frame in session for source: \"{}\"", m_source->getName());
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_SESSION_V1_ERROR_DUPLICATE_FRAME, "duplicate frame");
            return;
        }

        auto PFRAME = PROTO::imageCopyCapture->m_frames.emplace_back(
            makeShared<CImageCopyCaptureFrame>(makeShared<CExtImageCopyCaptureFrameV1>(pMgr->client(), pMgr->version(), id), m_self));

        m_frame = PFRAME;
    });

    if (m_source->m_monitor)
        m_session = Screenshare::mgr()->newSession(m_resource->client(), m_source->m_monitor.lock());
    else
        m_session = Screenshare::mgr()->newSession(m_resource->client(), m_source->m_window.lock());

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

bool CImageCopyCaptureSession::good() {
    return m_resource && m_resource->resource();
}

void CImageCopyCaptureSession::sendConstraints() {
    auto formats = m_session->allowedFormats();

    if UNLIKELY (formats.empty()) {
        m_session->stop();
        m_resource->error(-1, "no formats available");
        return;
    }

    for (DRMFormat format : formats) {
        m_resource->sendShmFormat(NFormatUtils::drmToShm(format));

        auto     modifiers = g_pHyprOpenGL->getDRMFormatModifiers(format);

        wl_array modsArr;
        wl_array_init(&modsArr);
        if (!modifiers.empty()) {
            wl_array_add(&modsArr, modifiers.size() * sizeof(uint64_t));
            memcpy(modsArr.data, modifiers.data(), modifiers.size() * sizeof(uint64_t));
        }
        m_resource->sendDmabufFormat(format, &modsArr);
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
    if UNLIKELY (!good())
        return;

    if (!m_source || (!m_source->m_monitor && !m_source->m_window))
        return;

    const auto PMONITOR = m_source->m_monitor.expired() ? m_source->m_window->m_monitor.lock() : m_source->m_monitor.lock();

    // TODO: add listeners for source being destroyed

    sendCursorEvents();
    m_listeners.commit = PMONITOR->m_events.commit.listen([this, PMONITOR]() { sendCursorEvents(); });

    m_resource->setDestroy([this](CExtImageCopyCaptureCursorSessionV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtImageCopyCaptureCursorSessionV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });

    m_resource->setGetCaptureSession([this](CExtImageCopyCaptureCursorSessionV1* pMgr, uint32_t id) {
        if (m_session || m_sessionResource) {
            LOGM(Log::ERR, "Duplicate cursor copy capture session for source: \"{}\"", m_source->getName());
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_CURSOR_SESSION_V1_ERROR_DUPLICATE_SESSION, "duplicate session");
            return;
        }

        m_sessionResource = makeShared<CExtImageCopyCaptureSessionV1>(pMgr->client(), pMgr->version(), id);

        m_sessionResource->setDestroy([this](CExtImageCopyCaptureSessionV1* pMgr) { destroyCaptureSession(); });
        m_sessionResource->setOnDestroy([this](CExtImageCopyCaptureSessionV1* pMgr) { destroyCaptureSession(); });

        m_sessionResource->setCreateFrame([this](CExtImageCopyCaptureSessionV1* pMgr, uint32_t id) {
            if UNLIKELY (!m_session || !m_sessionResource)
                return;

            if (m_frameResource) {
                LOGM(Log::ERR, "Duplicate frame in session for source: \"{}\"", m_source->getName());
                m_resource->error(EXT_IMAGE_COPY_CAPTURE_SESSION_V1_ERROR_DUPLICATE_FRAME, "duplicate frame");
                return;
            }

            createFrame(makeShared<CExtImageCopyCaptureFrameV1>(pMgr->client(), pMgr->version(), id));
        });

        m_session = Screenshare::mgr()->newCursorSession(pMgr->client(), m_pointer);
        if UNLIKELY (!m_session) {
            m_sessionResource->sendStopped();
            m_sessionResource->error(-1, "unable to share cursor");
            return;
        }

        sendConstraints();

        m_listeners.constraintsChanged = m_session->m_events.constraintsChanged.listen([this]() { sendConstraints(); });
        m_listeners.stopped            = m_session->m_events.stopped.listen([this]() { destroyCaptureSession(); });
    });
}

CImageCopyCaptureCursorSession::~CImageCopyCaptureCursorSession() {
    destroyCaptureSession();
}

bool CImageCopyCaptureCursorSession::good() {
    return m_resource && m_resource->resource();
}

void CImageCopyCaptureCursorSession::destroyCaptureSession() {
    m_listeners.constraintsChanged.reset();
    m_listeners.stopped.reset();

    if (m_frameResource && m_frameResource->resource())
        m_frameResource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED);
    m_frameResource.reset();

    m_sessionResource.reset();
    m_session.reset();
}

void CImageCopyCaptureCursorSession::createFrame(SP<CExtImageCopyCaptureFrameV1> resource) {
    m_frameResource = resource;
    m_captured      = false;
    m_buffer.reset();

    m_frameResource->setDestroy([this](CExtImageCopyCaptureFrameV1* pMgr) { m_frameResource.reset(); });
    m_frameResource->setOnDestroy([this](CExtImageCopyCaptureFrameV1* pMgr) { m_frameResource.reset(); });

    m_frameResource->setAttachBuffer([this](CExtImageCopyCaptureFrameV1* pMgr, wl_resource* buf) {
        if UNLIKELY (!m_frameResource || !m_frameResource->resource())
            return;

        if (m_captured) {
            LOGM(Log::ERR, "Frame already captured in attach_buffer, {:x}", (uintptr_t)this);
            m_frameResource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            m_frameResource.reset();
            return;
        }

        auto PBUFFERRES = CWLBufferResource::fromResource(buf);
        if (!PBUFFERRES || !PBUFFERRES->m_buffer) {
            LOGM(Log::ERR, "Invalid buffer in attach_buffer {:x}", (uintptr_t)this);
            m_frameResource->error(-1, "invalid buffer");
            m_frameResource.reset();
            return;
        }

        m_buffer = PBUFFERRES->m_buffer.lock();
    });

    m_frameResource->setDamageBuffer([this](CExtImageCopyCaptureFrameV1* pMgr, int32_t x, int32_t y, int32_t w, int32_t h) {
        if UNLIKELY (!m_frameResource || !m_frameResource->resource())
            return;

        if (m_captured) {
            LOGM(Log::ERR, "Frame already captured in damage_buffer, {:x}", (uintptr_t)this);
            m_frameResource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            m_frameResource.reset();
            return;
        }

        if (x < 0 || y < 0 || w <= 0 || h <= 0) {
            m_frameResource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_INVALID_BUFFER_DAMAGE, "invalid buffer damage");
            m_frameResource.reset();
            return;
        }

        // we don't really need to keep track of damage for cursor frames because we will just copy the whole thing
    });

    m_frameResource->setCapture([this](CExtImageCopyCaptureFrameV1* pMgr) {
        if UNLIKELY (!m_frameResource || !m_frameResource->resource())
            return;

        if (m_captured) {
            LOGM(Log::ERR, "Frame already captured in capture, {:x}", (uintptr_t)this);
            m_frameResource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            m_frameResource.reset();
            return;
        }

        const auto PMONITOR = m_source->m_monitor.expired() ? m_source->m_window->m_monitor.lock() : m_source->m_monitor.lock();

        auto       sourceBoxCallback = [this]() { return m_source ? m_source->logicalBox() : CBox(); };
        auto       error             = m_session->share(PMONITOR, m_buffer, sourceBoxCallback, [this](eScreenshareResult result) {
            switch (result) {
                case RESULT_COPIED: m_frameResource->sendReady(); break;
                case RESULT_NOT_COPIED: m_frameResource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN); break;
                case RESULT_TIMESTAMP:
                    auto [sec, nsec] = Time::secNsec(Time::steadyNow());
                    uint32_t tvSecHi = (sizeof(sec) > 4) ? sec >> 32 : 0;
                    uint32_t tvSecLo = sec & 0xFFFFFFFF;
                    m_frameResource->sendPresentationTime(tvSecHi, tvSecLo, nsec);
                    break;
            }
        });

        if (!m_frameResource)
            return;

        switch (error) {
            case ERROR_NONE: m_captured = true; break;
            case ERROR_NO_BUFFER:
                m_frameResource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_NO_BUFFER, "no buffer attached");
                m_frameResource.reset();
                break;
            case ERROR_BUFFER_SIZE:
            case ERROR_BUFFER_FORMAT: m_frameResource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS); break;
            case ERROR_STOPPED: m_frameResource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED); break;
            case ERROR_UNKNOWN: m_frameResource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN); break;
        }
    });

    // we should always copy over the entire cursor image, it doesn't cost much
    m_frameResource->sendDamage(0, 0, m_bufferSize.x, m_bufferSize.y);

    // the cursor is never transformed... probably?
    m_frameResource->sendTransform(WL_OUTPUT_TRANSFORM_NORMAL);
}

void CImageCopyCaptureCursorSession::sendConstraints() {
    if UNLIKELY (!m_session || !m_sessionResource)
        return;

    auto format = m_session->format();
    if UNLIKELY (format == DRM_FORMAT_INVALID) {
        m_session->stop();
        m_sessionResource->error(-1, "no formats available");
        return;
    }

    m_sessionResource->sendShmFormat(NFormatUtils::drmToShm(format));

    auto     modifiers = g_pHyprOpenGL->getDRMFormatModifiers(format);

    wl_array modsArr;
    wl_array_init(&modsArr);
    if (!modifiers.empty()) {
        wl_array_add(&modsArr, modifiers.size() * sizeof(uint64_t));
        memcpy(modsArr.data, modifiers.data(), modifiers.size() * sizeof(uint64_t));
    }
    m_sessionResource->sendDmabufFormat(format, &modsArr);
    wl_array_release(&modsArr);

    dev_t           device    = PROTO::linuxDma->getMainDevice();
    struct wl_array deviceArr = {
        .size = sizeof(device),
        .data = sc<void*>(&device),
    };
    m_sessionResource->sendDmabufDevice(&deviceArr);

    m_bufferSize = m_session->bufferSize();
    m_sessionResource->sendBufferSize(m_bufferSize.x, m_bufferSize.y);

    m_sessionResource->sendDone();
}

void CImageCopyCaptureCursorSession::sendCursorEvents() {
    const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(m_resource->client(), PERMISSION_TYPE_CURSOR_POS);
    if (PERM != PERMISSION_RULE_ALLOW_MODE_ALLOW) {
        if (PERM == PERMISSION_RULE_ALLOW_MODE_DENY) {
            m_resource->error(-1, "client not allowed to capture cursor");
            PROTO::imageCopyCapture->destroyResource(this);
        }
        return;
    }

    const auto PMONITOR  = m_source->m_monitor.expired() ? m_source->m_window->m_monitor.lock() : m_source->m_monitor.lock();
    CBox       sourceBox = m_source->logicalBox();
    bool       overlaps  = g_pPointerManager->getCursorBoxGlobal().overlaps(sourceBox);

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

    Vector2D pos = g_pPointerManager->position() - sourceBox.pos();
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
    if UNLIKELY (!good())
        return;

    if (m_session->m_bufferSize != m_session->m_session->bufferSize()) {
        m_session->sendConstraints();
        m_resource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN);
        return;
    }

    m_frame = m_session->m_session->nextFrame(m_session->m_paintCursor);

    m_resource->setDestroy([this](CExtImageCopyCaptureFrameV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtImageCopyCaptureFrameV1* pMgr) { PROTO::imageCopyCapture->destroyResource(this); });

    m_resource->setAttachBuffer([this](CExtImageCopyCaptureFrameV1* pMgr, wl_resource* buf) {
        if (m_captured) {
            LOGM(Log::ERR, "Frame already captured in attach_buffer, {:x}", (uintptr_t)this);
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            return;
        }

        auto PBUFFERRES = CWLBufferResource::fromResource(buf);
        if (!PBUFFERRES || !PBUFFERRES->m_buffer) {
            LOGM(Log::ERR, "Invalid buffer in attach_buffer {:x}", (uintptr_t)this);
            m_resource->error(-1, "invalid buffer");
            return;
        }

        m_buffer = PBUFFERRES->m_buffer.lock();
    });

    m_resource->setDamageBuffer([this](CExtImageCopyCaptureFrameV1* pMgr, int32_t x, int32_t y, int32_t w, int32_t h) {
        if (m_captured) {
            LOGM(Log::ERR, "Frame already captured in damage_buffer, {:x}", (uintptr_t)this);
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            return;
        }

        if (x < 0 || y < 0 || w <= 0 || h <= 0) {
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_INVALID_BUFFER_DAMAGE, "invalid buffer damage");
            return;
        }

        m_clientDamage.add(x, y, w, h);
    });

    m_resource->setCapture([this](CExtImageCopyCaptureFrameV1* pMgr) {
        if (m_captured) {
            LOGM(Log::ERR, "Frame already captured in capture, {:x}", (uintptr_t)this);
            m_resource->error(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED, "already captured");
            return;
        }

        auto error = m_frame->share(m_buffer, m_clientDamage, [this](eScreenshareResult result) {
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
            case ERROR_UNKNOWN: m_resource->sendFailed(EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN); break;
        }
    });

    m_clientDamage.clear();

    // TODO: see ScreenshareFrame::share() for "add a damage ring for output damage since last shared frame"
    m_resource->sendDamage(0, 0, m_session->m_bufferSize.x, m_session->m_bufferSize.y);

    m_resource->sendTransform(m_frame->transform());
}

CImageCopyCaptureFrame::~CImageCopyCaptureFrame() {
    if (m_session)
        m_session->m_frame.reset();
}

bool CImageCopyCaptureFrame::good() {
    return m_resource && m_resource->resource();
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
            LOGM(Log::ERR, "Client tried to create image copy capture session from invalid source");
            pMgr->error(-1, "invalid image capture source");
            return;
        }

        if (options > 1) {
            LOGM(Log::ERR, "Client tried to create image copy capture session with invalid options");
            pMgr->error(EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_ERROR_INVALID_OPTION, "Options can't be above 1");
            return;
        }

        auto& PSESSION =
            m_sessions.emplace_back(makeShared<CImageCopyCaptureSession>(makeShared<CExtImageCopyCaptureSessionV1>(pMgr->client(), pMgr->version(), id), source, options));
        PSESSION->m_self = PSESSION;
        LOGM(Log::INFO, "New image copy capture session for source ({}): \"{}\"", source->getTypeName(), source->getName());
    });

    RESOURCE->setCreatePointerCursorSession([this](CExtImageCopyCaptureManagerV1* pMgr, uint32_t id, wl_resource* source_, wl_resource* pointer_) {
        SP<CImageCaptureSource> source = PROTO::imageCaptureSource->sourceFromResource(source_);
        if (!source) {
            LOGM(Log::ERR, "Client tried to create image copy capture session from invalid source");
            destroyResource(pMgr);
            return;
        }

        const auto PERM = g_pDynamicPermissionManager->clientPermissionMode(pMgr->client(), PERMISSION_TYPE_CURSOR_POS);
        if (PERM == PERMISSION_RULE_ALLOW_MODE_DENY)
            return;

        m_cursorSessions.emplace_back(makeShared<CImageCopyCaptureCursorSession>(makeShared<CExtImageCopyCaptureCursorSessionV1>(pMgr->client(), pMgr->version(), id), source,
                                                                                 CWLPointerResource::fromResource(pointer_)));

        LOGM(Log::INFO, "New image copy capture cursor session for source ({}): \"{}\"", source->getTypeName(), source->getName());
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
