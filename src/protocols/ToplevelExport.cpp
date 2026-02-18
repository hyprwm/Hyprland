#include "ToplevelExport.hpp"
#include "../Compositor.hpp"
#include "ForeignToplevelWlr.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../managers/HookSystemManager.hpp"
#include "../helpers/Format.hpp"
#include "../render/Renderer.hpp"

#include <hyprutils/math/Vector2D.hpp>

using namespace Screenshare;

CToplevelExportClient::CToplevelExportClient(SP<CHyprlandToplevelExportManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) {
        Screenshare::mgr()->destroyClientSessions(m_savedClient);
        PROTO::toplevelExport->destroyResource(this);
    });
    m_resource->setDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setCaptureToplevel([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, uint32_t handle) {
        captureToplevel(frame, overlayCursor, g_pCompositor->getWindowFromHandle(handle));
    });
    m_resource->setCaptureToplevelWithWlrToplevelHandle([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* handle) {
        captureToplevel(frame, overlayCursor, PROTO::foreignToplevelWlr->windowFromHandleResource(handle));
    });

    m_savedClient = m_resource->client();
}

CToplevelExportClient::~CToplevelExportClient() {
    Screenshare::mgr()->destroyClientSessions(m_savedClient);
}

void CToplevelExportClient::captureToplevel(uint32_t frame, int32_t overlayCursor_, PHLWINDOW handle) {
    auto session = Screenshare::mgr()->getManagedSession(m_resource->client(), handle);

    // create a frame
    const auto FRAME = PROTO::toplevelExport->m_frames.emplace_back(
        makeShared<CToplevelExportFrame>(makeShared<CHyprlandToplevelExportFrameV1>(m_resource->client(), m_resource->version(), frame), session, !!overlayCursor_));

    if UNLIKELY (!FRAME->good()) {
        LOGM(Log::ERR, "Couldn't alloc frame for sharing! (no memory)");
        m_resource->noMemory();
        PROTO::toplevelExport->destroyResource(FRAME.get());
        return;
    }

    FRAME->m_client = m_self;
    FRAME->m_self   = FRAME;
}

bool CToplevelExportClient::good() {
    return m_resource && m_resource->resource();
}

CToplevelExportFrame::CToplevelExportFrame(SP<CHyprlandToplevelExportFrameV1> resource_, WP<CScreenshareSession> session, bool overlayCursor) :
    m_resource(resource_), m_session(session) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setCopy([this](CHyprlandToplevelExportFrameV1* pFrame, wl_resource* res, int32_t ignoreDamage) { shareFrame(res, !!ignoreDamage); });

    m_listeners.stopped = m_session->m_events.stopped.listen([this]() {
        if (good())
            m_resource->sendFailed();
    });

    m_frame = m_session->nextFrame(overlayCursor);

    auto formats = m_session->allowedFormats();
    if (formats.empty()) {
        LOGM(Log::ERR, "No format supported by renderer in toplevel export protocol");
        m_resource->sendFailed();
        return;
    }

    DRMFormat  format  = formats.at(0);
    auto       bufSize = m_frame->bufferSize();

    const auto PSHMINFO = NFormatUtils::getPixelFormatFromDRM(format);
    const auto stride   = NFormatUtils::minStride(PSHMINFO, bufSize.x);
    m_resource->sendBuffer(NFormatUtils::drmToShm(format), bufSize.x, bufSize.y, stride);

    if LIKELY (format != DRM_FORMAT_INVALID)
        m_resource->sendLinuxDmabuf(format, bufSize.x, bufSize.y);

    m_resource->sendBufferDone();
}

bool CToplevelExportFrame::good() {
    return m_resource && m_resource->resource();
}

void CToplevelExportFrame::shareFrame(wl_resource* buffer, bool ignoreDamage) {
    if UNLIKELY (!good()) {
        LOGM(Log::ERR, "No frame in shareFrame??");
        return;
    }

    if UNLIKELY (m_buffer) {
        LOGM(Log::ERR, "Buffer used in {:x}", (uintptr_t)this);
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        m_resource->sendFailed();
        return;
    }

    const auto PBUFFERRES = CWLBufferResource::fromResource(buffer);
    if UNLIKELY (!PBUFFERRES || !PBUFFERRES->m_buffer) {
        LOGM(Log::ERR, "Invalid buffer in {:x}", (uintptr_t)this);
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        m_resource->sendFailed();
        return;
    }

    const auto& PBUFFER = PBUFFERRES->m_buffer.lock();

    if (ignoreDamage)
        g_pHyprRenderer->damageMonitor(m_session->monitor());

    auto error = m_frame->share(PBUFFER, {}, [this, ignoreDamage, self = m_self](eScreenshareResult result) {
        if (self.expired() || !good())
            return;
        switch (result) {
            case RESULT_COPIED: {
                m_resource->sendFlags(sc<hyprlandToplevelExportFrameV1Flags>(0));
                if (!ignoreDamage)
                    m_frame->damage().forEachRect([&](const auto& rect) { m_resource->sendDamage(rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1); });

                const auto [sec, nsec] = Time::secNsec(m_timestamp);
                uint32_t tvSecHi       = (sizeof(sec) > 4) ? sec >> 32 : 0;
                uint32_t tvSecLo       = sec & 0xFFFFFFFF;
                m_resource->sendReady(tvSecHi, tvSecLo, nsec);
                break;
            }
            case RESULT_NOT_COPIED:
                LOGM(Log::ERR, "Frame share failed in {:x}", (uintptr_t)this);
                m_resource->sendFailed();
                break;
            case RESULT_TIMESTAMP: m_timestamp = Time::steadyNow(); break;
        }
    });

    switch (error) {
        case ERROR_NONE: m_buffer = CHLBufferReference(PBUFFER); break;
        case ERROR_NO_BUFFER: m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer"); break;
        case ERROR_BUFFER_SIZE:
        case ERROR_BUFFER_FORMAT: m_resource->sendFailed(); break;
        case ERROR_UNKNOWN:
        case ERROR_STOPPED: m_resource->sendFailed(); break;
    }
}

CToplevelExportProtocol::CToplevelExportProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CToplevelExportProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_clients.emplace_back(makeShared<CToplevelExportClient>(makeShared<CHyprlandToplevelExportManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        LOGM(Log::DEBUG, "Failed to bind client! (out of memory)");
        wl_client_post_no_memory(client);
        m_clients.pop_back();
        return;
    }

    CLIENT->m_self = CLIENT;

    LOGM(Log::DEBUG, "Bound client successfully!");
}

void CToplevelExportProtocol::destroyResource(CToplevelExportClient* client) {
    std::erase_if(m_frames, [&](const auto& other) { return other->m_client.get() == client; });
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
}

void CToplevelExportProtocol::destroyResource(CToplevelExportFrame* frame) {
    std::erase_if(m_frames, [&](const auto& other) { return other.get() == frame; });
}
