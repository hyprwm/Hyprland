#include "ToplevelExport.hpp"
#include "../Compositor.hpp"
#include "ForeignToplevelWlr.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../helpers/Format.hpp"
#include "../render/Renderer.hpp"

#include <hyprutils/math/Vector2D.hpp>

CToplevelExportClient::CToplevelExportClient(SP<CHyprlandToplevelExportManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandToplevelExportManagerV1* pMgr) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setCaptureToplevel([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, uint32_t handle) {
        captureToplevel(frame, overlayCursor, g_pCompositor->getWindowFromHandle(handle));
    });
    m_resource->setCaptureToplevelWithWlrToplevelHandle([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* handle) {
        captureToplevel(frame, overlayCursor, PROTO::foreignToplevelWlr->windowFromHandleResource(handle));
    });
}

CToplevelExportClient::~CToplevelExportClient() {
    if (good())
        g_pScreenshareManager->destroyClientSessions(m_resource->client());
}

void CToplevelExportClient::captureToplevel(uint32_t frame, int32_t overlayCursor_, PHLWINDOW handle) {
    auto session = g_pScreenshareManager->getManagedSession(m_resource->client(), handle);

    // create a frame
    const auto FRAME = PROTO::toplevelExport->m_frames.emplace_back(
        makeShared<CToplevelExportFrame>(makeShared<CHyprlandToplevelExportFrameV1>(m_resource->client(), m_resource->version(), frame), session, !!overlayCursor_));

    if UNLIKELY (!FRAME->good()) {
        LOGM(ERR, "Couldn't alloc frame for sharing! (no memory)");
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
    m_resource(resource_), m_session(session), m_overlayCursorRequested(overlayCursor) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandToplevelExportFrameV1* pFrame) { PROTO::toplevelExport->destroyResource(this); });
    m_resource->setCopy([this](CHyprlandToplevelExportFrameV1* pFrame, wl_resource* res, int32_t ignoreDamage) { shareFrame(res, !!ignoreDamage); });

    m_listeners.stopped = m_session->m_events.stopped.listen([this]() { PROTO::toplevelExport->destroyResource(this); });

    auto formats = m_session->allowedFormats();
    if (formats.empty()) {
        LOGM(ERR, "No format supported by renderer in toplevel export protocol");
        m_resource->sendFailed();
        return;
    }

    const auto& format    = formats.at(0);
    auto        drmFormat = format.drmFormat;
    auto        bufSize   = m_session->bufferSize();

    // why wasn't this in toplevel originally?
    // TODO: hack, we can't bit flip so we'll format flip heh, GL_BGRA_EXT won't work here
    // if (drmFormat == DRM_FORMAT_XRGB2101010 || drmFormat == DRM_FORMAT_ARGB2101010)
    //        drmFormat = DRM_FORMAT_XBGR2101010;

    const auto PSHMINFO = NFormatUtils::getPixelFormatFromDRM(drmFormat);
    const auto stride   = NFormatUtils::minStride(PSHMINFO, bufSize.x);
    m_resource->sendBuffer(NFormatUtils::drmToShm(drmFormat), bufSize.x, bufSize.y, stride);

    if LIKELY (drmFormat != DRM_FORMAT_INVALID)
        m_resource->sendLinuxDmabuf(drmFormat, bufSize.x, bufSize.y);

    m_resource->sendBufferDone();
}

bool CToplevelExportFrame::good() {
    return m_resource && m_resource->resource();
}

void CToplevelExportFrame::shareFrame(wl_resource* buffer, bool ignoreDamage) {
    LOGM(LOG, "shareFrame for, clients {}, frames {}", PROTO::toplevelExport->m_clients.size(), PROTO::toplevelExport->m_frames.size());

    if UNLIKELY (!good()) {
        LOGM(ERR, "No frame in shareFrame??");
        return;
    }

    if UNLIKELY (m_buffer) {
        LOGM(ERR, "Buffer used in {:x}", (uintptr_t)this);
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        m_resource->sendFailed();
        return;
    }

    const auto PBUFFERRES = CWLBufferResource::fromResource(buffer);
    if UNLIKELY (!PBUFFERRES || !PBUFFERRES->m_buffer) {
        LOGM(ERR, "Invalid buffer in {:x}", (uintptr_t)this);
        m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
        m_resource->sendFailed();
        return;
    }

    const auto& PBUFFER = PBUFFERRES->m_buffer.lock();

    if (ignoreDamage)
        g_pHyprRenderer->damageMonitor(m_session->monitor());

    auto error = m_session->shareNextFrame(PBUFFER, m_overlayCursorRequested, [this, ignoreDamage, self = m_self](eScreenshareResult result) {
        if (self.expired() || !good())
            return;
        switch (result) {
            case RESULT_SHARED: {
                m_resource->sendFlags(sc<hyprlandToplevelExportFrameV1Flags>(0));
                if (!ignoreDamage) {
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
            m_resource->error(HYPRLAND_TOPLEVEL_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
            m_resource->sendFailed();
            break;
        case ERROR_MONITOR:
        case ERROR_WINDOW:
        case ERROR_STOPPED: m_resource->sendFailed(); break;
    }
}

CToplevelExportProtocol::CToplevelExportProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CToplevelExportProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_clients.emplace_back(makeShared<CToplevelExportClient>(makeShared<CHyprlandToplevelExportManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        LOGM(LOG, "Failed to bind client! (out of memory)");
        wl_client_post_no_memory(client);
        m_clients.pop_back();
        return;
    }

    CLIENT->m_self = CLIENT;

    LOGM(LOG, "Bound client successfully!");
}

void CToplevelExportProtocol::destroyResource(CToplevelExportClient* client) {
    std::erase_if(m_frames, [&](const auto& other) { return other->m_client.get() == client; });
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
}

void CToplevelExportProtocol::destroyResource(CToplevelExportFrame* frame) {
    std::erase_if(m_frames, [&](const auto& other) { return other.get() == frame; });
}
