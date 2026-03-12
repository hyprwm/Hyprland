#include "protocols/XDGForeignV2.hpp"
#include "protocols/XDGShell.hpp"
#include "xdg-foreign-unstable-v2.hpp"
#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <uuid.h>
#include <wayland-server.h>
#include "protocols/core/Compositor.hpp"

CXDGExportedResourceV2::CXDGExportedResourceV2(SP<CZxdgExportedV2> resource, SP<CXDGToplevelResource> toplevel, const std::string& handle) :
    m_resource(resource), m_toplevel(toplevel), m_handle(handle) {

    if UNLIKELY (!good())
        return;

    m_resource->setData(this);
    m_resource->sendHandle(handle.c_str());
    m_listeners.topLevelDestroyed = toplevel->m_events.destroy.listen([this] { PROTO::xdgForeignExporter->destroyExported(this); });
    m_resource->setOnDestroy([this](CZxdgExportedV2*) { PROTO::xdgForeignExporter->destroyExported(this); });
    m_resource->setDestroy([this](CZxdgExportedV2*) { PROTO::xdgForeignExporter->destroyExported(this); });
}

CXDGExportedResourceV2::~CXDGExportedResourceV2() {
    m_events.destroy.emit();
}

bool CXDGExportedResourceV2::good() {
    return m_resource->resource();
}

WP<CXDGToplevelResource> CXDGExportedResourceV2::xdgSurf() {
    return m_toplevel;
}

std::string_view CXDGExportedResourceV2::handle() {
    return m_handle;
}

CXDGForeignExporterProtocolV2::CXDGForeignExporterProtocolV2(const wl_interface* iface, const int& ver, const std::string& name) : IWaylandProtocol(iface, ver, name) {}

void CXDGForeignExporterProtocolV2::bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id) {
    const auto RESOURCE = m_exporters.emplace_back(makeUnique<CZxdgExporterV2>(client, ver, id)).get();

    if UNLIKELY (!RESOURCE->resource()) {
        wl_client_post_no_memory(client);
        return;
    }

    RESOURCE->setExportToplevel([this, RESOURCE](CZxdgExporterV2* exporter, uint32_t id, wl_resource* surface) {
        std::array<char, 37> uuidStr;
        uuid_t               uuid;
        auto                 wlSurf = CWLSurfaceResource::fromResource(surface);

        if (wlSurf->m_role != SURFACE_ROLE_XDG_SHELL) {
            RESOURCE->error(zxdgExporterV2Error::ZXDG_EXPORTER_V2_ERROR_INVALID_SURFACE, "surface must be an xdg_toplevel");
            return;
        }

        auto xdgSurfResource = sc<CXDGSurfaceRole*>(wlSurf->m_role.get())->m_xdgSurface.lock();
        if (xdgSurfResource->m_toplevel.expired())
            return;

        auto xdgSurf = xdgSurfResource->m_toplevel.lock();
        uuid_generate_random(uuid);
        uuid_unparse_lower(uuid, uuidStr.data());
        const std::string handle = std::string{std::begin(uuidStr), std::end(uuidStr)};

        const auto [ELM, EMPLACED] =
            this->m_exported.emplace(handle, makeShared<CXDGExportedResourceV2>(makeShared<CZxdgExportedV2>(exporter->client(), RESOURCE->version(), id), xdgSurf, handle));

        // This would only happen if we have handle collision
        if UNLIKELY (!EMPLACED)
            wl_client_post_no_memory(exporter->client());
    });

    RESOURCE->setDestroy([this](CZxdgExporterV2* e) { onExporterDestroyed(e); });
    RESOURCE->setOnDestroy([this](CZxdgExporterV2* e) { onExporterDestroyed(e); });
}

SP<CXDGExportedResourceV2> CXDGForeignExporterProtocolV2::getExported(const std::string& handle) {
    if (!m_exported.contains(handle))
        return nullptr;
    return m_exported.at(handle);
}

void CXDGForeignExporterProtocolV2::onExporterDestroyed(CZxdgExporterV2* exporter) {
    std::erase_if(m_exporters, [exporter](const auto& other) { return other->resource() == exporter->resource(); });
}

void CXDGForeignExporterProtocolV2::destroyExported(CXDGExportedResourceV2* r) {
    PROTO::xdgForeignExporter->m_exported.erase(r->m_handle);
}

CXDGImportedResourceV2::CXDGImportedResourceV2(SP<CZxdgImportedV2> imported, SP<CXDGExportedResourceV2> exported, const std::string& handle) :
    m_resource(imported), m_exported(exported), m_handle(handle) {
    if UNLIKELY (!m_resource->resource())
        return;

    m_resource->setData(this);

    m_resource->setSetParentOf([this](CZxdgImportedV2* r, wl_resource* surf) {
        const auto CHILDSURF = CWLSurfaceResource::fromResource(surf);

        if (CHILDSURF->m_role != SURFACE_ROLE_XDG_SHELL) {
            m_resource->error(zxdgExporterV2Error::ZXDG_EXPORTER_V2_ERROR_INVALID_SURFACE, "surface must be an xdg_toplevel");
            return;
        }

        const auto CHILDXDGSURF = sc<CXDGSurfaceRole*>(CHILDSURF->m_role.get())->m_xdgSurface.lock();
        if (!CHILDXDGSURF->m_toplevel)
            return;

        auto childTopLevel    = CHILDXDGSURF->m_toplevel.lock();
        auto exportedTopLevel = PROTO::xdgForeignExporter->getExported(this->m_handle)->xdgSurf().lock();
        childTopLevel->setNewParent(exportedTopLevel);
    });

    auto handleDestroy            = [this]() { PROTO::xdgForeignImporter->destroyImported(this); };
    m_listeners.exportedDestoryed = m_exported->m_events.destroy.listen([handleDestroy]() { handleDestroy(); });
    m_resource->setDestroy([handleDestroy](CZxdgImportedV2*) { handleDestroy(); });
    m_resource->setOnDestroy([handleDestroy](CZxdgImportedV2*) { handleDestroy(); });
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
        return;
    }

    RESOURCE->setImportToplevel([RESOURCE, this](CZxdgImporterV2* importer, uint32_t id, const char* handle) {
        auto exported = PROTO::xdgForeignExporter->getExported(handle);
        if (!exported)
            return;

        auto imported = m_imports.emplace_back(makeShared<CXDGImportedResourceV2>(makeShared<CZxdgImportedV2>(importer->client(), RESOURCE->version(), id), exported, handle));
        if UNLIKELY (!imported->m_resource->resource())
            wl_client_post_no_memory(importer->client());
    });

    RESOURCE->setDestroy([this](CZxdgImporterV2* r) { onImporterDestroyed(r); });
    RESOURCE->setOnDestroy([this](CZxdgImporterV2* r) { onImporterDestroyed(r); });
}

void CXDGForeignImporterProtocolV2::onImporterDestroyed(CZxdgImporterV2* importer) {
    std::erase_if(m_importers, [importer](const auto& other) { return other->resource() == importer->resource(); });
}

void CXDGForeignImporterProtocolV2::destroyImported(CXDGImportedResourceV2* r) {
    std::erase_if(m_imports, [r](const auto& other) { return other->m_resource->resource() == r->m_resource->resource(); });
}