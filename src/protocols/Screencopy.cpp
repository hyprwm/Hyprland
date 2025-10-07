#include "Screencopy.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "core/Output.hpp"
#include "../render/Renderer.hpp"
#include "types/Buffer.hpp"
#include "../helpers/Format.hpp"
#include "../helpers/time/Time.hpp"

CScreencopyClient::CScreencopyClient(SP<CZwlrScreencopyManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setDestroy([this](CZwlrScreencopyManagerV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    m_resource->setOnDestroy([this](CZwlrScreencopyManagerV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    m_resource->setCaptureOutput(
        [this](CZwlrScreencopyManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* output) { captureOutput(frame, overlayCursor, output, {}); });
    m_resource->setCaptureOutputRegion([this](CZwlrScreencopyManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* output, int32_t x, int32_t y, int32_t w,
                                              int32_t h) { captureOutput(frame, overlayCursor, output, {x, y, w, h}); });
}

CScreencopyClient::~CScreencopyClient() {
    if (good())
        g_pScreenshareManager->destroyClientSessions(m_resource->client());
}

void CScreencopyClient::captureOutput(uint32_t frame, int32_t overlayCursor_, wl_resource* output, CBox box) {
    const auto PMONITORRES = CWLOutputResource::fromResource(output);
    if (!PMONITORRES || !PMONITORRES->m_monitor) {
        LOGM(ERR, "Tried to capture invalid output/monitor in {:x}", (uintptr_t)this);
        m_resource->error(-1, "invalid output");
        return;
    }

    const auto PMONITOR = PMONITORRES->m_monitor.lock();
    auto       session  = box.w == 0 && box.h == 0 ? g_pScreenshareManager->getManagedSession(m_resource->client(), PMONITOR) :
                                                     g_pScreenshareManager->getManagedSession(m_resource->client(), PMONITOR, box);

    const auto FRAME = PROTO::screencopy->m_frames.emplace_back(
        makeShared<CScreencopyFrame>(makeShared<CZwlrScreencopyFrameV1>(m_resource->client(), m_resource->version(), frame), session, !!overlayCursor_));

    if (!FRAME->good()) {
        LOGM(ERR, "Couldn't alloc frame for sharing! (no memory)");
        m_resource->noMemory();
        PROTO::screencopy->destroyResource(FRAME.get());
        return;
    }

    FRAME->m_client = m_self;
    FRAME->m_self   = FRAME;
}

bool CScreencopyClient::good() {
    return m_resource && m_resource->resource();
}

CScreencopyFrame::CScreencopyFrame(SP<CZwlrScreencopyFrameV1> resource_, WP<CScreenshareSession> session, bool overlayCursor) :
    m_resource(resource_), m_session(session), m_overlayCursor(overlayCursor) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CZwlrScreencopyFrameV1* pMgr) { PROTO::screencopy->destroyResource(this); });
    m_resource->setDestroy([this](CZwlrScreencopyFrameV1* pFrame) { PROTO::screencopy->destroyResource(this); });
    m_resource->setCopy([this](CZwlrScreencopyFrameV1* pFrame, wl_resource* res) { shareFrame(pFrame, res, false); });
    m_resource->setCopyWithDamage([this](CZwlrScreencopyFrameV1* pFrame, wl_resource* res) { shareFrame(pFrame, res, true); });

    m_listeners.stopped = m_session->m_events.stopped.listen([this]() { PROTO::screencopy->destroyResource(this); });

    auto formats = m_session->allowedFormats();
    if (formats.empty()) {
        LOGM(ERR, "No format supported by renderer in screencopy protocol");
        m_resource->sendFailed();
        return;
    }

    const auto& format    = formats.at(0);
    auto        drmFormat = format.drmFormat;
    auto        bufSize   = m_session->bufferSize();

    // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT won't work here
    if (drmFormat == DRM_FORMAT_XRGB2101010 || drmFormat == DRM_FORMAT_ARGB2101010)
        drmFormat = DRM_FORMAT_XBGR2101010;

    const auto PSHMINFO = NFormatUtils::getPixelFormatFromDRM(drmFormat);
    const auto stride   = NFormatUtils::minStride(PSHMINFO, bufSize.x);
    m_resource->sendBuffer(NFormatUtils::drmToShm(drmFormat), bufSize.x, bufSize.y, stride);

    if (m_resource->version() >= 3) {
        if LIKELY (drmFormat != DRM_FORMAT_INVALID)
            m_resource->sendLinuxDmabuf(drmFormat, bufSize.x, bufSize.y);

        m_resource->sendBufferDone();
    }
}

void CScreencopyFrame::shareFrame(CZwlrScreencopyFrameV1* pFrame, wl_resource* buffer, bool withDamage) {
    LOGM(LOG, "shareFrame for, clients {}, frames {}", PROTO::screencopy->m_clients.size(), PROTO::screencopy->m_frames.size());

    if UNLIKELY (!good()) {
        LOGM(ERR, "No frame in shareFrame??");
        return;
    }

    if UNLIKELY (m_buffer) {
        LOGM(ERR, "Buffer used in {:x}", (uintptr_t)this);
        m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        m_resource->sendFailed();
        return;
    }

    const auto PBUFFERRES = CWLBufferResource::fromResource(buffer);
    if UNLIKELY (!PBUFFERRES || !PBUFFERRES->m_buffer) {
        LOGM(ERR, "Invalid buffer in {:x}", (uintptr_t)this);
        m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        m_resource->sendFailed();
        return;
    }

    const auto& PBUFFER = PBUFFERRES->m_buffer.lock();

    if (!withDamage)
        g_pHyprRenderer->damageMonitor(m_session->monitor());

    auto error = m_session->shareNextFrame(PBUFFER, m_overlayCursor, [this, withDamage, self = m_self](eScreenshareResult result) {
        if (self.expired() || !good())
            return;
        switch (result) {
            case RESULT_SHARED: {
                m_resource->sendFlags(sc<zwlrScreencopyFrameV1Flags>(0));
                if (withDamage) {
                    // TODO: add a damage ring for this.
                    m_resource->sendDamage(0, 0, m_buffer->size.x, m_buffer->size.y);
                }

                const auto [sec, nsec] = Time::secNsec(m_timestamp);
                uint32_t tvSecHi       = (sizeof(sec) > 4) ? sec >> 32 : 0;
                uint32_t tvSecLo       = sec & 0xFFFFFFFF;
                m_resource->sendReady(tvSecHi, tvSecLo, nsec);
                break;
            }
            case RESULT_NOT_SHARED:
                LOGM(ERR, "Frame share failed in {:x}", (uintptr_t)this);
                m_resource->sendFailed();
                break;
            case RESULT_TIMESTAMP: m_timestamp = Time::steadyNow(); break;
        }
    });

    switch (error) {
        case ERROR_NONE: m_buffer = CHLBufferReference(PBUFFER); break;
        case ERROR_BUFFER:
        case ERROR_BUFFER_SIZE:
        case ERROR_BUFFER_FORMAT:
            m_resource->error(ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
            m_resource->sendFailed();
            break;
        case ERROR_MONITOR:
        case ERROR_WINDOW:
        case ERROR_STOPPED: m_resource->sendFailed(); break;
    }
}

bool CScreencopyFrame::good() {
    return m_resource && m_resource->resource();
}

CScreencopyProtocol::CScreencopyProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CScreencopyProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_clients.emplace_back(makeShared<CScreencopyClient>(makeShared<CZwlrScreencopyManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        wl_client_post_no_memory(CLIENT->m_resource->client());
        m_clients.pop_back();
        return;
    }

    CLIENT->m_self = CLIENT;

    LOGM(LOG, "Bound client successfully!");
}

void CScreencopyProtocol::destroyResource(CScreencopyClient* client) {
    std::erase_if(m_frames, [&](const auto& other) { return other->m_client.get() == client; });
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
}

void CScreencopyProtocol::destroyResource(CScreencopyFrame* frame) {
    std::erase_if(m_frames, [&](const auto& other) { return other.get() == frame; });
}
