#include "XDGForeign.hpp"
#include "protocols/XDGShell.hpp"
#include "xdg-foreign-unstable-v2.hpp"
#include <algorithm>
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <string_view>
#include <unordered_map>
#include <uuid.h>
#include <wayland-server.h>
#include "core/Compositor.hpp"

CXDGExportedResource::CXDGExportedResource(SP<CZxdgExportedV2> resource_, SP<CXDGToplevelResource> toplevel, const std::string& handle) :
    m_resource(resource_), m_toplevel(toplevel), m_handle(handle) {

    if UNLIKELY (!good())
        return;

    m_resource->sendHandle(handle.c_str());
    m_xdgDestroyListener = toplevel->m_events.destroy.listen([this] { PROTO::xdgForeignExporter->destroyExported(this); });
    m_resource->setOnDestroy([this](CZxdgExportedV2* r) { PROTO::xdgForeignExporter->destroyExported(this); });
    m_resource->setDestroy([this](CZxdgExportedV2* r) { PROTO::xdgForeignExporter->destroyExported(this); });
}

bool CXDGExportedResource::good() {
    return m_resource->resource();
}

WP<CXDGToplevelResource> CXDGExportedResource::xdgSurf() {
    return m_toplevel;
}

std::string_view CXDGExportedResource::handle() {
    return m_handle;
}

CXDGForeignExporterProtocol::CXDGForeignExporterProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {}

void CXDGForeignExporterProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_exporters.emplace_back(makeUnique<CZxdgExporterV2>(client, ver, id)).get();

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        return;
    }

    RESOURCE->setExportToplevel([this, RESOURCE](CZxdgExporterV2* pMgr, uint32_t id, wl_resource* surface) {
        std::array<char, 37> uuidStr;
        uuid_t               uuid;
        auto                 wlSurf = CWLSurfaceResource::fromResource(surface);

        if (wlSurf->m_role != SURFACE_ROLE_XDG_SHELL) {
            RESOURCE->error(zxdgExporterV2Error::ZXDG_EXPORTER_V2_ERROR_INVALID_SURFACE, "surface must be an xdg_toplevel");
            return;
        }

        auto xdgSurfResource = sc<CXDGSurfaceRole*>(wlSurf->m_role.get())->m_xdgSurface.lock();
        if (xdgSurfResource->m_toplevel.expired())
            return; // ??

        auto xdgSurf = xdgSurfResource->m_toplevel.lock();
        uuid_generate_random(uuid);
        uuid_unparse_lower(uuid, uuidStr.data());
        std::string handle = std::string{std::begin(uuidStr), std::end(uuidStr)};

        //auto [r, t] = this->m.emplace(handle, makeShared<CZxdgExportedV2>(pMgr->client(), 2, id));
        auto [r, t] = this->mv2.emplace(handle, makeUnique<CXDGExportedResource>(makeShared<CZxdgExportedV2>(pMgr->client(), RESOURCE->version(), id), xdgSurf, handle));

        if (!t) {
            wl_client_post_no_memory(pMgr->client());
            return;
        }
    });

    RESOURCE->setDestroy([this](CZxdgExporterV2* e) { onExporterDestroyed(e); });
    RESOURCE->setOnDestroy([this](CZxdgExporterV2* e) { onExporterDestroyed(e); });
}

void CXDGForeignExporterProtocol::onExporterDestroyed(CZxdgExporterV2* exporter) {
    std::erase_if(m_exporters, [exporter](const auto& other) { return other->resource() == exporter->resource(); });
}

void CXDGForeignExporterProtocol::destroyExported(CXDGExportedResource* r) {
    r->m_events.destroy.emit();
    PROTO::xdgForeignExporter->mv2.erase(r->m_handle);
}

CXDGImportedResource::CXDGImportedResource(SP<CZxdgImportedV2> imported, const std::string& handle) : m_resource(imported), m_handle(handle) {
    if UNLIKELY (!m_resource->resource())
        return;

    auto& exported = PROTO::xdgForeignExporter->mv2.at(handle);
    m_resource->setSetParentOf([this](CZxdgImportedV2* r, wl_resource* surf) {
        const auto wlSurf = CWLSurfaceResource::fromResource(surf);

        if (wlSurf->m_role != SURFACE_ROLE_XDG_SHELL) {
            m_resource->error(zxdgExporterV2Error::ZXDG_EXPORTER_V2_ERROR_INVALID_SURFACE, "surface must be an xdg_toplevel");
            return;
        }

        const auto xdgSurfResource = sc<CXDGSurfaceRole*>(wlSurf->m_role.get())->m_xdgSurface.lock();
        if (xdgSurfResource->m_toplevel.expired())
            return; // ??

        const auto xdgSurf  = xdgSurfResource->m_toplevel.lock();
        auto&      exported = PROTO::xdgForeignExporter->mv2.at(this->m_handle);
        exported->xdgSurf()->m_children.emplace_back(xdgSurf);
    });

    auto handleDestroy = [this]() {
        this->m_resource->sendDestroyed();
        PROTO::xdgForeignImporter->destoryImported(this);
    };

    m_exportDestoryListener = exported->m_events.destroy.listen([handleDestroy]() { handleDestroy(); });
    m_resource->setDestroy([handleDestroy](CZxdgImportedV2*) { handleDestroy(); });
    m_resource->setOnDestroy([handleDestroy](CZxdgImportedV2*) { handleDestroy(); });
}

CXDGForeignImporterProtocol::CXDGForeignImporterProtocol(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGForeignImporterProtocol::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_importers.emplace_back(makeUnique<CZxdgImporterV2>(client, ver, id)).get();

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        return;
    }

    RESOURCE->setImportToplevel([RESOURCE, this](CZxdgImporterV2* pMgr, uint32_t id, const char* handle) {
        if (!PROTO::xdgForeignExporter->mv2.contains(handle))
            return;

        auto [r, t] = this->m_imports.emplace(handle, makeShared<CXDGImportedResource>(makeShared<CZxdgImportedV2>(pMgr->client(), RESOURCE->version(), id), handle));
        if (!t) {
            wl_client_post_no_memory(pMgr->client());
            return;
        }
    });

    RESOURCE->setDestroy([this](CZxdgImporterV2* r) { onImporterDestroyed(r); });
    RESOURCE->setOnDestroy([this](CZxdgImporterV2* r) { onImporterDestroyed(r); });
}

void CXDGForeignImporterProtocol::onImporterDestroyed(CZxdgImporterV2* importer) {
    std::erase_if(m_importers, [importer](const auto& other) { return other->resource() == importer->resource(); });
}

void CXDGForeignImporterProtocol::destoryImported(CXDGImportedResource* r) {
    PROTO::xdgForeignImporter->m_imports.erase(r->m_handle);
}