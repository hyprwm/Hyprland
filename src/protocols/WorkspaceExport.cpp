#include "desktop/DesktopTypes.hpp"
#include "hyprland-workspace-export-v1.hpp"
#include "WaylandProtocol.hpp"
#include "WorkspaceExport.hpp"
#include "../managers/screenshare/ScreenshareManager.hpp"
#include "../Compositor.hpp"
#include <hyprgraphics/egl/Egl.hpp>

// class CHyprlandWorkspaceExportManagerV1;
// class CHyprlandWorkspaceExportFrameV1;
// class CHyprlandWorkspaceExportFrameV1;

using namespace Hyprgraphics::Egl;
using namespace Screenshare;

CWorkspaceExportClient::CWorkspaceExportClient(SP<CHyprlandWorkspaceExportManagerV1> resource_) : m_resource(resource_) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandWorkspaceExportManagerV1* pMgr) { PROTO::workspaceExport->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandWorkspaceExportManagerV1* pMgr) { PROTO::workspaceExport->destroyResource(this); });

    m_resource->setCaptureWorkspace([this](CHyprlandWorkspaceExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, const char* workspace_name) {
        captureWorkspace(frame, overlayCursor, g_pCompositor->getWorkspaceByName(workspace_name));
    });
    //m_resource->setCaptureToplevelWithWlrToplevelHandle([this](CHyprlandToplevelExportManagerV1* pMgr, uint32_t frame, int32_t overlayCursor, wl_resource* handle) {
    //    captureToplevel(frame, overlayCursor, PROTO::foreignToplevelWlr->windowFromHandleResource(handle));
    //});
    m_savedClient = m_resource->client();
}

void CWorkspaceExportClient::captureWorkspace(uint32_t frame, int32_t overlayCursor, PHLWORKSPACE workspace){

    if UNLIKELY (!workspace) {
        LOGM(Log::ERR, "Couldn't capture (workspace doesn't exist)");
        return;
    }

    auto session = Screenshare::mgr()->getManagedSession(m_resource->client(), workspace);

    // create a frame
    const auto FRAME = PROTO::workspaceExport->m_frames.emplace_back(
        makeShared<CWorkspaceExportFrame>(makeShared<CHyprlandWorkspaceExportFrameV1>(m_resource->client(), m_resource->version(), frame), session, 
            !!overlayCursor));

    if UNLIKELY (!FRAME->good()) {
        LOGM(Log::ERR, "Couldn't alloc frame for sharing! (no memory)");
        m_resource->noMemory();
        PROTO::workspaceExport->destroyResource(FRAME.get());
        return;
    }

    FRAME->m_client = m_self;
    FRAME->m_self   = FRAME;
}

bool CWorkspaceExportClient::good() {
    return m_resource && m_resource->resource();
}

CWorkspaceExportFrame::CWorkspaceExportFrame(SP<CHyprlandWorkspaceExportFrameV1> resource_, WP<CScreenshareSession> session, bool overlayCursor) :
    m_resource(resource_), m_session(session) {
    if UNLIKELY (!good())
        return;

    m_resource->setOnDestroy([this](CHyprlandWorkspaceExportFrameV1* pFrame) { PROTO::workspaceExport->destroyResource(this); });
    m_resource->setDestroy([this](CHyprlandWorkspaceExportFrameV1* pFrame) { PROTO::workspaceExport->destroyResource(this); });
    m_resource->setCopy([this](CHyprlandWorkspaceExportFrameV1* pFrame, wl_resource* res, int32_t ignoreDamage) { shareFrame(res, !!ignoreDamage); });

    m_frame = m_session->nextFrame(overlayCursor);

    auto formats = m_session->allowedFormats();
    if (formats.empty()) {
        LOGM(Log::ERR, "No format supported by renderer in toplevel export protocol");
        m_resource->sendFailed();
        return;
    }

    DRMFormat  format  = formats.at(0);
    auto       bufSize = m_frame->bufferSize();

    const auto PSHMINFO = getPixelFormatFromDRM(format);
    const auto stride   = minStride(PSHMINFO, bufSize.x);
    m_resource->sendBuffer(NFormatUtils::drmToShm(format), bufSize.x, bufSize.y, stride);

    if LIKELY (format != DRM_FORMAT_INVALID)
        m_resource->sendLinuxDmabuf(format, bufSize.x, bufSize.y);

    m_resource->sendBufferDone();
}

bool CWorkspaceExportFrame::good() {
    return m_resource && m_resource->resource();
}

void CWorkspaceExportFrame::shareFrame(wl_resource* buffer, bool ignoreDamage) {
    if UNLIKELY (!good()) {
        LOGM(Log::ERR, "No frame in shareFrame??");
        return;
    }

    if UNLIKELY (m_session.expired() || !m_session->monitor()) {
        LOGM(Log::ERR, "Session stopped for frame {:x}", (uintptr_t)this);
        m_resource->sendFailed();
        return;
    }

    if UNLIKELY (m_buffer) {
        LOGM(Log::ERR, "Buffer used in {:x}", (uintptr_t)this);
        m_resource->error(HYPRLAND_WORKSPACE_EXPORT_FRAME_V1_ERROR_ALREADY_USED, "frame already used");
        m_resource->sendFailed();
        return;
    }

    const auto PBUFFERRES = CWLBufferResource::fromResource(buffer);
    if UNLIKELY (!PBUFFERRES || !PBUFFERRES->m_buffer) {
        LOGM(Log::ERR, "Invalid buffer in {:x}", (uintptr_t)this);
        m_resource->error(HYPRLAND_WORKSPACE_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer");
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
                m_resource->sendFlags(sc<hyprlandWorkspaceExportFrameV1Flags>(0));
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
        case ERROR_NO_BUFFER: m_resource->error(HYPRLAND_WORKSPACE_EXPORT_FRAME_V1_ERROR_INVALID_BUFFER, "invalid buffer"); break;
        case ERROR_BUFFER_SIZE:
        case ERROR_BUFFER_FORMAT: m_resource->sendFailed(); break;
        case ERROR_UNKNOWN:
        case ERROR_STOPPED: m_resource->sendFailed(); break;
    }
}

CWorkspaceExportProtocol::CWorkspaceExportProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver,name) {
    ;
}

void CWorkspaceExportProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto CLIENT = m_clients.emplace_back(makeShared<CWorkspaceExportClient>(makeShared<CHyprlandWorkspaceExportManagerV1>(client, ver, id)));

    if (!CLIENT->good()) {
        LOGM(Log::DEBUG, "Failed to bind client! (out of memory)");
        wl_client_post_no_memory(client);
        m_clients.pop_back();
        return;
    }

    CLIENT->m_self = CLIENT;
    LOGM(Log::DEBUG, "Bound client successfully!");
}

void CWorkspaceExportProtocol::destroyResource(CWorkspaceExportClient* client){
    std::erase_if(m_frames, [&](const auto& other) { return other->m_client.get() == client; });
    std::erase_if(m_clients, [&](const auto& other) { return other.get() == client; });
}

void CWorkspaceExportProtocol::destroyResource(CWorkspaceExportFrame* frame) {
    std::erase_if(m_frames, [&](const auto& other) { return other.get() == frame; });
}




