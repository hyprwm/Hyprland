#include "ImageCaptureSource.hpp"
#include "core/Output.hpp"
#include "../helpers/Monitor.hpp"
#include "../desktop/view/Window.hpp"
#include "ForeignToplevel.hpp"

CImageCaptureSource::CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource, PHLMONITOR pMonitor) : m_resource(resource), m_monitor(pMonitor) {
    m_resource->setData(this);
    m_resource->setDestroy([this](CExtImageCaptureSourceV1* pMgr) { PROTO::imageCaptureSource->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtImageCaptureSourceV1* pMgr) { PROTO::imageCaptureSource->destroyResource(this); });
}

CImageCaptureSource::CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource, PHLWINDOW pWindow) : m_resource(resource), m_window(pWindow) {
    m_resource->setData(this);
    m_resource->setDestroy([this](CExtImageCaptureSourceV1* pMgr) { PROTO::imageCaptureSource->destroyResource(this); });
    m_resource->setOnDestroy([this](CExtImageCaptureSourceV1* pMgr) { PROTO::imageCaptureSource->destroyResource(this); });
}

std::string CImageCaptureSource::getName() {
    if (!m_monitor.expired())
        return m_monitor->m_name;
    if (!m_window.expired())
        return m_window->m_title;

    return "error";
}

std::string CImageCaptureSource::getTypeName() {
    if (!m_monitor.expired())
        return "monitor";
    if (!m_window.expired())
        return "window";

    return "error";
}

COutputImageCaptureSourceProtocol::COutputImageCaptureSourceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void COutputImageCaptureSourceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = PROTO::imageCaptureSource->m_outputManagers.emplace_back(makeShared<CExtOutputImageCaptureSourceManagerV1>(client, ver, id));

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        PROTO::imageCaptureSource->m_outputManagers.pop_back();
        return;
    }

    RESOURCE->setDestroy([](CExtOutputImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setOnDestroy([](CExtOutputImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setCreateSource([](CExtOutputImageCaptureSourceManagerV1* pMgr, uint32_t id, wl_resource* output) {
        PHLMONITOR pMonitor = CWLOutputResource::fromResource(output)->m_monitor.lock();
        if (!pMonitor) {
            LOGM(Log::ERR, "Client tried to create source from invalid output resource");
            pMgr->error(-1, "invalid output resource");
            return;
        }

        auto PSOURCE =
            PROTO::imageCaptureSource->m_sources.emplace_back(makeShared<CImageCaptureSource>(makeShared<CExtImageCaptureSourceV1>(pMgr->client(), pMgr->version(), id), pMonitor));
        PSOURCE->m_self = PSOURCE;

        LOGM(Log::INFO, "New capture source for monitor: {}", pMonitor->m_name);
    });
}

CToplevelImageCaptureSourceProtocol::CToplevelImageCaptureSourceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CToplevelImageCaptureSourceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = PROTO::imageCaptureSource->m_toplevelManagers.emplace_back(makeShared<CExtForeignToplevelImageCaptureSourceManagerV1>(client, ver, id));

    if UNLIKELY (!RESOURCE->resource()) {
        RESOURCE->noMemory();
        PROTO::imageCaptureSource->m_toplevelManagers.pop_back();
        return;
    }

    RESOURCE->setDestroy([](CExtForeignToplevelImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setOnDestroy([](CExtForeignToplevelImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setCreateSource([](CExtForeignToplevelImageCaptureSourceManagerV1* pMgr, uint32_t id, wl_resource* handle) {
        PHLWINDOW pWindow = PROTO::foreignToplevel->windowFromHandleResource(handle);
        if (!pWindow) {
            LOGM(Log::ERR, "Client tried to create source from invalid foreign toplevel handle resource");
            pMgr->error(-1, "invalid foreign toplevel resource");
            return;
        }

        auto PSOURCE =
            PROTO::imageCaptureSource->m_sources.emplace_back(makeShared<CImageCaptureSource>(makeShared<CExtImageCaptureSourceV1>(pMgr->client(), pMgr->version(), id), pWindow));
        PSOURCE->m_self = PSOURCE;

        LOGM(Log::INFO, "New capture source for foreign toplevel: {}", pWindow->m_title);
    });
}

CImageCaptureSourceProtocol::CImageCaptureSourceProtocol() {
    m_output   = makeUnique<COutputImageCaptureSourceProtocol>(&ext_output_image_capture_source_manager_v1_interface, 1, "OutputImageCaptureSource");
    m_toplevel = makeUnique<CToplevelImageCaptureSourceProtocol>(&ext_foreign_toplevel_image_capture_source_manager_v1_interface, 1, "ForeignToplevelImageCaptureSource");
}

SP<CImageCaptureSource> CImageCaptureSourceProtocol::sourceFromResource(wl_resource* res) {
    auto data = sc<CImageCaptureSource*>(sc<CExtImageCaptureSourceV1*>(wl_resource_get_user_data(res))->data());
    return data && data->m_self ? data->m_self.lock() : nullptr;
}

void CImageCaptureSourceProtocol::destroyResource(CExtOutputImageCaptureSourceManagerV1* resource) {
    std::erase_if(m_outputManagers, [&](const auto& other) { return other.get() == resource; });
}
void CImageCaptureSourceProtocol::destroyResource(CExtForeignToplevelImageCaptureSourceManagerV1* resource) {
    std::erase_if(m_toplevelManagers, [&](const auto& other) { return other.get() == resource; });
}
void CImageCaptureSourceProtocol::destroyResource(CImageCaptureSource* resource) {
    std::erase_if(m_sources, [&](const auto& other) { return other.get() == resource; });
}
