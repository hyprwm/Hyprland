#include "ImageCaptureSource.hpp"
#include "ForeignToplevel.hpp"
#include "desktop/Window.hpp"
#include "helpers/Monitor.hpp"
#include "core/Output.hpp"

CImageCaptureSource::CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource_, SP<CMonitor> pMonitor_) : resource(resource_), pMonitor(pMonitor_) {
    listeners.destroy1 = pMonitor->events.disconnect.registerListener([this](std::any data) { PROTO::imageCaptureSource->destroyResource(this); });
    listeners.destroy2 = pMonitor->events.destroy.registerListener([this](std::any data) { PROTO::imageCaptureSource->destroyResource(this); });
}

CImageCaptureSource::CImageCaptureSource(SP<CExtImageCaptureSourceV1> resource_, SP<CWindow> pWindow_) : resource(resource_), pWindow(pWindow_) {
    listeners.destroy1 = pWindow->events.destroy.registerListener([this](std::any data) { PROTO::imageCaptureSource->destroyResource(this); });
    listeners.destroy2 = pWindow->events.unmap.registerListener([this](std::any data) { PROTO::imageCaptureSource->destroyResource(this); });
    listeners.destroy3 = pWindow->events.hide.registerListener([this](std::any data) { PROTO::imageCaptureSource->destroyResource(this); });
}

CImageCaptureSource::~CImageCaptureSource() {
    events.destroy.emit();
}

wl_resource* CImageCaptureSource::res() {
    return resource->resource();
}

COutputImageCaptureSourceProtocol::COutputImageCaptureSourceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void COutputImageCaptureSourceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = PROTO::imageCaptureSource->m_vOutputManagers.emplace_back(makeShared<CExtOutputImageCaptureSourceManagerV1>(client, ver, id));

    if (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        PROTO::imageCaptureSource->m_vOutputManagers.pop_back();
        return;
    }

    RESOURCE->setDestroy([](CExtOutputImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setOnDestroy([](CExtOutputImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setCreateSource([](CExtOutputImageCaptureSourceManagerV1* pMgr, uint32_t id, wl_resource* output) {
        SP<CMonitor> pMonitor = CWLOutputResource::fromResource(output)->monitor.lock();
        if (!pMonitor) {
            LOGM(LOG, "Client tried to create source from invalid output resource");
            PROTO::imageCaptureSource->destroyResource(pMgr);
            return;
        }

        PROTO::imageCaptureSource->m_vSources.emplace_back(makeShared<CImageCaptureSource>(makeShared<CExtImageCaptureSourceV1>(pMgr->client(), pMgr->version(), id), pMonitor));
        LOGM(LOG, "New capture source for monitor: {}", pMonitor->szName);
    });
}

CToplevelImageCaptureSourceProtocol::CToplevelImageCaptureSourceProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CToplevelImageCaptureSourceProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = PROTO::imageCaptureSource->m_vToplevelManagers.emplace_back(makeShared<CExtForeignToplevelImageCaptureSourceManagerV1>(client, ver, id));

    if (!RESOURCE->resource()) {
        RESOURCE->noMemory();
        PROTO::imageCaptureSource->m_vToplevelManagers.pop_back();
        return;
    }

    RESOURCE->setDestroy([](CExtForeignToplevelImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setOnDestroy([](CExtForeignToplevelImageCaptureSourceManagerV1* pMgr) { PROTO::imageCaptureSource->destroyResource(pMgr); });
    RESOURCE->setCreateSource([](CExtForeignToplevelImageCaptureSourceManagerV1* pMgr, uint32_t id, wl_resource* handle) {
        PHLWINDOW window = PROTO::foreignToplevel->windowFromHandleResource(handle);
        if (!window) {
            LOGM(LOG, "Client tried to create source from invalid foreign toplevel handle resource");
            PROTO::imageCaptureSource->destroyResource(pMgr);
            return;
        }

        PROTO::imageCaptureSource->m_vSources.emplace_back(makeShared<CImageCaptureSource>(makeShared<CExtImageCaptureSourceV1>(pMgr->client(), pMgr->version(), id), window));
        LOGM(LOG, "New capture source for foreign toplevel: {}", window->m_szTitle);
    });
}

CImageCaptureSourceProtocol::CImageCaptureSourceProtocol() {
    output   = std::make_unique<COutputImageCaptureSourceProtocol>(&ext_output_image_capture_source_manager_v1_interface, 1, "OutputImageCaptureSource");
    toplevel = std::make_unique<CToplevelImageCaptureSourceProtocol>(&ext_foreign_toplevel_image_capture_source_manager_v1_interface, 1, "ForeignToplevelImageCaptureSource");
}

SP<CImageCaptureSource> CImageCaptureSourceProtocol::sourceFromResource(wl_resource* resource) {
    for (const auto& s : m_vSources) {
        if (s->res() != resource)
            continue;

        return s;
    }

    return nullptr;
}

void CImageCaptureSourceProtocol::destroyResource(CExtOutputImageCaptureSourceManagerV1* resource) {
    std::erase_if(m_vOutputManagers, [&](const auto& other) { return other.get() == resource; });
}
void CImageCaptureSourceProtocol::destroyResource(CExtForeignToplevelImageCaptureSourceManagerV1* resource) {
    std::erase_if(m_vToplevelManagers, [&](const auto& other) { return other.get() == resource; });
}
void CImageCaptureSourceProtocol::destroyResource(CImageCaptureSource* resource) {
    std::erase_if(m_vSources, [&](const auto& other) { return other.get() == resource; });
}
