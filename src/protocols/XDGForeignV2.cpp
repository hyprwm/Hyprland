#include "protocols/XDGForeignV2.hpp"
#include "managers/TokenManager.hpp"
#include "protocols/XDGShell.hpp"
#include "xdg-foreign-unstable-v2.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <wayland-server.h>
#include "protocols/core/Compositor.hpp"

CXDGExportedResourceV2::CXDGExportedResourceV2(SP<CZxdgExportedV2> resource, SP<CXDGToplevelResource> toplevel, const std::string& handle) :
    m_resource(resource), m_toplevel(toplevel), m_handle(handle) {

    if UNLIKELY (!good())
        return;

    m_resource->setData(this);
    m_resource->sendHandle(handle.c_str());
    m_listeners.topLevelDestroyed = toplevel->m_events.destroy.listen([this] {
        m_topLevelDestroyed = true;
        m_listeners.topLevelDestroyed.reset();
        m_events.destroy.emit();
    });
    m_resource->setOnDestroy([this](CZxdgExportedV2*) { PROTO::xdgForeignExporter->destroyExported(this); });
    m_resource->setDestroy([this](CZxdgExportedV2*) { PROTO::xdgForeignExporter->destroyExported(this); });
}

CXDGExportedResourceV2::~CXDGExportedResourceV2() {
    if (!m_topLevelDestroyed)
        m_events.destroy.emit();
}

bool CXDGExportedResourceV2::good() const {
    return m_resource->resource();
}

WP<CXDGToplevelResource> CXDGExportedResourceV2::xdgSurf() const {
    return m_toplevel;
}

std::string_view CXDGExportedResourceV2::handle() const {
    return m_handle;
}

CXDGForeignExporterProtocolV2::CXDGForeignExporterProtocolV2(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {}

void CXDGForeignExporterProtocolV2::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_exporters.emplace_back(makeUnique<CZxdgExporterV2>(client, ver, id)).get();

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        m_exporters.pop_back();
        return;
    }

    RESOURCE->setExportToplevel([this](CZxdgExporterV2* exporter, uint32_t id, wl_resource* surface) {
        auto wlSurf = CWLSurfaceResource::fromResource(surface);

        if (wlSurf->m_role != SURFACE_ROLE_XDG_SHELL) {
            exporter->error(zxdgExporterV2Error::ZXDG_EXPORTER_V2_ERROR_INVALID_SURFACE, "surface must be an xdg_toplevel");
            return;
        }

        auto xdgSurfResource = sc<CXDGSurfaceRole*>(wlSurf->m_role.get())->m_xdgSurface.lock();
        if (xdgSurfResource->m_toplevel.expired())
            return;

        auto              xdgSurf = xdgSurfResource->m_toplevel.lock();
        const std::string HANDLE  = g_pTokenManager->getRandomUUID();
        const auto [ELM, EMPLACED] =
            this->m_exported.emplace(HANDLE, makeShared<CXDGExportedResourceV2>(makeShared<CZxdgExportedV2>(exporter->client(), exporter->version(), id), xdgSurf, HANDLE));

        // This should only happen if we have our generated handles collide.
        if UNLIKELY (!EMPLACED) {
            wl_client_post_no_memory(exporter->client());
            return;
        }

        if UNLIKELY (!ELM->second->good()) {
            wl_client_post_no_memory(exporter->client());
            destroyExported(ELM->second.get());
        }
    });

    RESOURCE->setDestroy([this](CZxdgExporterV2* e) { onExporterDestroyed(e); });
    RESOURCE->setOnDestroy([this](CZxdgExporterV2* e) { onExporterDestroyed(e); });
}

SP<CXDGExportedResourceV2> CXDGForeignExporterProtocolV2::getExported(const std::string& handle) const {
    return m_exported.contains(handle) ? m_exported.at(handle) : nullptr;
}

void CXDGForeignExporterProtocolV2::onExporterDestroyed(CZxdgExporterV2* exporter) {
    std::erase_if(m_exporters, [exporter](const auto& other) { return exporter == other.get(); });
}

void CXDGForeignExporterProtocolV2::destroyExported(CXDGExportedResourceV2* r) {
    PROTO::xdgForeignExporter->m_exported.erase(r->m_handle);
}

CXDGImportedResourceV2::CXDGImportedResourceV2(SP<CZxdgImportedV2> imported, SP<CXDGExportedResourceV2> exported, const std::string& handle) :
    m_resource(imported), m_exported(exported), m_handle(handle) {
    if UNLIKELY (!m_resource->resource() || m_exported.expired())
        return;

    m_resource->setData(this);

    m_resource->setSetParentOf([this](CZxdgImportedV2* r, wl_resource* surf) {
        const auto CHILDSURF = CWLSurfaceResource::fromResource(surf);

        if (CHILDSURF->m_role != SURFACE_ROLE_XDG_SHELL) {
            m_resource->error(zxdgImportedV2Error::ZXDG_IMPORTED_V2_ERROR_INVALID_SURFACE, "surface must be an xdg_toplevel");
            return;
        }

        const auto CHILDXDGSURF = sc<CXDGSurfaceRole*>(CHILDSURF->m_role.get())->m_xdgSurface.lock();
        if (CHILDXDGSURF->m_toplevel.expired())
            return;

        if LIKELY (auto exportedTopLevel = m_exported->xdgSurf(); !exportedTopLevel.expired())
            CHILDXDGSURF->m_toplevel->setNewParent(exportedTopLevel.lock());
    });

    m_listeners.exportedDestroyed = m_exported->m_events.destroy.listen([this]() { PROTO::xdgForeignImporter->destroyImported(this); });
    m_resource->setDestroy([this](CZxdgImportedV2*) { PROTO::xdgForeignImporter->destroyImported(this); });
    m_resource->setOnDestroy([this](CZxdgImportedV2*) { PROTO::xdgForeignImporter->destroyImported(this); });
}

CXDGImportedResourceV2::~CXDGImportedResourceV2() {
    m_resource->sendDestroyed();
}

CXDGForeignImporterProtocolV2::CXDGForeignImporterProtocolV2(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {
    ;
}

void CXDGForeignImporterProtocolV2::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_importers.emplace_back(makeUnique<CZxdgImporterV2>(client, ver, id)).get();

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        m_importers.pop_back();
        return;
    }

    RESOURCE->setImportToplevel([this](CZxdgImporterV2* importer, uint32_t id, const char* _handle) {
        const std::string HANDLE   = _handle;
        auto              exported = PROTO::xdgForeignExporter->getExported(HANDLE);
        auto              imported =
            m_imports.emplace_back(makeUnique<CXDGImportedResourceV2>(makeShared<CZxdgImportedV2>(importer->client(), importer->version(), id), exported, HANDLE)).get();

        if UNLIKELY (!imported->m_resource->resource()) {
            wl_client_post_no_memory(importer->client());
            m_imports.pop_back();
            return;
        }

        // Couldn't find the handle.
        if UNLIKELY (imported->m_exported.expired())
            destroyImported(imported);
    });

    RESOURCE->setDestroy([this](CZxdgImporterV2* r) { onImporterDestroyed(r); });
    RESOURCE->setOnDestroy([this](CZxdgImporterV2* r) { onImporterDestroyed(r); });
}

void CXDGForeignImporterProtocolV2::onImporterDestroyed(CZxdgImporterV2* importer) {
    std::erase_if(m_importers, [importer](const auto& other) { return importer == other.get(); });
}

void CXDGForeignImporterProtocolV2::destroyImported(CXDGImportedResourceV2* imported) {
    std::erase_if(m_imports, [imported](const auto& other) { return imported == other.get(); });
}